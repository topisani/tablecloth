#pragma once

#include <xkbcommon/xkbcommon.h>
#include <functional>
#include <string>
#include <vector>

#include "util/bindings.hpp"

#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif

namespace cloth::kbd {

  struct KeyboardState;

  enum struct LayerType { Letters, Numbers, Symbols };
  enum struct Modifiers {
    None = 0,
    Shift = 0b00001,
    Caps = 0b00010,
    Ctrl = 0b00100,
    Alt = 0b01000,
    Super = 0b10000
  };

  struct Key {
    Key(std::string label,
        std::function<void(KeyboardState&)> press_func,
        std::function<void(KeyboardState&)> release_func,
        int width);
    Key(std::string label, xkb_keysym_t key, int width);
    Key(std::string label, Modifiers modifier, int width);
    Key(std::string label, LayerType layer, int width);
    std::string label;
    std::function<void(KeyboardState&)> press_func;
    std::function<void(KeyboardState&)> release_func;
    int width;
  };

  using Row = std::vector<Key>;
  using Layer = std::vector<Row>;

  inline auto handle_ignore(KeyboardState&) noexcept -> void {};
  auto do_quit(KeyboardState&) -> void;

  inline const Layer letters_layer = {
    {
      {"Q", KEY_Q, 1},
      {"W", KEY_W, 1},
      {"E", KEY_E, 1},
      {"R", KEY_R, 1},
      {"T", KEY_T, 1},
      {"Y", KEY_Y, 1},
      {"U", KEY_U, 1},
      {"I", KEY_I, 1},
      {"O", KEY_O, 1},
      {"P", KEY_P, 1},
      {"Backscape", KEY_BACKSPACE, 2},
    },
    {
      {"TAB", KEY_TAB, 1},
      {"A", KEY_A, 1},
      {"S", KEY_S, 1},
      {"D", KEY_D, 1},
      {"F", KEY_F, 1},
      {"G", KEY_G, 1},
      {"H", KEY_H, 1},
      {"J", KEY_J, 1},
      {"K", KEY_K, 1},
      {"L", KEY_L, 1},
      {"Enter", KEY_ENTER, 2},
    },
    {
      {"Shift", Modifiers::Shift, 2},
      {"Z", KEY_Z, 1},
      {"X", KEY_X, 1},
      {"C", KEY_C, 1},
      {"V", KEY_V, 1},
      {"B", KEY_B, 1},
      {"N", KEY_N, 1},
      {"M", KEY_M, 1},
      {",", KEY_COMMA, 1},
      {".", KEY_DOT, 1},
      {"Close", do_quit, handle_ignore, 1},
    },
    {
      {"?123", LayerType::Numbers, 1},
      {"", KEY_SPACE, 5},
      {"←", KEY_LEFT, 1},
      {"↑", KEY_UP, 1},
      {"↓", KEY_DOWN, 1},
      {"→", KEY_RIGHT, 1},
      {"ctrl", Modifiers::Ctrl, 2},
    },
  };

  inline const Layer number_layer = {
    {
      {"1", KEY_1, 1},
      {"2", KEY_2, 1},
      {"3", KEY_3, 1},
      {"4", KEY_4, 1},
      {"5", KEY_5, 1},
      {"6", KEY_6, 1},
      {"7", KEY_7, 1},
      {"8", KEY_8, 1},
      {"9", KEY_9, 1},
      {"0", KEY_0, 1},
      {"Backscape", KEY_BACKSPACE, 2},
    },
    {
      {"TAB", KEY_TAB, 1},
      {"-", KEY_MINUS, 1},
      {"@", KEY_0, 1},
      {"*", KEY_0, 1},
      {"^", KEY_0, 1},
      {":", KEY_0, 1},
      {";", KEY_SEMICOLON, 1},
      {"(", KEY_KPLEFTPAREN, 1},
      {")", KEY_KPRIGHTPAREN, 1},
      {"~", KEY_0, 1},
      {"Enter", KEY_ENTER, 2},
    },
    {
      {"Shift", Modifiers::Shift, 2},
      {"/", KEY_SLASH, 1},
      {"'", KEY_APOSTROPHE, 1},
      {"\"", KEY_0, 1},
      {"+", KEY_KPPLUS, 1},
      {"=", KEY_EQUAL, 1},
      {"?", KEY_QUESTION, 1},
      {"!", KEY_0, 1},
      {"\\", KEY_BACKSLASH, 1},
      {"|", KEY_0, 1},
      {"Close", do_quit, handle_ignore, 1},
    },
    {
      {"abc", LayerType::Letters, 1},
      {"", KEY_SPACE, 5},
      {"←", KEY_LEFT, 1},
      {"↑", KEY_UP, 1},
      {"↓", KEY_DOWN, 1},
      {"→", KEY_RIGHT, 1},
      {"ctrl", Modifiers::Ctrl, 2},
    },
  };

} // namespace cloth::kbd

namespace cloth {
  CLOTH_ENABLE_BITMASK_OPS(cloth::kbd::Modifiers)
}
