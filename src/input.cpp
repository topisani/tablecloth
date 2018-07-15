#include "input.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"

#include <string>

namespace cloth {

  static const char* to_string(enum wlr_input_device_type type)
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

  Input::Input(Server& server) noexcept : server(server)
  {
    LOGD("Initializing Input");

    new_input = [this](void* data) {
      auto& device = *(wlr::input_device_t*) data;
      std::string seat_name = "seat0";

      auto& seat = get_seat(seat_name);

      LOGD("New input device: {} ({}:{}) {} seat: {}", device.name, device.vendor, device.product,
           to_string(device.type), seat_name);

      seat.add_device(device);
    };
    new_input.add_to(server.backend->events.new_input);
  }

  Seat& Input::get_seat(std::string_view name) noexcept
  {
    auto found = util::find_if(seats, [name](Seat& s) { return s.name == name; });

    if (found != seats.end()) return *found;
    return seats.emplace_back(*this, name);
  }
} // namespace cloth
