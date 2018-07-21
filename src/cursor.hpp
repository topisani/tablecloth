#pragma once

#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct SeatView;

  struct Cursor {
    enum struct Mode { Passthrough = 0, Move, Resize, Rotate };

    Cursor(Seat& seat, wlr::cursor_t* cursor) noexcept;
    ~Cursor() noexcept;

    // Member data

    Seat& seat;
    wlr::cursor_t* wlr_cursor = nullptr;

    Mode mode;

    std::string default_xcursor;

    // state from input (review if this is necessary)
    wlr::xcursor_manager_t* xcursor_manager = nullptr;
    wlr::seat_t* wl_seat = nullptr;
    wl::client_t* cursor_client = nullptr;

    int offs_x, offs_y;
    int view_x, view_y, view_width, view_height;
    float view_rotation = 0;
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

  private:
    void passthrough_cursor(uint32_t time);
    void update_position(uint32_t time);
    void press_button(wlr::input_device_t& device,
                      uint32_t time,
                      wlr::Button button,
                      wlr::button_state_t state,
                      double lx,
                      double ly);
  };

} // namespace cloth
