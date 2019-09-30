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

    bool operator==(const Position& rhs) const
    {
      return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const Position& rhs) const
    {
      return !(*this == rhs);
    }
  };

  struct Size {
    int width = 0;
    int height = 0;

    bool operator==(const Size& rhs) const
    {
      return width == rhs.width && height == rhs.height;
    }

    bool operator!=(const Size& rhs) const
    {
      return !(*this == rhs);
    }
  };

  struct Mode {
    bool preferred;
    bool current;
    int width = 0;
    int height = 0;
    int refresh = 0;
  };

  enum struct Transform {
    normal,
    /** \brief 90 degrees counter-clockwise */
    _90,
    /** \brief 180 degrees counter-clockwise */
    _180,
    /** \brief 270 degrees counter-clockwise */
    _270,
    /** \brief 180 degree flip around a vertical axis */
    flipped,
    /** \brief flip and rotate 90 degrees counter-clockwise */
    flipped_90,
    /** \brief flip and rotate 180 degrees counter-clockwise */
    flipped_180,
    /** \brief flip and rotate 270 degrees counter-clockwise */
    flipped_270,
  };

  std::string to_string(Transform t);
  Transform transform_from_string(std::string s);

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
    Transform transform = Transform::normal;
    bool done = false;

    std::vector<Mode> avaliable_modes;

    auto set_position(Position p) -> void;
    auto set_size(Size s) -> void;
    auto set_mode(Mode) -> void;
    auto set_transform(Transform) -> void;
    auto toggle() -> void;
  };

} // namespace cloth::outputs
