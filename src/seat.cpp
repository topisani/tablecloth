#include "seat.hpp"

#include "util/logging.hpp"

namespace cloth {

  void Seat::add_device(wlr::input_device_t& device) noexcept 
  {
    switch (device.type) {
      case WLR_INPUT_DEVICE_KEYBOARD:
        add_keyboard(device);
        break;
      case WLR_INPUT_DEVICE_POINTER:
        add_pointer(device);
        break;  
      case WLR_INPUT_DEVICE_TOUCH:
        add_touch(device);
        break;  
      case WLR_INPUT_DEVICE_TABLET_PAD:
        add_tablet_pad(device);
        break;  
      case WLR_INPUT_DEVICE_TABLET_TOOL:
        add_tablet_tool(device);
        break;  
    }

    update_capabilities();
  }

  void Seat::add_keyboard(wlr::input_device_t& device) noexcept
  {
    assert(device.type == WLR_INPUT_DEVICE_KEYBOARD);
    auto& keyboard = keyboards.emplace_back(device, input);

    keyboard.device_destroy = [&] (void* data) {
      keyboards.erase(keyboards.begin() + (&keyboard - keyboards.data()));
      update_capabilities();
    };
    keyboard.device_destroy.add_to(keyboard.device->events.destroy);
    keyboard.keyboard_key = [&] (void* data) {
      [[maybe_unused]]
      auto& event = *(wlr::event_keyboard_key_t*) data;
    };
    keyboard.keyboard_key.add_to(keyboard.device->events.destroy);
    keyboard.keyboard_modifiers = [&] (void* data) {
    };
    keyboard.keyboard_modifiers.add_to(keyboard.device->events.destroy);  
  }

  void Seat::update_capabilities() noexcept {
    uint32_t caps = 0;
    if (!keyboards.empty()) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (!pointers.empty() || !tablet_tools.empty()) caps |= WL_SEAT_CAPABILITY_POINTER;
    if (!touch.empty()) caps |= WL_SEAT_CAPABILITY_TOUCH;
    wlr_seat_set_capabilities(base(), caps);

    if ((caps & WL_SEAT_CAPABILITY_POINTER) == 0) {
      wlr_cursor_set_image(cursor, nullptr, 0, 0, 0, 0, 0, 0);
    } else {
      wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, cursor.default_xcursor.c_str(), cursor);
    }
  }

}
