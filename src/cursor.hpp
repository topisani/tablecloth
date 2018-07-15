#pragma once

#include "wlroots.hpp"

namespace cloth {

  struct Seat;

  struct Cursor {
    CLOTH_BIND_BASE(Cursor, wlr::cursor_t);

    enum struct Mode { Passthrough = 0, Move, Resize, Rotate };

    Mode mode;

  	std::string default_xcursor;

  	wlr::xcursor_manager_t* xcursor_manager;
  	Seat* wl_seat;

  	wl_client *cursor_client;
  	int offs_x, offs_y;
  	int view_x, view_y, view_width, view_height;
  	float view_rotation;
  	uint32_t resize_edges;

    wlr::Listener on_motion;
    wlr::Listener on_motion_absolute;
    wlr::Listener on_button;
    wlr::Listener on_axis;

    wlr::Listener on_touch_down;
    wlr::Listener on_touch_up;
    wlr::Listener on_touch_motion;

    wlr::Listener on_tool_axis;
    wlr::Listener on_tool_tip;

    wlr::Listener on_request_set_cursor;
  };

} // namespace cloth
