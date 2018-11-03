#pragma once

#include "wlroots.hpp"

#include "gesture.hpp"

namespace cloth {

  struct Seat;
  struct SeatView;
  struct Tablet;

  struct Cursor {
    enum struct Mode { Passthrough = 0, Move, Resize, Rotate };

    Cursor(Seat& seat, wlr::cursor_t* cursor) noexcept;
    ~Cursor() noexcept;

    void update_focus();
    void update_position(uint32_t time);
    void set_visible(bool);
    void constrain(wlr::pointer_constraint_v1_t* constraint, double sx, double sy);

    // Member data

    Seat& seat;
    wlr::cursor_t* wlr_cursor = nullptr;

    wlr::pointer_constraint_v1_t* active_constraint = nullptr;
    pixman_region32_t confine;

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

    SeatView* pointer_view = nullptr;
    wlr::surface_t *wlr_surface;

    wl::Listener on_motion;
    wl::Listener on_motion_absolute;
    wl::Listener on_button;
    wl::Listener on_axis;

    wl::Listener on_touch_down;
    wl::Listener on_touch_up;
    wl::Listener on_touch_motion;

    wl::Listener on_tool_axis;
    wl::Listener on_tool_tip;
    wl::Listener on_tool_proximity;
    wl::Listener on_tool_button;

    wl::Listener on_request_set_cursor;

    wl::Listener on_focus_change;
    wl::Listener on_constraint_commit;

  private:
    void passthrough_cursor(int64_t time);
    void press_button(wlr::input_device_t& device,
                      uint32_t time,
                      wlr::Button button,
                      wlr::button_state_t state,
                      double lx,
                      double ly);

    auto handle_tablet_tool_position(Tablet& tablet,
                                     wlr::tablet_tool_t* wlr_tool,
                                     bool change_x,
                                     bool change_y,
                                     double x,
                                     double y,
                                     double dx,
                                     double dy,
                                     unsigned time) -> void;

    std::optional<TouchGesture> current_gesture = std::nullopt;

    bool _is_visible = true;
  };

} // namespace cloth
