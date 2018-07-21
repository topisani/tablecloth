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

    wlr_xwayland_surface_configure(xwayland_surface, xwayland_surface->x, xwayland_surface->y, constrained_width,
                                   constrained_height);
  }

  void XwaylandSurface::do_move_resize(double x, double y, int width, int height)
  {
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

    wlr_xwayland_surface_configure(xwayland_surface, x, y, constrained_width, constrained_width);
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

  XwaylandSurface::XwaylandSurface(Desktop& desktop, wlr::xwayland_surface_t* xwayland_surface)
    : View(desktop), xwayland_surface(xwayland_surface)
  {
    width = xwayland_surface->surface->current.width;
    height = xwayland_surface->surface->current.height;

    on_request_configure.add_to(xwayland_surface->events.request_configure);
    on_request_configure = [&](void* data) {
      auto& e = *(wlr::xwayland_surface_configure_event_t*) data;
      update_position(e.x, e.y);
    };

    on_request_move.add_to(xwayland_surface->events.request_move);
    on_request_move = [&](void* data) {
      Seat* seat = guess_seat_for_view(*this);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_move(*this);
    };

    on_request_resize.add_to(xwayland_surface->events.request_resize);
    on_request_resize = [&](void* data) {
      Seat* seat = guess_seat_for_view(*this);
      auto& e = *(wlr::xwayland_resize_event_t*) data;
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_resize(*this, wlr::edges_t(e.edges));
    };

    on_request_maximize.add_to(xwayland_surface->events.request_maximize);
    on_request_maximize = [&](void* data) {
      maximize(xwayland_surface->maximized_vert && xwayland_surface->maximized_horz);
    };

    on_request_fullscreen.add_to(xwayland_surface->events.request_fullscreen);
    on_request_fullscreen = [&](void* data) { set_fullscreen(xwayland_surface->fullscreen, nullptr); };

    // Added/removed on map/unmap
    on_surface_commit = [&](void* data) {
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
    on_map = [&](void* data) {
      auto& surface = *(wlr::xwayland_surface_t*) data;
      x = surface.x;
      y = surface.y;
      width = surface.surface->current.width;
      height = surface.surface->current.height;

      on_surface_commit.add_to(surface.surface->events.commit);

      map(*xwayland_surface->surface);
      if (!surface.override_redirect) {
        if (surface.decorations == WLR_XWAYLAND_SURFACE_DECORATIONS_ALL) {
          // TODO: Desired behaviour?
          decorated = true;
          border_width = 4;
          titlebar_height = 12;
        }
        setup();
      } else {
        initial_focus();
      }
    };

    on_unmap.add_to(xwayland_surface->events.unmap);
    on_unmap = [&](void* data) {
      on_surface_commit.remove();
      unmap();
    };

    on_destroy.add_to(xwayland_surface->events.destroy);
    on_destroy = [&] { util::erase_this(desktop.views, this); };
  }

  void Desktop::handle_xwayland_surface(void* data)
  {
    auto& surface = *(wlr::xwayland_surface_t*) data;

    LOGD("New xwayland surface: title={}, class={}, instance={}", surface.title, surface.class_,
         surface.instance);
    wlr_xwayland_surface_ping(&surface);

    auto view_ptr = std::make_unique<XwaylandSurface>(*this, &surface);
    views.push_back(std::move(view_ptr));
  }
} // namespace cloth
