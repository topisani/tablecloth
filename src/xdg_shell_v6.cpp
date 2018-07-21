#include "view.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "wlroots.hpp"

#include "desktop.hpp"
#include "input.hpp"
#include "server.hpp"
#include "seat.hpp"

namespace cloth {

  XdgPopupV6::XdgPopupV6(View& view, wlr::xdg_popup_v6_t& wlr_popup)
    : ViewChild(view, *wlr_popup.base->surface), wlr_popup(wlr_popup)
  {
    on_destroy.add_to(wlr_popup.base->events.destroy);
    on_destroy = [&] { util::erase_this(view.children, this); };
    on_new_popup.add_to(wlr_popup.base->events.new_popup);
    on_new_popup = [&](void* data) { view.create_popup(*((wlr::xdg_surface_v6_t*) data)->surface); };
    on_unmap.add_to(wlr_popup.base->events.unmap);
    on_unmap = [&] { view.damage_whole(); };
    on_map.add_to(wlr_popup.base->events.map);
    on_map = [&] { view.damage_whole(); };

    // TODO: Desired behaviour?
    unconstrain();
  }

  void XdgPopupV6::unconstrain()
  {
    // get the output of the popup's positioner anchor point and convert it to
    // the toplevel parent's coordinate system and then pass it to
    // wlr_xdg_popup_v6_unconstrain_from_box

    // TODO: unconstrain popups for rotated windows
    if (view.rotation != 0.0) {
      return;
    }

    int anchor_lx, anchor_ly;
    wlr_xdg_popup_v6_get_anchor_point(&wlr_popup, &anchor_lx, &anchor_ly);

    int popup_lx, popup_ly;
    wlr_xdg_popup_v6_get_toplevel_coords(&wlr_popup, wlr_popup.geometry.x, wlr_popup.geometry.y,
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

    wlr_xdg_popup_v6_unconstrain_from_box(&wlr_popup, &output_toplevel_sx_box);
  }

  wlr::box_t XdgSurfaceV6::get_size()
  {
    wlr::box_t geo_box;
    wlr_xdg_surface_v6_get_geometry(xdg_surface, &geo_box);
    return geo_box;
  }

  void XdgSurfaceV6::do_activate(bool active)
  {
    if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
      wlr_xdg_toplevel_v6_set_activated(xdg_surface, active);
    }
  }

  void XdgSurfaceV6::apply_size_constraints(int width,
                                          int height,
                                          int& dest_width,
                                          int& dest_height)
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

  void XdgSurfaceV6::do_resize(int width, int height)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
      return;
    }

    int constrained_width, constrained_height;
    apply_size_constraints(width, height, constrained_width, constrained_height);

    wlr_xdg_toplevel_v6_set_size(xdg_surface, constrained_width, constrained_height);
  }

  void XdgSurfaceV6::do_move_resize(double x, double y, int width, int height)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
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

    uint32_t serial = wlr_xdg_toplevel_v6_set_size(xdg_surface, constrained_width, constrained_height);
    if (serial > 0) {
      pending_move_resize_configure_serial = serial;
    } else if (pending_move_resize_configure_serial == 0) {
      update_position(x, y);
    }
  }

  void XdgSurfaceV6::do_maximize(bool maximized)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
      return;
    }

    wlr_xdg_toplevel_v6_set_maximized(xdg_surface, maximized);
  }

  void XdgSurfaceV6::do_set_fullscreen(bool fullscreen)
  {
    if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
      return;
    }

    wlr_xdg_toplevel_v6_set_fullscreen(xdg_surface, fullscreen);
  }

  void XdgSurfaceV6::do_close()
  {
    wlr::xdg_popup_v6_t* popup = NULL;
    wl_list_for_each(popup, &xdg_surface->popups, link)
    {
      wlr_xdg_surface_v6_send_close(popup->base);
    }
    wlr_xdg_surface_v6_send_close(xdg_surface);
  }

  ViewChild& XdgSurfaceV6::create_popup(wlr::surface_t& wlr_popup) {
    wlr::xdg_popup_v6_t* xdg_pop = wl_container_of(&wlr_popup, std::declval<wlr::xdg_popup_v6_t*>(), base);
    auto popup = std::make_unique<XdgPopupV6>(*this, *xdg_pop);
    return children.push_back(std::move(popup));
  }

  XdgSurfaceV6::XdgSurfaceV6(Desktop& desktop, wlr::xdg_surface_v6_t* xdg_surface)
    : View(desktop), xdg_surface(xdg_surface)
  {
    View::wlr_surface = xdg_surface->surface;
    width = xdg_surface->surface->current.width;
    height = xdg_surface->surface->current.height;
    on_request_move.add_to(xdg_surface->toplevel->events.request_move);
    on_request_move = [&](void* data) {
      Input& input = this->desktop.server.input;
      auto& e = *(wlr::xdg_toplevel_v6_move_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_move(*this);
    };

    on_request_resize.add_to(xdg_surface->toplevel->events.request_resize);
    on_request_resize = [&](void* data) {
      Input& input = this->desktop.server.input;
      auto& e = *(wlr::xdg_toplevel_v6_resize_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_resize(*this, wlr::edges_t(e.edges));
    };

    on_request_maximize.add_to(xdg_surface->toplevel->events.request_maximize);
    on_request_maximize = [&](void* data) {
      if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) return;
      maximize(xdg_surface->toplevel->client_pending.maximized);
    };

    on_request_fullscreen.add_to(xdg_surface->toplevel->events.request_fullscreen);
    on_request_fullscreen = [&](void* data) {
      if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) return;
      auto& e = *(wlr::xdg_toplevel_v6_set_fullscreen_event_t*) data;
      set_fullscreen(e.fullscreen, e.output);
    };

    on_surface_commit.add_to(xdg_surface->surface->events.commit);
    on_surface_commit = [&](void* data) {
      if (!xdg_surface->mapped) return;

      apply_damage();

      auto size = get_size();
      update_size(size.width, size.height);

    	uint32_t pending_serial = pending_move_resize_configure_serial;
    	if (pending_serial > 0 && pending_serial >= xdg_surface->configure_serial) {
    		double x = this->x;
    		double y = this->y;
    		if (pending_move_resize.update_x) {
    			x = pending_move_resize.x + pending_move_resize.width -
    				size.width;
    		}
    		if (pending_move_resize.update_y) {
    			y = pending_move_resize.y + pending_move_resize.height -
    				size.height;
    		}
    		update_position(x, y);

    		if (pending_serial == xdg_surface->configure_serial) {
    			pending_move_resize_configure_serial = 0;
    		}
    	}
    };

    on_new_popup.add_to(xdg_surface->events.new_popup);
    on_new_popup = [&](void* data) {
      auto& wlr_xdg_surface = *(wlr::xdg_surface_v6_t*) data;
      create_popup(*wlr_xdg_surface.surface);
    };

    on_map.add_to(xdg_surface->events.map);
    on_map = [&](void* data) {
      auto box = get_size();
      width = box.width;
      height = box.height;

      map(*xdg_surface->surface);
      setup();
    };

    on_unmap.add_to(xdg_surface->events.unmap);
    on_unmap = [&](void* data) {
      unmap();
    };

    on_destroy.add_to(xdg_surface->events.destroy);
    on_destroy = [&] { util::erase_this(desktop.views, this); };
  }

  void Desktop::handle_xdg_shell_v6_surface(void* data)
  {
    auto& surface = *(wlr::xdg_surface_v6_t*) data;

    if (surface.role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
      LOGD("new xdg popup");
      return;
    }

    LOGD("new xdg toplevel: title={}, class={}", surface.toplevel->title, surface.toplevel->app_id);
    wlr_xdg_surface_v6_ping(&surface);

    auto view_ptr = std::make_unique<XdgSurfaceV6>(*this, &surface);
    auto& view = *view_ptr;
    views.push_back(std::move(view_ptr));

  	if (surface.toplevel->client_pending.maximized) {
  		view.maximize(true);
  	}
  	if (surface.toplevel->client_pending.fullscreen) {
  		view.set_fullscreen(true, nullptr);
  	}
  }
} // namespace cloth
