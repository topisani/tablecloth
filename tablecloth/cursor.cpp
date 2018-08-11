#include "cursor.hpp"

#include <math.h>
#include <stdlib.h>

#include "util/logging.hpp"

#include "wlroots.hpp"

#include "desktop.hpp"
#include "input.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "xcursor.hpp"

namespace cloth {

  static void handle_tablet_tool_position(Cursor& cursor,
                                          Tablet& tablet,
                                          wlr::tablet_tool_t* wlr_tool,
                                          bool change_x,
                                          bool change_y,
                                          double x,
                                          double y,
                                          double dx,
                                          double dy)
  {
    if (!change_x && !change_y) {
      return;
    }
    switch (wlr_tool->type) {
    case WLR_TABLET_TOOL_TYPE_MOUSE:
      // They are 0 either way when they weren't modified
      wlr_cursor_move(cursor.wlr_cursor, &tablet.wlr_device, dx, dy);
      break;
    default:
      wlr_cursor_warp_absolute(cursor.wlr_cursor, &tablet.wlr_device, change_x ? x : NAN,
                               change_y ? y : NAN);
    }
    double sx, sy;
    View* view = nullptr;
    Seat& seat = cursor.seat;
    Desktop& desktop = seat.input.server.desktop;
    wlr::surface_t* surface =
      desktop.surface_at(cursor.wlr_cursor->x, cursor.wlr_cursor->y, sx, sy, view);
    auto& tool = *(TabletTool*) wlr_tool->data;
    if (!surface) {
      wlr_send_tablet_v2_tablet_tool_proximity_out(&tool.tablet_v2_tool);
      /* XXX: TODO: Fallback pointer semantics */
      return;
    }
    if (!wlr_surface_accepts_tablet_v2(&tablet.tablet_v2, surface)) {
      wlr_send_tablet_v2_tablet_tool_proximity_out(&tool.tablet_v2_tool);
      /* XXX: TODO: Fallback pointer semantics */
      return;
    }
    wlr_send_tablet_v2_tablet_tool_proximity_in(&tool.tablet_v2_tool, &tablet.tablet_v2, surface);
    wlr_send_tablet_v2_tablet_tool_motion(&tool.tablet_v2_tool, sx, sy);
  }


  Cursor::Cursor(Seat& p_seat, wlr::cursor_t* p_cursor) noexcept
    : seat(p_seat), wlr_cursor(p_cursor), default_xcursor(xcursor_default)
  {
    Desktop& desktop = seat.input.server.desktop;
    wlr_cursor_attach_output_layout(wlr_cursor, desktop.layout);

    // add input signals
    on_motion.add_to(wlr_cursor->events.motion);
    on_motion = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_pointer_motion_t*) data;
      wlr_cursor_move(wlr_cursor, event->device, event->delta_x, event->delta_y);
      update_position(event->time_msec);
    };

    on_motion_absolute.add_to(wlr_cursor->events.motion_absolute);
    on_motion_absolute = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_pointer_motion_absolute_t*) data;
      wlr_cursor_warp_absolute(wlr_cursor, event->device, event->x, event->y);
      update_position(event->time_msec);
    };

    on_button.add_to(wlr_cursor->events.button);
    on_button = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_pointer_button_t*) data;
      press_button(*event->device, event->time_msec, wlr::Button(event->button), event->state,
                   wlr_cursor->x, wlr_cursor->y);
    };

    on_axis.add_to(wlr_cursor->events.axis);
    on_axis = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_pointer_axis_t*) data;
      wlr_seat_pointer_notify_axis(this->seat.wlr_seat, event->time_msec, event->orientation,
                                   event->delta, event->delta_discrete, event->source);
    };

    on_touch_down.add_to(wlr_cursor->events.touch_down);
    on_touch_down = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_touch_down_t*) data;
      Desktop& desktop = seat.input.server.desktop;
      double lx, ly;
      wlr_cursor_absolute_to_layout_coords(wlr_cursor, event->device, event->x, event->y, &lx, &ly);

      double sx, sy;
      View* v;
      auto* surface = desktop.surface_at(lx, ly, sx, sy, v);

      uint32_t serial = 0;
      if (surface && seat.allow_input(*surface->resource)) {
        serial = wlr_seat_touch_notify_down(this->seat.wlr_seat, surface, event->time_msec,
                                            event->touch_id, sx, sy);
      }

      if (serial && wlr_seat_touch_num_points(this->seat.wlr_seat) == 1) {
        seat.touch_id = event->touch_id;
        seat.touch_x = lx;
        seat.touch_y = ly;
        press_button(*event->device, event->time_msec, wlr::Button::left, WLR_BUTTON_PRESSED, lx,
                     ly);
      }
    };

    on_touch_up.add_to(wlr_cursor->events.touch_up);
    on_touch_up = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_touch_up_t*) data;
      wlr::touch_point_t* point = wlr_seat_touch_get_point(this->seat.wlr_seat, event->touch_id);
      if (!point) {
        return;
      }

      if (wlr_seat_touch_num_points(this->seat.wlr_seat) == 1) {
        press_button(*event->device, event->time_msec, wlr::Button::left, WLR_BUTTON_RELEASED,
                     seat.touch_x, seat.touch_y);
      }

      wlr_seat_touch_notify_up(this->seat.wlr_seat, event->time_msec, event->touch_id);
    };

    on_touch_motion.add_to(wlr_cursor->events.touch_motion);
    on_touch_motion = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_touch_motion_t*) data;
      auto& desktop = seat.input.server.desktop;
      wlr::touch_point_t* point = wlr_seat_touch_get_point(this->seat.wlr_seat, event->touch_id);
      if (!point) {
        return;
      }

      double lx, ly;
      wlr_cursor_absolute_to_layout_coords(wlr_cursor, event->device, event->x, event->y, &lx, &ly);

      double sx, sy;
      View* view;
      wlr::surface_t* surface = desktop.surface_at(lx, ly, sx, sy, view);

      if (surface && seat.allow_input(*surface->resource)) {
        wlr_seat_touch_point_focus(this->seat.wlr_seat, surface, event->time_msec, event->touch_id,
                                   sx, sy);
        wlr_seat_touch_notify_motion(this->seat.wlr_seat, event->time_msec, event->touch_id, sx,
                                     sy);
      } else {
        wlr_seat_touch_point_clear_focus(this->seat.wlr_seat, event->time_msec, event->touch_id);
      }

      if (event->touch_id == seat.touch_id) {
        seat.touch_x = lx;
        seat.touch_y = ly;
      }
    };

    on_tool_axis.add_to(wlr_cursor->events.tablet_tool_axis);
    on_tool_axis = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_tablet_tool_axis_t*) data;
      assert(event->tool->data);
      auto& tool = *(TabletTool*) event->tool->data;
      auto& tablet = *(Tablet*) event->device->data;

      /**
       * We need to handle them ourselves, not pass it into the cursor
       * without any consideration
       */
      handle_tablet_tool_position(
        *this, tablet, event->tool, event->updated_axes & WLR_TABLET_TOOL_AXIS_X,
        event->updated_axes & WLR_TABLET_TOOL_AXIS_Y, event->x, event->y, event->dx, event->dy);
      if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
        wlr_send_tablet_v2_tablet_tool_pressure(&tool.tablet_v2_tool, event->pressure);
      }
      if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
        wlr_send_tablet_v2_tablet_tool_distance(&tool.tablet_v2_tool, event->distance);
      }
      if (event->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
        wlr_send_tablet_v2_tablet_tool_tilt(&tool.tablet_v2_tool, event->tilt_x, event->tilt_y);
      }
      if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
        wlr_send_tablet_v2_tablet_tool_rotation(&tool.tablet_v2_tool, event->rotation);
      }
      if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
        wlr_send_tablet_v2_tablet_tool_slider(&tool.tablet_v2_tool, event->slider);
      }
      if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
        wlr_send_tablet_v2_tablet_tool_wheel(&tool.tablet_v2_tool, event->wheel_delta, 0);
      }
    };

    on_tool_tip.add_to(wlr_cursor->events.tablet_tool_tip);
    on_tool_tip = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_tablet_tool_tip_t*) data;
      auto& tool = *(TabletTool*) event->tool->data;

      if (event->state == WLR_TABLET_TOOL_TIP_DOWN) {
        wlr_send_tablet_v2_tablet_tool_down(&tool.tablet_v2_tool);
      } else {
        wlr_send_tablet_v2_tablet_tool_up(&tool.tablet_v2_tool);
      }
    };

    on_tool_proximity.add_to(wlr_cursor->events.tablet_tool_proximity);
    on_tool_proximity = [this](void* data) {
      Desktop& desktop = seat.input.server.desktop;
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_tablet_tool_proximity_t*) data;
      wlr::tablet_tool_t* wlr_tool = event->tool;
      if (!wlr_tool->data) {
        // This is attached to wlr_tool.data, and deleted when wlr_tool is destroyed
        // TODO: cleaner solution? I mean, this works fine...
        new TabletTool(seat, *wlr_tablet_tool_create(desktop.tablet_v2, seat.wlr_seat, wlr_tool));
      }
      handle_tablet_tool_position(*this, *(Tablet*) event->device->data, event->tool, true, true, event->x,
                                  event->y, 0, 0);
    };


    on_tool_button.add_to(wlr_cursor->events.tablet_tool_button);
    on_tool_button = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::event_tablet_tool_button_t*) data;
      auto& tool = *(TabletTool*) event->tool->data;

      wlr_send_tablet_v2_tablet_tool_button(&tool.tablet_v2_tool,
                                            (enum zwp_tablet_pad_v2_button_state) event->button,
                                            (enum zwp_tablet_pad_v2_button_state) event->state);
    };

    on_request_set_cursor.add_to(seat.wlr_seat->events.request_set_cursor);
    on_request_set_cursor = [this](void* data) {
      wlr_idle_notify_activity(seat.input.server.desktop.idle, seat.wlr_seat);
      auto* event = (wlr::seat_pointer_request_set_cursor_event_t*) data;
      wlr::surface_t* focused_surface = event->seat_client->seat->pointer_state.focused_surface;
      bool has_focused = focused_surface != nullptr && focused_surface->resource != nullptr;
      struct wl_client* focused_client = nullptr;
      if (has_focused) {
        focused_client = wl_resource_get_client(focused_surface->resource);
      }
      if (event->seat_client->client != focused_client || mode != Mode::Passthrough) {
        LOGD("Denying request to set cursor from unfocused client");
        // return;
      }

      wlr_cursor_set_surface(wlr_cursor, event->surface, event->hotspot_x, event->hotspot_y);
      cursor_client = event->seat_client->client;
    };
  }

  Cursor::~Cursor() noexcept
  {
    // TODO
  }

  void Cursor::passthrough_cursor(uint32_t time)
  {
    double sx, sy;
    View* view = nullptr;
    auto* surface =
      seat.input.server.desktop.surface_at(wlr_cursor->x, wlr_cursor->y, sx, sy, view);
    wl::client_t* client = nullptr;
    if (surface) {
      client = wl_resource_get_client(surface->resource);
    }
    if (surface && !seat.allow_input(*surface->resource)) {
      return;
    }

    if (cursor_client != client) {
      wlr_xcursor_manager_set_cursor_image(xcursor_manager, default_xcursor.c_str(), wlr_cursor);
      cursor_client = client;
    }

    if (view) {
      SeatView& seat_view = seat.seat_view_from_view(*view);
      if (pointer_view && (surface || &seat_view != pointer_view)) {
        pointer_view->deco_leave();
        pointer_view = nullptr;
      }
      if (!surface) {
        pointer_view = &seat_view;
        seat_view.deco_motion(sx, sy);
      }
    }

    if (surface) {
      // https://github.com/swaywm/wlroots/commit/2e6eb097b6e23b8923bbfc68b1843d5ccde1955b
      // Whenever a new surface is created, we have to update the cursor focus,
      // even if there's no input event. So, we generate one motion event, and
      // reuse the code to update the proper cursor focus. We need to do this
      // for all surface roles - toplevels, popups, subsurfaces.
      bool focus_changed = (seat.wlr_seat->pointer_state.focused_surface != surface);
      wlr_seat_pointer_notify_enter(seat.wlr_seat, surface, sx, sy);
      if (!focus_changed) {
        wlr_seat_pointer_notify_motion(seat.wlr_seat, time, sx, sy);
      }
    } else {
      wlr_seat_pointer_clear_focus(seat.wlr_seat);
    }

    for (auto& icon : seat.drag_icons) icon.update_position();
  }

  void Cursor::update_position(uint32_t time)
  {
    View* view;
    switch (mode) {
    case Mode::Passthrough: passthrough_cursor(time); break;
    case Mode::Move:
      view = seat.get_focus();
      if (view != nullptr) {
        double dx = wlr_cursor->x - this->offs_x;
        double dy = wlr_cursor->y - this->offs_y;
        view->move(this->view_x + dx, this->view_y + dy);
      }
      break;
    case Mode::Resize:
      view = seat.get_focus();
      if (view != nullptr) {
        double dx = wlr_cursor->x - this->offs_x;
        double dy = wlr_cursor->y - this->offs_y;
        double x = view->x;
        double y = view->y;
        int width = this->view_width;
        int height = this->view_height;
        if (this->resize_edges & WLR_EDGE_TOP) {
          y = this->view_y + dy;
          height -= dy;
          if (height < 1) {
            y += height;
          }
        } else if (this->resize_edges & WLR_EDGE_BOTTOM) {
          height += dy;
        }
        if (this->resize_edges & WLR_EDGE_LEFT) {
          x = this->view_x + dx;
          width -= dx;
          if (width < 1) {
            x += width;
          }
        } else if (this->resize_edges & WLR_EDGE_RIGHT) {
          width += dx;
        }
        view->move_resize(x, y, width < 1 ? 1 : width, height < 1 ? 1 : height);
      }
      break;
    case Mode::Rotate:
      view = seat.get_focus();
      if (view != nullptr) {
        int ox = view->x + view->wlr_surface->current.width / 2,
            oy = view->y + view->wlr_surface->current.height / 2;
        int ux = this->offs_x - ox, uy = this->offs_y - oy;
        int vx = wlr_cursor->x - ox, vy = wlr_cursor->y - oy;
        float angle = atan2(ux * vy - uy * vx, vx * ux + vy * uy);
        int steps = 12;
        angle = round(angle / M_PI * steps) / (steps / M_PI);
        view->rotate(this->view_rotation + angle);
      }
      break;
    }
  }

  void Cursor::press_button(wlr::input_device_t& device,
                            uint32_t time,
                            wlr::Button button,
                            wlr::button_state_t state,
                            double lx,
                            double ly)
  {
    auto& desktop = seat.input.server.desktop;

    bool is_touch = device.type == WLR_INPUT_DEVICE_TOUCH;

    double sx, sy;
    View* view;
    wlr::surface_t* surface = desktop.surface_at(lx, ly, sx, sy, view);

    if (state == WLR_BUTTON_PRESSED && view && seat.has_meta_pressed()) {
      view->workspace->set_focused_view(view);

      wlr::edges_t edges;
      switch (button) {
      case wlr::Button::left: seat.begin_move(*view); break;
      case wlr::Button::right:
        edges = WLR_EDGE_NONE;
        if (sx < view->wlr_surface->current.width / 2) {
          edges |= WLR_EDGE_LEFT;
        } else {
          edges |= WLR_EDGE_RIGHT;
        }
        if (sy < view->wlr_surface->current.height / 2) {
          edges |= WLR_EDGE_TOP;
        } else {
          edges |= WLR_EDGE_BOTTOM;
        }
        seat.begin_resize(*view, edges);
        break;
      case wlr::Button::middle: seat.begin_rotate(*view); break;
      default: break;
      }
    } else {
      if (view && !surface && pointer_view) {
        pointer_view->deco_button(sx, sy, button, state);
      }

      if (state == WLR_BUTTON_RELEASED && mode != Cursor::Mode::Passthrough) {
        mode = Mode::Passthrough;
      }

      switch (state) {
      case WLR_BUTTON_RELEASED:
        if (!is_touch) {
          // update_position(time);
        }
        break;
      case WLR_BUTTON_PRESSED:
        if (view) {
          view->workspace->set_focused_view(view);
        }
        if (surface && wlr_surface_is_layer_surface(surface)) {
          auto* layer = wlr_layer_surface_from_wlr_surface(surface);
          if (layer->current.keyboard_interactive) {
            seat.set_focus_layer(layer);
          }
        }
        break;
      }
    }

    if (!is_touch) {
      wlr_seat_pointer_notify_button(seat.wlr_seat, time, util::enum_cast(button), state);
    }
  }

} // namespace cloth
