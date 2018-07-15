#pragma once

#include <xkbcommon/xkbcommon.h>

#include "config.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct Input;

  struct Keyboard {
    ~Keyboard() noexcept;

    void handle_key(wlr::event_keyboard_key_t& event);
    void handle_modifiers();

    static constexpr const int pressed_keysyms_cap = 32;
    Seat& seat;

    wlr::input_device_t* device;
    Config::Keyboard& config;

    wl::Listener on_device_destroy;
    wl::Listener on_keyboard_key;
    wl::Listener on_keyboard_modifiers;

    xkb_keysym_t pressed_keysyms_translated[pressed_keysyms_cap];
    xkb_keysym_t pressed_keysyms_raw[pressed_keysyms_cap];
  private:
    friend struct Seat;
    Keyboard(Seat& seat, wlr::input_device_t* device) noexcept;
  };

} // namespace cloth
