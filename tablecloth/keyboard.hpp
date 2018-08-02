#pragma once

#include "config.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct Input;

  using xkb_keysym_t = uint32_t;
  using xkb_keycode_t = uint32_t;

  struct Keyboard {
    Keyboard(Seat& seat, wlr::input_device_t& device);

    static constexpr const int pressed_keysyms_cap = 32;
    Seat& seat;

    wlr::input_device_t& device;
    Config::Keyboard config;

    wl::Listener on_device_destroy;
    wl::Listener on_keyboard_key;
    wl::Listener on_keyboard_modifiers;

    xkb_keysym_t pressed_keysyms_translated[pressed_keysyms_cap] = {0};
    xkb_keysym_t pressed_keysyms_raw[pressed_keysyms_cap] = {0};

    void execute_user_binding(std::string_view command);

  private:
    bool execute_compositor_binding(xkb_keysym_t keysym);

    bool execute_binding(xkb_keysym_t* pressed_keysyms,
                         uint32_t modifiers,
                         const xkb_keysym_t* keysyms,
                         size_t keysyms_len);
    std::size_t keysyms_translated(xkb_keycode_t keycode,
                                   const xkb_keysym_t** keysyms,
                                   uint32_t* modifiers);
    std::size_t keysyms_raw(xkb_keycode_t keycode,
                            const xkb_keysym_t** keysyms,
                            uint32_t* modifiers);

    void handle_key(wlr::event_keyboard_key_t& event);
    void handle_modifiers();
  };

} // namespace cloth
