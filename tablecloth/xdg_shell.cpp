#include "view.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "wlroots.hpp"

#include "desktop.hpp"
#include "input.hpp"
#include "seat.hpp"
#include "server.hpp"

namespace cloth {

  XdgPopup::XdgPopup(View& p_view, wlr::xdg_popup_t* p_wlr_popup)
    : ViewChild(p_view, p_wlr_popup->base->surface), wlr_popup(p_wlr_popup)
  {
    on_destroy.add_to(wlr_popup->base->events.destroy);
    on_destroy = [this] { util::erase_this(view.children, this); };
    on_new_popup.add_to(wlr_popup->base->events.new_popup);
    on_new_popup = [this](void* data) {
      dynamic_cast<XdgSurface&>(view).create_popup(*((wlr::xdg_popup_t*) data));
    };
    on_unmap.add_to(wlr_popup->base->events.unmap);
    on_unmap = [this] { view.damage_whole(); };
    on_map.add_to(wlr_popup->base->events.map);
    on_map = [this] { view.damage_whole(); };

    // TODO: Desired behaviour?
    unconstrain();
  }

  void XdgPopup::unconstrain()
  {
    // get the output of the popup's positioner anchor point and convert it to
    // the toplevel parent's coordinate system and then pass it to
    // wlr_xdg_popup_v6_unconstrain_from_box

    // TODO: unconstrain popups for rotated windows
    if (view.rotation != 0.0) {
      return;
    }

    int anchor_lx, anchor_ly;
    wlr_xdg_popup_get_anchor_point(wlr_popup, &anchor_lx, &anchor_ly);

    int popup_lx, popup_ly;
    wlr_xdg_popup_get_toplevel_coords(wlr_popup, wlr_popup->geometry.x, wlr_popup->geometry.y,
                                      &popup_lx, &popup_ly);
    popup_lx += view.x;
    popup_ly += view.y;

    anchor_lx += popup_lx;
    anchor_ly += popup_ly;

    double dest_x = 0, dest_y = 0;
    wlr_output_layout_closest_point(view.desktop.layout, nullptr, anchor_lx, anchor_ly, &dest_x,
                                    &dest_y);

    wlr::output_t* output = wlr_output_layout_output_at(view.desktop.layout, dest_x, dest_y);

    if (output == nullptr) {
      return;
    }

    int width = 0, height = 0;
    wlr_output_effective_resolution(output, &width, &height);

    // the output box expressed in the coordinate system of the toplevel parent
    // of the popup
    wlr::box_t output_toplevel_sx_box = {.x = int(output->lx - view.x),
                                         .y = int(output->ly - view.y),
                                         .width = width,
                                         .height = height};

    wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
  }

  wlr::box_t XdgSurface::get_size()
  {
    wlr::box_t geo_box;
    wlr_xdg_surface_get_geometry(xdg_surface, &geo_box);
    return geo_box;
  }

  void XdgSurface::do_activate(bool active)
  {
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      wlr_xdg_toplevel_set_activated(xdg_surface, active);
    }
  }

  void XdgSurface::apply_size_constraints(int width, int height, int& dest_width, int& dest_height)
  {
    dest_width = width;
    dest_height = height;

    auto* state = &xdg_surface->toplevel->current;
    if (width < state->min_width) {
      dest_width = state->min_width;
    } else if (state->max_width > 0 && width > state->max_width) {
      dest_width = state->max_width;
    }
    if (height < state->min_height) {
      dest_height = state->min_height;
    } else if (state->max_height > 0 && height > state->max_height) {
      dest_height = state->max_height;
    }
  }

  void XdgSurface::do_resize(int width, int height)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return;
    }

    int constrained_width, constrained_height;
    apply_size_constraints(width, height, constrained_width, constrained_height);

    wlr_xdg_toplevel_set_size(xdg_surface, constrained_width, constrained_height);
  }

  void XdgSurface::do_move_resize(double x, double y, int width, int height)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return;
    }

    bool update_x = x != this->x;
    bool update_y = y != this->y;

    int constrained_width, constrained_height;
    apply_size_constraints(width, height, constrained_width, constrained_height);

    if (update_x) {
      x = x + width - constrained_width;
    }
    if (update_y) {
      y = y + height - constrained_height;
    }

    this->pending_move_resize.update_x = update_x;
    this->pending_move_resize.update_y = update_y;
    this->pending_move_resize.x = x;
    this->pending_move_resize.y = y;
    this->pending_move_resize.width = constrained_width;
    this->pending_move_resize.height = constrained_height;

    uint32_t serial = wlr_xdg_toplevel_set_size(xdg_surface, constrained_width, constrained_height);
    if (serial > 0) {
      pending_move_resize_configure_serial = serial;
    } else if (pending_move_resize_configure_serial == 0) {
      update_position(x, y);
    }
  }

  void XdgSurface::do_maximize(bool maximized)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return;
    }

    wlr_xdg_toplevel_set_maximized(xdg_surface, maximized);
  }

  void XdgSurface::do_set_fullscreen(bool fullscreen)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return;
    }

    wlr_xdg_toplevel_set_fullscreen(xdg_surface, fullscreen);
  }

  void XdgSurface::do_close()
  {
    wlr::xdg_popup_t* popup = NULL;
    wl_list_for_each(popup, &xdg_surface->popups, link)
    {
      wlr_xdg_surface_send_close(popup->base);
    }
    wlr_xdg_surface_send_close(xdg_surface);
  }

  XdgPopup& XdgSurface::create_popup(wlr::xdg_popup_t& wlr_popup)
  {
    auto popup = std::make_unique<XdgPopup>(*this, &wlr_popup);
    children.push_back(std::move(popup));
    return *popup;
  }

  auto XdgSurface::get_name() -> std::string
  {
    if (!xdg_surface) return "";
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return util::nonull(xdg_surface->toplevel->title);
    } else {
      return "";
    }
  }

  XdgSurface::XdgSurface(Workspace& p_workspace, wlr::xdg_surface_t* p_xdg_surface)
    : View(p_workspace), xdg_surface(p_xdg_surface)
  {
    View::wlr_surface = xdg_surface->surface;
    width = xdg_surface->surface->current.width;
    height = xdg_surface->surface->current.height;
    on_request_move.add_to(xdg_surface->toplevel->events.request_move);
    on_request_move = [this](void* data) {
      Input& input = this->desktop.server.input;
      auto& e = *(wlr::xdg_toplevel_move_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_move(*this);
    };

    on_request_resize.add_to(xdg_surface->toplevel->events.request_resize);
    on_request_resize = [this](void* data) {
      Input& input = this->desktop.server.input;
      auto& e = *(wlr::xdg_toplevel_resize_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_resize(*this, wlr::edges_t(e.edges));
    };

    on_request_maximize.add_to(xdg_surface->toplevel->events.request_maximize);
    on_request_maximize = [this](void* data) {
      if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) return;
      maximize(xdg_surface->toplevel->client_pending.maximized);
    };

    on_request_fullscreen.add_to(xdg_surface->toplevel->events.request_fullscreen);
    on_request_fullscreen = [this](void* data) {
      if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) return;
      auto& e = *(wlr::xdg_toplevel_set_fullscreen_event_t*) data;
      set_fullscreen(e.fullscreen, e.output);
    };

    on_surface_commit.add_to(xdg_surface->surface->events.commit);
    on_surface_commit = [this](void* data) {
      if (!xdg_surface->mapped) return;

      apply_damage();

      auto size = get_size();
      update_size(size.width, size.height);

      uint32_t pending_serial = pending_move_resize_configure_serial;
      if (pending_serial > 0 && pending_serial >= xdg_surface->configure_serial) {
        double x = this->x;
        double y = this->y;
        if (pending_move_resize.update_x) {
          x = pending_move_resize.x + pending_move_resize.width - size.width;
        }
        if (pending_move_resize.update_y) {
          y = pending_move_resize.y + pending_move_resize.height - size.height;
        }
        update_position(x, y);

        if (pending_serial == xdg_surface->configure_serial) {
          pending_move_resize_configure_serial = 0;
        }
      }
    };

    on_new_popup.add_to(xdg_surface->events.new_popup);
    on_new_popup = [this](void* data) { create_popup(*(wlr::xdg_popup_t*) data); };

    on_map.add_to(xdg_surface->events.map);
    on_map = [this](void* data) {
      auto box = get_size();
      width = box.width;
      height = box.height;

      map(*xdg_surface->surface);
      setup();
    };

    on_unmap.add_to(xdg_surface->events.unmap);
    on_unmap = [this](void* data) { unmap(); };

    on_destroy.add_to(xdg_surface->events.destroy);
    on_destroy = [this] { workspace->erase_view(*this); };
  }

  void Desktop::handle_xdg_shell_surface(void* data)
  {
    auto& surface = *(wlr::xdg_surface_t*) data;

    if (surface.role == WLR_XDG_SURFACE_ROLE_POPUP) {
      LOGD("new xdg popup");
      return;
    }

    LOGD("new xdg toplevel: title={}, class={}", util::nonull(surface.toplevel->title),
         util::nonull(surface.toplevel->app_id));
    wlr_xdg_surface_ping(&surface);

    auto& workspace = outputs.front().workspace;
    auto view_ptr = std::make_unique<XdgSurface>(*workspace, &surface);
    auto& view = *view_ptr;
    workspace->add_view(std::move(view_ptr));

    if (surface.toplevel->client_pending.maximized) {
      view.maximize(true);
    }
    if (surface.toplevel->client_pending.fullscreen) {
      view.set_fullscreen(true, nullptr);
    }
  }
} // namespace cloth
