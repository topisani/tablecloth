#include "view.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"

#include "desktop.hpp"
#include "layers.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "wlroots.hpp"
#include "xcursor.hpp"
#include "workspace.hpp"

#include "wlr-layer-shell-unstable-v1-protocol.h"

namespace cloth {

  View::View(Workspace& workspace) : workspace(&workspace), desktop(workspace.desktop) {
    update_decorated(true);
  }

  View::~View() noexcept
  {
    events.destroy.emit();
    if (wlr_surface) unmap();
  }

  ViewType View::type() noexcept {
    if (util::dynamic_is<XdgSurfaceV6>(this)) return ViewType::xdg_shell_v6;
    if (util::dynamic_is<XdgSurface>(this)) return ViewType::xdg_shell;
    if (util::dynamic_is<WlShellSurface>(this)) return ViewType::wl_shell;
    #ifdef WLR_HAS_XWAYLAND
    if (util::dynamic_is<XwaylandSurface>(this)) return ViewType::xwayland;
    #endif
    assert(false);
  }

  bool View::is_focused() {
    return workspace->focused_view() == this;
  }

  wlr::box_t View::get_box() const
  {
    return {.x = (int) x, .y = (int) y, .width = (int) width, .height = (int) height};
  }

  wlr::box_t View::get_deco_box() const
  {
    auto box = get_box();
    if (decorated) {
      box.x -= border_width;
      box.y -= (border_width + titlebar_height);
      box.width += border_width * 2;
      box.height += (border_width * 2 + titlebar_height);
    }
    return box;
  }

  DecoPart View::get_deco_part(double sx, double sy)
  {
    if (!decorated) {
      return DecoPart::none;
    }

    int sw = wlr_surface->current.width;
    int sh = wlr_surface->current.height;
    int bw = border_width;
    int titlebar_h = titlebar_height;

    if (sx > 0 && sx < sw && sy < 0 && sy > -titlebar_height) {
      return DecoPart::titlebar;
    }

    DecoPart parts = DecoPart::none;
    if (sy >= -(titlebar_h + bw) && sy <= sh + bw) {
      if (sx < 0 && sx > -bw) {
        parts |= DecoPart::left_border;
      } else if (sx > sw && sx < sw + bw) {
        parts |= DecoPart::right_border;
      }
    }

    if (sx >= -bw && sx <= sw + bw) {
      if (sy > sh && sy <= sh + bw) {
        parts |= DecoPart::bottom_border;
      } else if (sy >= -(titlebar_h + bw) && sy < 0) {
        parts |= DecoPart::top_border;
      }
    }
    // TODO corners
    return parts;
  }

  void View::update_output(std::optional<wlr::box_t> before) const
  {
    if (wlr_surface == nullptr) return;

    auto box = get_box();

    for (auto& output : desktop.outputs) {
      bool intersected = before.has_value() && wlr_output_layout_intersects(desktop.layout, &output.wlr_output, &before.value());
      bool intersects = wlr_output_layout_intersects(desktop.layout, &output.wlr_output, &box);
      if (intersected && !intersects) {
        wlr_surface_send_leave(wlr_surface, &output.wlr_output);
      }
      if (!intersected && intersects) {
        wlr_surface_send_enter(wlr_surface, &output.wlr_output);
      }
    }
  }

  void View::do_move(double x, double y) {
      update_position(x, y);
  }

  void View::move(double x, double y)
  {
    if (this->x == x && this->y == y) {
      return;
    }

    auto before = get_box();
    do_move(x, y);
    update_output(before);
  }

  void View::activate(bool activate)
  {
    active = activate;
    do_activate(activate);
    if (decorated) damage_whole();
  }

  void View::resize(int width, int height)
  {
    auto before = get_box();
    do_resize(width, height);
    update_output(before);
  }

  void View::do_move_resize(double x, double y, int width, int height)
  {
      pending_move_resize.update_x = x != this->x;
      pending_move_resize.update_y = y != this->y;
      pending_move_resize.x = x;
      pending_move_resize.y = y;
      pending_move_resize.width = width;
      pending_move_resize.height = height;
  }

  void View::move_resize(double x, double y, int width, int height)
  {
    bool update_x = x != this->x;
    bool update_y = y != this->y;
    if (update_x || update_y) {
        do_move_resize(x, y, width, height);
    }
    resize(width, height);
  }

  void View::move_resize(wlr::box_t box)
  {
    move_resize(box.x, box.y, box.width, box.height);
  }

  wlr::output_t* View::get_output()
  {
    auto view_box = get_box();
    double output_x, output_y;
    wlr_output_layout_closest_point(desktop.layout, nullptr, x + (double) view_box.width / 2,
                                    y + (double) view_box.height / 2, &output_x, &output_y);
    return wlr_output_layout_output_at(desktop.layout, output_x, output_y);
  }

  void View::arrange_maximized()
  {
    auto* wlr_output = get_output();
    auto& output = *(Output*) wlr_output->data;

    auto* output_box = wlr_output_layout_get_box(desktop.layout, wlr_output);
    auto usable_area = output.usable_area;

    usable_area.x += output_box->x;
    usable_area.y += output_box->y;

    move_resize(usable_area);
    rotate(0);
  }

  void View::maximize(bool maximized)
  {
    if (this->maximized == maximized) return;

    do_maximize(maximized);

    if (!this->maximized && maximized) {
      this->maximized = true;
      this->saved.x = this->x;
      this->saved.y = this->y;
      this->saved.rotation = this->rotation;
      this->saved.width = this->width;
      this->saved.height = this->height;

      arrange_maximized();
    }

    if (this->maximized && !maximized) {
      this->maximized = false;

      move_resize(saved);
      rotate(saved.rotation);
    }
  }

  void View::set_fullscreen(bool fullscreen, wlr::output_t* wlr_output)
  {
    bool was_fullscreen = this->fullscreen_output != nullptr;
    if (was_fullscreen == fullscreen) {
      // TODO: support changing the output?
      return;
    }

    // TODO: check if client is focused?
    do_set_fullscreen(fullscreen);

    if (!was_fullscreen && fullscreen) {
      if (wlr_output == nullptr) {
        wlr_output = get_output();
      }
      auto* output_ptr = desktop.output_from_wlr_output(wlr_output);
      if (output_ptr == nullptr) {
        return;
      }
      auto& output = *output_ptr;

      auto view_box = get_box();

      this->saved.x = this->x;
      this->saved.y = this->y;
      this->saved.rotation = this->rotation;
      this->saved.width = view_box.width;
      this->saved.height = view_box.height;

      auto output_box = *wlr_output_layout_get_box(desktop.layout, wlr_output);
      move_resize(output_box);
      rotate(0);

      output.workspace->fullscreen_view = this;
      fullscreen_output = &output;
      output.damage_whole();
    }

    if (was_fullscreen && !fullscreen) {
      move_resize(saved);
      rotate(saved.rotation);
      fullscreen_output->damage_whole();

      fullscreen_output->workspace->fullscreen_view = nullptr;
      fullscreen_output = nullptr;
    }
  }

  void View::rotate(float rotation)
  {
    if (this->rotation == rotation) {
      return;
    }

    damage_whole();
    this->rotation = rotation;
    damage_whole();
  }

  void View::cycle_alpha()
  {
    alpha -= 0.05;
    /* Don't go completely transparent */
    if (alpha < 0.1) {
      alpha = 1.0;
    }
    damage_whole();
  }

  void View::close()
  {
    do_close();
  }

  bool View::center()
  {
    auto box = get_box();

    auto& input = desktop.server.input;
    auto* seat = input.last_active_seat();
    if (!seat) {
      return false;
    }

    auto* wlr_output =
      wlr_output_layout_output_at(desktop.layout, seat->cursor.wlr_cursor->x, seat->cursor.wlr_cursor->y);
    if (!wlr_output) {
      // empty layout
      return false;
    }

    const wlr::output_layout_output_t* l_output = wlr_output_layout_get(desktop.layout, wlr_output);

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);

    double view_x = (double) (width - box.width) / 2 + l_output->x;
    double view_y = (double) (height - box.height) / 2 + l_output->y;
    move(view_x, view_y);

    return true;
  }

  ViewChild::~ViewChild() noexcept
  {}

  void ViewChild::finish()
  {
    auto keep_alive = util::erase_this(view.children, this);
    view.damage_whole();
  }

  void ViewChild::handle_commit(void* data)
  {
    view.apply_damage();
  }

  void ViewChild::handle_new_subsurface(void* data)
  {
    auto& wlr_subsurface = *(wlr::subsurface_t*) data;
    view.create_subsurface(wlr_subsurface);
  }

  ViewChild::ViewChild(View& view, wlr::surface_t* wlr_surface)
    : view(view), wlr_surface(wlr_surface)
  {
    on_commit = [this](void* data) { handle_commit(data); };
    on_commit.add_to(wlr_surface->events.commit);

    on_new_subsurface = [this](void* data) { handle_commit(data); };
    on_new_subsurface.add_to(wlr_surface->events.new_subsurface);
  }

  ViewChild& View::create_child(wlr::surface_t& wlr_surface)
  {
    return children.emplace_back(*this, &wlr_surface);
  }

  Subsurface::Subsurface(View& view, wlr::subsurface_t* wlr_subsurface)
    : ViewChild(view, wlr_subsurface->surface), wlr_subsurface(wlr_subsurface)
  {
    on_destroy = [this](void*) { finish(); };
    on_destroy.add_to(wlr_subsurface->events.destroy);
    view.desktop.server.input.update_cursor_focus();
  }

  Subsurface& View::create_subsurface(wlr::subsurface_t& wlr_subsurface)
  {
    LOGD("New subsurface");
    return static_cast<Subsurface&>(
      children.push_back(std::make_unique<Subsurface>(*this, &wlr_subsurface)));
  }

  void View::map(wlr::surface_t& surface)
  {
    if (wlr_surface) wlr_surface->data = nullptr;
    wlr_surface = &surface;
    wlr_surface->data = this;

    wlr::subsurface_t* subsurface;
    wl_list_for_each(subsurface, &wlr_surface->subsurfaces, parent_link)
    {
      create_subsurface(*subsurface);
    }

    on_new_subsurface = [this] (void* data) {
      auto& subs = *(wlr::subsurface_t*) data;
      create_subsurface(subs);
    };
    on_new_subsurface.add_to(wlr_surface->events.new_subsurface);

    this->mapped = true;
    damage_whole();
    desktop.server.input.update_cursor_focus();
  }

  void View::unmap()
  {
    assert(this->wlr_surface != nullptr);
    this->wlr_surface->data = nullptr;
    this->mapped = false;
    events.unmap.emit(this);
    damage_whole();

    on_new_subsurface.remove();

    for (auto& child : children) {
      child.finish();
    }

    if (fullscreen_output != nullptr) {
      fullscreen_output->damage_whole();
      fullscreen_output->workspace->fullscreen_view = nullptr;
      fullscreen_output = nullptr;
    }

    wlr_surface = nullptr;
    width = height = 0;
  }

  void View::update_decorated(bool decorated)
  {
    //if (this->decorated == decorated) return;

    damage_whole();
    this->decorated = decorated;
    if (decorated) {
      border_width = 4;
      titlebar_height = 12;
    } else {
      border_width = 0;
      titlebar_height = 0;
    }
    damage_whole();
  }

  void View::initial_focus()
  {
    // TODO what seat gets focus? the one with the last input event?
    workspace->set_focused_view(this);
  }

  void View::setup()
  {
    initial_focus();

    if (fullscreen_output == nullptr && !maximized) {
      center();
    }
    update_output();
  }

  void View::apply_damage()
  {
    for (auto& output : desktop.outputs) {
      output.damage_from_view(*this);
    }
  }

  void View::damage_whole()
  {
    for (auto& output : desktop.outputs) {
      output.damage_whole_view(*this);
    }
  }

  void View::update_position(double x, double y)
  {
    if (this->x == x && this->y == y) {
      return;
    }

    damage_whole();
    this->x = x;
    this->y = y;
    damage_whole();
  }

  void View::update_size(uint32_t width, uint32_t height)
  {
    if (this->width == width && this->height == height) {
      return;
    }

    damage_whole();
    this->width = width;
    this->height = height;
    damage_whole();
  }

  bool View::at(double lx, double ly, wlr::surface_t*& wlr_surface, double& sx, double& sy)
  {
    if (!this->wlr_surface || !this->mapped) return false;
    if (util::dynamic_is<WlShellSurface>(this) &&
        dynamic_cast<WlShellSurface*>(this)->wl_shell_surface->state == WLR_WL_SHELL_SURFACE_STATE_POPUP) {
      return false;
    }

    double view_sx = lx - this->x;
    double view_sy = ly - this->y;

    struct wlr_surface_state* state = &this->wlr_surface->current;
    struct wlr_box box = {
      .x = 0,
      .y = 0,
      .width = state->width,
      .height = state->height,
    };
    if (this->rotation != 0.0) {
      // Coordinates relative to the center of the view
      double ox = view_sx - (double) box.width / 2, oy = view_sy - (double) box.height / 2;
      // Rotated coordinates
      double rx = cos(this->rotation) * ox + sin(this->rotation) * oy,
             ry = cos(this->rotation) * oy - sin(this->rotation) * ox;
      view_sx = rx + (double) box.width / 2;
      view_sy = ry + (double) box.height / 2;
    }

    double _sx, _sy;
    wlr::surface_t* _surface = nullptr;
    if (util::dynamic_is<XdgSurfaceV6>(this)) {
      _surface = wlr_xdg_surface_v6_surface_at(dynamic_cast<XdgSurfaceV6&>(*this).xdg_surface, view_sx, view_sy, &_sx, &_sy);
    } else if (util::dynamic_is<XdgSurface>(this)) {
      _surface = wlr_xdg_surface_surface_at(dynamic_cast<XdgSurface&>(*this).xdg_surface, view_sx, view_sy, &_sx, &_sy);
#ifdef WLR_HAS_XWAYLAND
    } else if (util::dynamic_is<XwaylandSurface>(this)) {
      _surface = wlr_surface_surface_at(dynamic_cast<XwaylandSurface&>(*this).xwayland_surface->surface, view_sx, view_sy, &_sx, &_sy);
#endif
    }
    if (_surface != nullptr) {
      sx = _sx;
      sy = _sy;
      wlr_surface = _surface;
      return true;
    }

    if (get_deco_part(view_sx, view_sy) != DecoPart::none) {
      sx = view_sx;
      sy = view_sy;
      wlr_surface = nullptr;
      return true;
    }

    return false;
  }
} // namespace cloth
