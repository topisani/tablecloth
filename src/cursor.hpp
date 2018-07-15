#pragma once

#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct SeatView;

  struct Cursor {
    enum struct Mode { Passthrough = 0, Move, Resize, Rotate };

    Cursor(Seat& seat, wlr::cursor_t* cursor) noexcept;
    ~Cursor() noexcept;

    void handle_motion(wlr::event_pointer_motion_t& event);
    void handle_motion_absolute(wlr::event_pointer_motion_absolute_t& event);
    void handle_button(wlr::event_pointer_button_t& event);
    void handle_axis(wlr::event_pointer_axis_t& event);
    void handle_touch_down(wlr::event_touch_down_t& event);
    void handle_touch_up(wlr::event_touch_up_t& event);
    void handle_touch_motion(wlr::event_touch_motion_t& event);
    void handle_tool_axis(wlr::event_tablet_tool_axis_t& event);
    void handle_tool_tip(wlr::event_tablet_tool_tip_t& event);
    void handle_request_set_cursor(wlr::seat_pointer_request_set_cursor_event_t& event);

    // Member data

    Seat& seat;
    wlr::cursor_t* wlr_cursor;

    Mode mode;

    std::string default_xcursor;

    // state from input (review if this is necessary)
    wlr::xcursor_manager_t* xcursor_manager;
    wlr::seat_t* wl_seat;
    wl::client_t* cursor_client;

    int offs_x, offs_y;
    int view_x, view_y, view_width, view_height;
    float view_rotation;
    uint32_t resize_edges;

    SeatView* pointer_view;

    wl::Listener on_motion;
    wl::Listener on_motion_absolute;
    wl::Listener on_button;
    wl::Listener on_axis;

    wl::Listener on_touch_down;
    wl::Listener on_touch_up;
    wl::Listener on_touch_motion;

    wl::Listener on_tool_axis;
    wl::Listener on_tool_tip;

    wl::Listener on_request_set_cursor;
  };

} // namespace cloth
