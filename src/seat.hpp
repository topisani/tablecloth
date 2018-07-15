#pragma once

#include "util/bindings.hpp"
#include "wlroots.hpp"
#include "cursor.hpp"

namespace cloth {

  struct Seat;
  struct Input;

  struct DragIcon {
    Seat& seat;
    wlr::drag_icon_t* wlr_drag_icon;

    double x, y;

    wlr::Listener surface_commit;
    wlr::Listener map;
    wlr::Listener unmap;
    wlr::Listener destroy;
  };

  struct Keyboard {
    Seat& seat;
    wlr::input_device_t* device;

    wlr::Listener device_destroy;
    wlr::Listener keyboard_key;
    wlr::Listener keyboard_modifiers;
  };

  struct Pointer {
    Seat& seat;

    wlr::input_device_t* device;
    wlr::Listener device_destroy;
  };

  struct Touch {
    Seat& seat;

    wlr::input_device_t* device;
    wlr::Listener device_destroy;
  };

  struct Tablet {
    Seat& seat;

    wlr::input_device_t* device;
    wlr::Listener device_destroy;

    wlr::Listener axis;
    wlr::Listener proximity;
    wlr::Listener tip;
    wlr::Listener button;
  };

  struct Seat {
    CLOTH_BIND_BASE(Seat, wlr::seat_t);

    void add_device(wlr::input_device_t& device) noexcept;
    void update_capabilities() noexcept;

    Input* input;
    Cursor cursor;

    std::string name;

    std::vector<DragIcon> drag_icons; // roots_drag_icon::link

    std::vector<Keyboard> keyboards;
    std::vector<Pointer> pointers;
    std::vector<Touch> touch;
    std::vector<Tablet> tablet_tools;

    struct wl_listener new_drag_icon;
    struct wl_listener destroy;

  private:

    void add_keyboard(wlr::input_device_t& device) noexcept;
    void add_pointer(wlr::input_device_t& device) noexcept;
    void add_touch(wlr::input_device_t& device) noexcept;
    void add_tablet_pad(wlr::input_device_t& device) noexcept;
    void add_tablet_tool(wlr::input_device_t& device) noexcept;
  };

} // namespace cloth
