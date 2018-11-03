#include "keyboard.hpp"

#include <xkbcommon/xkbcommon.h>
#include "util/algorithm.hpp"
#include "util/exception.hpp"
#include "util/logging.hpp"

#include "input.hpp"
#include "keyboard.hpp"
#include "seat.hpp"
#include "server.hpp"

namespace cloth {

  static ssize_t pressed_keysyms_index(xkb_keysym_t* pressed_keysyms, xkb_keysym_t keysym)
  {
    for (size_t i = 0; i < Keyboard::pressed_keysyms_cap; ++i) {
      if (pressed_keysyms[i] == keysym) {
        return i;
      }
    }
    return -1;
  }

  static size_t pressed_keysyms_length(xkb_keysym_t* pressed_keysyms)
  {
    size_t n = 0;
    for (size_t i = 0; i < Keyboard::pressed_keysyms_cap; ++i) {
      if (pressed_keysyms[i] != XKB_KEY_NoSymbol) {
        ++n;
      }
    }
    return n;
  }

  static void pressed_keysyms_add(xkb_keysym_t* pressed_keysyms, xkb_keysym_t keysym)
  {
    ssize_t i = pressed_keysyms_index(pressed_keysyms, keysym);
    if (i < 0) {
      i = pressed_keysyms_index(pressed_keysyms, XKB_KEY_NoSymbol);
      if (i >= 0) {
        pressed_keysyms[i] = keysym;
      }
    }
  }

  static void pressed_keysyms_remove(xkb_keysym_t* pressed_keysyms, xkb_keysym_t keysym)
  {
    ssize_t i = pressed_keysyms_index(pressed_keysyms, keysym);
    if (i >= 0) {
      pressed_keysyms[i] = XKB_KEY_NoSymbol;
    }
  }

  static bool keysym_is_modifier(xkb_keysym_t keysym)
  {
    switch (keysym) {
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R:
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R:
    case XKB_KEY_Caps_Lock:
    case XKB_KEY_Shift_Lock:
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R:
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
    case XKB_KEY_Super_L:
    case XKB_KEY_Super_R:
    case XKB_KEY_Hyper_L:
    case XKB_KEY_Hyper_R: return true;
    default: return false;
    }
  }

  static void pressed_keysyms_update(xkb_keysym_t* pressed_keysyms,
                                     const xkb_keysym_t* keysyms,
                                     size_t keysyms_len,
                                     enum wlr_key_state state)
  {
    for (size_t i = 0; i < keysyms_len; ++i) {
      if (keysym_is_modifier(keysyms[i])) {
        continue;
      }
      if (state == WLR_KEY_PRESSED) {
        pressed_keysyms_add(pressed_keysyms, keysyms[i]);
      } else { // WLR_KEY_RELEASED
        pressed_keysyms_remove(pressed_keysyms, keysyms[i]);
      }
    }
  }

  void Keyboard::execute_user_binding(std::string_view command_str)
  {
    seat.input.server.desktop.run_command(command_str);
  }

  /// Execute a built-in, hardcoded compositor binding. These are triggered from a
  /// single keysym.
  ///
  /// Returns true if the keysym was handled by a binding and false if the event
  /// should be propagated to clients.
  bool Keyboard::execute_compositor_binding(xkb_keysym_t keysym)
  {
    if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
      Server& server = seat.input.server;

      struct wlr_session* session = wlr_backend_get_session(server.backend);
      if (session) {
        unsigned vt = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(session, vt);
      }
      return true;
    }

    if (keysym == XKB_KEY_Escape) {
      wlr_seat_pointer_end_grab(seat.wlr_seat);
      wlr_seat_keyboard_end_grab(seat.wlr_seat);
      seat.end_compositor_grab();
    }

    return false;
  }

  /// Execute keyboard bindings. These include compositor bindings and user-defined
  /// bindings.
  ///
  /// Returns true if the keysym was handled by a binding and false if the event
  /// should be propagated to clients.
  bool Keyboard::execute_binding(xkb_keysym_t* pressed_keysyms,
                                 uint32_t modifiers,
                                 const xkb_keysym_t* keysyms,
                                 size_t keysyms_len)
  {
    for (size_t i = 0; i < keysyms_len; ++i) {
      if (execute_compositor_binding(keysyms[i])) return true;
    }

    // An input inhibitor is enabled
    if (seat.exclusive_client) return false;

    // User-defined bindings
    size_t n = pressed_keysyms_length(pressed_keysyms);
    auto& bindings = seat.input.server.config.bindings;
    for (auto& binding : bindings) {
      if (modifiers ^ binding.modifiers || n != binding.keysyms.size()) {
        continue;
      }

      bool ok = true;
      for (auto sym : binding.keysyms) {
        ssize_t j = pressed_keysyms_index(pressed_keysyms, sym);
        if (j < 0) {
          ok = false;
          break;
        }
      }

      if (ok) {
        execute_user_binding(binding.command);
        return true;
      }
    }

    return false;
  }

  /// Get keysyms and modifiers from the keyboard as xkb sees them.
  ///
  /// This uses the xkb keysyms translation based on pressed modifiers and clears
  /// the consumed modifiers from the list of modifiers passed to keybind
  /// detection.
  ///
  /// On US layout, pressing Alt+Shift+2 will trigger Alt+@.
  std::size_t Keyboard::keysyms_translated(xkb_keycode_t keycode,
                                           const xkb_keysym_t** keysyms,
                                           uint32_t* modifiers)
  {
    *modifiers = wlr_keyboard_get_modifiers(wlr_device.keyboard);
    xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(wlr_device.keyboard->xkb_state,
                                                               keycode, XKB_CONSUMED_MODE_XKB);
    *modifiers = *modifiers & ~consumed;

    return xkb_state_key_get_syms(wlr_device.keyboard->xkb_state, keycode, keysyms);
  }

  /// Get keysyms and modifiers from the keyboard as if modifiers didn't change
  /// keysyms.
  ///
  /// This avoids the xkb keysym translation based on modifiers considered pressed
  /// in the state.
  ///
  /// This will trigger keybinds such as Alt+Shift+2.
  std::size_t Keyboard::keysyms_raw(xkb_keycode_t keycode,
                                    const xkb_keysym_t** keysyms,
                                    uint32_t* modifiers)
  {
    *modifiers = wlr_keyboard_get_modifiers(wlr_device.keyboard);

    xkb_layout_index_t layout_index =
      xkb_state_key_get_layout(wlr_device.keyboard->xkb_state, keycode);
    return xkb_keymap_key_get_syms_by_level(wlr_device.keyboard->keymap, keycode, layout_index, 0,
                                            keysyms);
  }

  void Keyboard::handle_key(wlr::event_keyboard_key_t& event)
  {
    xkb_keycode_t keycode = event.keycode + 8;

    bool handled = false;
    uint32_t modifiers;
    const xkb_keysym_t* keysyms;

    // Handle translated keysyms

    std::size_t keysyms_len = keysyms_translated(keycode, &keysyms, &modifiers);
    pressed_keysyms_update(pressed_keysyms_translated, keysyms, keysyms_len, event.state);
    if (event.state == WLR_KEY_RELEASED) {
      handled = execute_binding(pressed_keysyms_translated, modifiers, keysyms, keysyms_len);
    }

    // Handle raw keysyms
    keysyms_len = keysyms_raw(keycode, &keysyms, &modifiers);
    pressed_keysyms_update(pressed_keysyms_raw, keysyms, keysyms_len, event.state);
    if (event.state == WLR_KEY_PRESSED && !handled) {
      handled = execute_binding(pressed_keysyms_raw, modifiers, keysyms, keysyms_len);
    }

    if (!handled) {
      wlr_seat_set_keyboard(seat.wlr_seat, &wlr_device);
      wlr_seat_keyboard_notify_key(seat.wlr_seat, event.time_msec, event.keycode, event.state);
    }
  }

  void Keyboard::handle_modifiers()
  {
    wlr_seat_set_keyboard(seat.wlr_seat, &wlr_device);
    wlr_seat_keyboard_notify_modifiers(seat.wlr_seat, &wlr_device.keyboard->modifiers);
  }

  static void keyboard_config_merge(Config::Keyboard& config, Config::Keyboard* fallback)
  {
    if (fallback == nullptr) return;
    if (config.rules.empty()) {
      config.rules = fallback->rules;
    }
    if (config.model.empty()) {
      config.model = fallback->model;
    }
    if (config.layout.empty()) {
      config.layout = fallback->layout;
    }
    if (config.variant.empty()) {
      config.variant = fallback->variant;
    }
    if (config.options.empty()) {
      config.options = fallback->options;
    }
    if (config.meta_key == 0) {
      config.meta_key = fallback->meta_key;
    }
    if (config.name.empty()) {
      config.name = fallback->name;
    }
    if (config.repeat_rate <= 0) {
      config.repeat_rate = fallback->repeat_rate;
    }
    if (config.repeat_delay <= 0) {
      config.repeat_delay = fallback->repeat_delay;
    }
  }

  Keyboard::Keyboard(Seat& seat, wlr::input_device_t& device) : Device(seat, device)
  {
    assert(device.type == WLR_INPUT_DEVICE_KEYBOARD);

    device.data = this;

    keyboard_config_merge(config, seat.input.config.get_keyboard(&device));
    keyboard_config_merge(config, seat.input.config.get_keyboard(nullptr));

    Config::Keyboard env_config = {
      .rules = util::nonull(getenv("XKB_DEFAULT_RULES")),
      .model = util::nonull(getenv("XKB_DEFAULT_MODEL")),
      .layout = util::nonull(getenv("XKB_DEFAULT_LAYOUT")),
      .variant = util::nonull(getenv("XKB_DEFAULT_VARIANT")),
      .options = util::nonull(getenv("XKB_DEFAULT_OPTIONS")),
    };
    keyboard_config_merge(config, &env_config);

    struct xkb_rule_names rules = {0};
    rules.rules = config.rules.c_str();
    rules.model = config.model.c_str();
    rules.layout = config.layout.c_str();
    rules.variant = config.variant.c_str();
    rules.options = config.options.c_str();

    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (context == nullptr) {
      throw util::exception("Cannot create XKB context");
    }

    struct xkb_keymap* keymap =
      xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap == nullptr) {
      xkb_context_unref(context);
      throw util::exception("Cannot create XKB keymap");
    }

    wlr_keyboard_set_keymap(device.keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    int repeat_rate = (config.repeat_rate > 0) ? config.repeat_rate : 25;
    int repeat_delay = (config.repeat_delay > 0) ? config.repeat_delay : 600;
    wlr_keyboard_set_repeat_info(device.keyboard, repeat_rate, repeat_delay);

    on_keyboard_key.add_to(device.keyboard->events.key);
    on_keyboard_key = [this](void* data) {
      Desktop& desktop = this->seat.input.server.desktop;
      wlr_idle_notify_activity(desktop.idle, this->seat.wlr_seat);
      auto& event = *(wlr::event_keyboard_key_t*) data;
      handle_key(event);
    };

    on_keyboard_modifiers.add_to(device.keyboard->events.modifiers);
    on_keyboard_modifiers = [this] {
      wlr_idle_notify_activity(this->seat.input.server.desktop.idle, this->seat.wlr_seat);
      handle_modifiers();
    };

    on_device_destroy.add_to(device.events.destroy);
    on_device_destroy = [this] {
      cloth_debug("Keyboard destroyed");
      auto keep_around = util::erase_this(this->seat.keyboards, this);
      this->seat.update_capabilities();
    };
  }


} // namespace cloth
