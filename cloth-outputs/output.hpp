#pragma once

#include <gtkmm.h>

#include <protocols.hpp>
#include <wayland-client.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

#include <fmt/format.h>

namespace cloth::outputs {

  using namespace std::literals;

  template<typename... Args>
  auto send_sway_cmd(std::string cmd, Args&&... args)
  {
    auto str = "swaymsg "s + fmt::format(cmd, args...);
    cloth_debug("EXEC: {}", str);
    std::system(str.c_str());
  }

  namespace wl = wayland;
  struct Client;

  struct Position {
    int x = 0;
    int y = 0;
  };

  struct Size {
    int width = 0;
    int height = 0;
  };

  struct Output {
    Output(Client& client, std::unique_ptr<wl::output_t>&& output);

    Client& client;
    std::unique_ptr<wl::output_t> output;
    wl::zxdg_output_v1_t xdg_output;

    Position logical_position;
    Size logical_size;
    int scale = 0;
    std::string name;
    std::string description;
    bool done = false;

    auto set_position(Position p) -> void;
    auto set_size(Size s) -> void;
    auto toggle() -> void;
  };

} // namespace cloth::outputs
