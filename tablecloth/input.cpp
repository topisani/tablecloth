#include <assert.h>
#include <stdlib.h>

#include <any>

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "wlroots.hpp"

#include "config.hpp"
#include "input.hpp"
#include "keyboard.hpp"
#include "seat.hpp"
#include "server.hpp"

namespace cloth {

  static const char* device_type(enum wlr_input_device_type type)
  {
    switch (type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return "keyboard";
    case WLR_INPUT_DEVICE_POINTER: return "pointer";
    case WLR_INPUT_DEVICE_TOUCH: return "touch";
    case WLR_INPUT_DEVICE_TABLET_TOOL: return "tablet tool";
    case WLR_INPUT_DEVICE_TABLET_PAD: return "tablet pad";
    }
    return NULL;
  }

  Input::Input(Server& p_server, Config& p_config) noexcept : server(p_server), config(p_config)
  {
    LOGD("Initializing roots input");

    on_new_input = [this](void* data) {
      auto* device = (wlr::input_device_t*) data;

      auto seat_name = Config::default_seat_name;
      auto* dc = config.get_device(*device);
      if (dc) {
        seat_name = dc->seat;
      }

      auto& seat = get_seat(seat_name);
      LOGD("New input device: {} ({}:{}) {} seat:{}", device->name, device->vendor, device->product,
           device_type(device->type), seat_name);

      seat.add_device(*device);

      if (dc && wlr_input_device_is_libinput(device)) {
        struct libinput_device* libinput_dev = wlr_libinput_get_device_handle(device);

        LOGD("input has config, tap_enabled: {}", dc->tap_enabled);
        ;
        if (dc->tap_enabled) {
          libinput_device_config_tap_set_enabled(libinput_dev, LIBINPUT_CONFIG_TAP_ENABLED);
        }
      }
    };
    on_new_input.add_to(server.backend->events.new_input);
  }

  Input::~Input() noexcept
  {
    // TODO
  }

  Seat* Input::last_active_seat()
  {
    Seat* _seat = nullptr;
    for (auto& seat : seats) {
      if (!_seat || (_seat->wlr_seat->last_event.tv_sec > seat.wlr_seat->last_event.tv_sec &&
                     _seat->wlr_seat->last_event.tv_nsec > seat.wlr_seat->last_event.tv_nsec)) {
        _seat = &seat;
      }
    }
    return _seat;
  }

  Seat& Input::create_seat(const std::string& name)
  {
    return seats.emplace_back(*this, name);
  }

  Seat& Input::get_seat(const std::string& name)
  {
    for (auto& seat : seats) {
      if (seat.wlr_seat->name == name) return seat;
    }
    return create_seat(name);
  }

  Seat* Input::seat_from_wlr_seat(wlr::seat_t& wlr_seat)
  {
    for (auto& seat : seats) {
      if (seat.wlr_seat == &wlr_seat) return &seat;
    }
    return nullptr;
  }

  bool Input::view_has_focus(View& view)
  {
    for (auto& seat : seats) {
      if (&view == seat.get_focus()) return true;
    }
    return false;
  }

  void Input::update_cursor_focus()
  {
    for (auto& seat : seats) {
      seat.cursor.update_position(chrono::duration_cast<chrono::milliseconds>(chrono::clock::now().time_since_epoch()).count());
    };
  }

} // namespace cloth
