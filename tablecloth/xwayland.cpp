#include "view.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "wlroots.hpp"

#include "desktop.hpp"
#include "input.hpp"
#include "seat.hpp"
#include "server.hpp"

namespace cloth {

  void XwaylandSurface::do_activate(bool active)
  {
    if (!xwayland_surface->override_redirect)
      wlr_xwayland_surface_activate(xwayland_surface, active);
  }

  void XwaylandSurface::apply_size_constraints(int width,
                                               int height,
                                               int& dest_width,
                                               int& dest_height)
  {
    dest_width = width;
    dest_height = height;

    auto* size_hints = xwayland_surface->size_hints;
    if (size_hints != nullptr) {
      if (width < size_hints->min_width) {
        dest_width = size_hints->min_width;
      } else if (size_hints->max_width > 0 && width > size_hints->max_width) {
        dest_width = size_hints->max_width;
      }
      if (height < size_hints->min_height) {
        dest_height = size_hints->min_height;
      } else if (size_hints->max_height > 0 && height > size_hints->max_height) {
        dest_height = size_hints->max_height;
      }
    }
  }

  void XwaylandSurface::do_resize(int width, int height)
  {
    int constrained_width, constrained_height;
    apply_size_constraints(width, height, constrained_width, constrained_height);

    wlr_xwayland_surface_configure(xwayland_surface, xwayland_surface->x, xwayland_surface->y,
                                   constrained_width, constrained_height);
  }

  void XwaylandSurface::do_move_resize(double x, double y, int width, int height)
  {
    bool update_x = x != this->x;
    bool update_y = y != this->y;

    LOGD("XWL move resize: {}", wlr::box_t{(int) x, (int) y, width, height});

    int constrained_width = width, constrained_height = height;
    apply_size_constraints(width, height, constrained_width, constrained_height);

    if (update_x) {
      x = x + width - constrained_width;
    }
    if (update_y) {
      y = y + height - constrained_height;
    }

    LOGD("Constrained: {}", wlr::box_t{(int) x, (int) y, constrained_width, constrained_height});

    this->pending_move_resize.update_x = update_x;
    this->pending_move_resize.update_y = update_y;
    this->pending_move_resize.x = x;
    this->pending_move_resize.y = y;
    this->pending_move_resize.width = constrained_width;
    this->pending_move_resize.height = constrained_height;

    wlr_xwayland_surface_configure(xwayland_surface, x, y, constrained_width, constrained_height);
  }

  void XwaylandSurface::do_maximize(bool maximized)
  {
    wlr_xwayland_surface_set_maximized(xwayland_surface, maximized);
  }

  void XwaylandSurface::do_set_fullscreen(bool fullscreen)
  {
    wlr_xwayland_surface_set_fullscreen(xwayland_surface, fullscreen);
  }

  void XwaylandSurface::do_close()
  {
    wlr_xwayland_surface_close(xwayland_surface);
  }

  void XwaylandSurface::do_move(double x, double y)
  {
    update_position(x, y);
    wlr_xwayland_surface_configure(xwayland_surface, x, y, xwayland_surface->width,
                                   xwayland_surface->height);
  }

  static Seat* guess_seat_for_view(View& view)
  {
    // the best we can do is to pick the first seat that has the surface focused
    // for the pointer
    auto& input = view.desktop.server.input;
    for (auto& seat : input.seats) {
      if (seat.wlr_seat->pointer_state.focused_surface == view.wlr_surface) {
        return &seat;
      }
    }
    return nullptr;
  }

  auto XwaylandSurface::get_name() -> std::string
  {
    if (xwayland_surface) return util::nonull(xwayland_surface->title);
    return "";
  }

  XwaylandSurface::XwaylandSurface(Workspace& p_workspace,
                                   wlr::xwayland_surface_t* p_xwayland_surface)
    : View(p_workspace), xwayland_surface(p_xwayland_surface)
  {
    View::wlr_surface = xwayland_surface->surface;
    x = xwayland_surface->x;
    y = xwayland_surface->y;
    width = xwayland_surface->width;
    height = xwayland_surface->height;

    on_request_configure.add_to(xwayland_surface->events.request_configure);
    on_request_configure = [this](void* data) {
      auto& e = *(wlr::xwayland_surface_configure_event_t*) data;
      move_resize(e.x, e.y, e.width, e.height);
    };

    on_request_move.add_to(xwayland_surface->events.request_move);
    on_request_move = [this](void* data) {
      Seat* seat = guess_seat_for_view(*this);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_move(*this);
    };

    on_request_resize.add_to(xwayland_surface->events.request_resize);
    on_request_resize = [this](void* data) {
      Seat* seat = guess_seat_for_view(*this);
      auto& e = *(wlr::xwayland_resize_event_t*) data;
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_resize(*this, wlr::edges_t(e.edges));
    };

    on_request_maximize.add_to(xwayland_surface->events.request_maximize);
    on_request_maximize = [this](void* data) {
      maximize(xwayland_surface->maximized_vert && xwayland_surface->maximized_horz);
    };

    on_request_fullscreen.add_to(xwayland_surface->events.request_fullscreen);
    on_request_fullscreen = [this](void* data) {
      set_fullscreen(xwayland_surface->fullscreen, nullptr);
    };

    // Added/removed on map/unmap
    on_surface_commit = [this](void* data) {
      LOGD("XWL surface committed");
      apply_damage();

      int width = xwayland_surface->surface->current.width;
      int height = xwayland_surface->surface->current.height;
      update_size(width, height);

      double x = this->x;
      double y = this->y;
      if (pending_move_resize.update_x) {
        x = pending_move_resize.x + pending_move_resize.width - width;
        pending_move_resize.update_x = false;
      }
      if (pending_move_resize.update_y) {
        y = pending_move_resize.y + pending_move_resize.height - height;
        pending_move_resize.update_y = false;
      }
      update_position(x, y);
    };

    on_map.add_to(xwayland_surface->events.map);
    on_map = [this](void* data) {
      auto& surface = *(wlr::xwayland_surface_t*) data;
      x = surface.x;
      y = surface.y;
      width = surface.surface->current.width;
      height = surface.surface->current.height;
      //move_resize(x, y, width, height);

      on_surface_commit.add_to(surface.surface->events.commit);

      map(*xwayland_surface->surface);
      if (surface.override_redirect) {
        // Usually set on popup windows (like menus, dropdowns etc) to make sure the
        // compositor stays out of it
        //
        // Seat::set_focus() checks this variable, and does not deactivate the previously focused
        // window if it is set That code should probably check if the previously focused window is
        // indeed the parent
        deco.set_visible(false);
        initial_focus();
      } else {
        if (surface.decorations == WLR_XWAYLAND_SURFACE_DECORATIONS_ALL) {
          // TODO: Desired behaviour?
          deco.set_visible(true);
        } else {
          deco.set_visible(false);
        }
        setup();
      }
    };

    on_unmap.add_to(xwayland_surface->events.unmap);
    on_unmap = [this](void* data) {
      LOGD("Unmapped");
      unmap();
      on_surface_commit.remove();
    };

    on_set_title.add_to(xwayland_surface->events.set_title);
    on_set_title = [this](void* data) {
      workspace->desktop.server.window_manager.send_focused_window_name(*workspace);
    };

    on_destroy.add_to(xwayland_surface->events.destroy);
    on_destroy = [this] {
      LOGD("Destroyed");
      workspace->erase_view(*this);
    };
  }

  void Desktop::handle_xwayland_surface(void* data)
  {
    if (data == nullptr) return;
    auto& surface = *(wlr::xwayland_surface_t*) data;

    LOGD("New xwayland surface: title={}, class={}, instance={}", util::nonull(surface.title),
         util::nonull(surface.class_), util::nonull(surface.instance));
    wlr_xwayland_surface_ping(&surface);

    auto& output = outputs.front();
    auto view_ptr = std::make_unique<XwaylandSurface>(*output.workspace, &surface);
    output.workspace->add_view(std::move(view_ptr));
  }
} // namespace cloth
