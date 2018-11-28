#pragma once

#include <vector>
#include <string_view>

#include <xf86drmMode.h>
#include "wlroots.hpp"

#include "util/algorithm.hpp"

namespace cloth {

  struct ArgList {
    explicit ArgList(std::string_view str) : _str(str), _first(str.begin()), _last(str.begin()) {}

    auto peek() -> std::string_view;
    auto next() -> std::string_view;
    auto current() -> std::string_view;
    auto data() -> std::string_view;
    auto remaining() -> std::string_view;

  private:
    std::string_view _str;
    std::string_view::iterator _first;
    std::string_view::iterator _last;
  };

  struct Config {

    inline static const std::string default_seat_name = "seat0";

    struct OutputMode {
      drmModeModeInfo info;
    };

    struct Output {
      std::string name;
      bool enable = true;
      wl::output_transform_t transform = WL_OUTPUT_TRANSFORM_NORMAL;
      int x = 0, y = 0;
      float scale = 1;
      struct {
        int width, height;
        float refresh_rate;
      } mode;
      std::vector<OutputMode> modes;
    };

    struct Device {
      std::string name;
      std::string seat;
      std::string mapped_output;
      bool tap_enabled = true;
      bool natural_scroll = true;
      wlr::box_t mapped_box;
    };

    struct KeyCombo {
      uint32_t modifiers = 0;
      std::vector<xkb_keysym_t> keys;
    };

    struct Binding {
      KeyCombo combo;
      std::string command;
    };

    struct Keyboard {
      std::string name;
      std::string seat;
      uint32_t meta_key = 0;
      std::string rules;
      std::string model;
      std::string layout;
      std::string variant;
      std::string options;
      int repeat_rate = 0, repeat_delay = 0;
    };

    struct Cursor {
      std::string seat;
      std::string mapped_output;
      wlr::box_t mapped_box;
      std::string theme;
      std::string default_image;
    };

    Config() noexcept {};

    /// Create a roots config from the given command line arguments. Command line
    /// arguments can specify the location of the config file. If it is not
    /// specified, the default location will be used.
    Config(int argc, char* argv[]) noexcept;

    /// Destroy the config and free its resources.
    ~Config() noexcept;

    /// Get configuration for the output. If the output is not configured, returns
    /// NULL.
    Config::Output* get_output(wlr::output_t& output) noexcept;

    /// Get configuration for the device. If the device is not configured, returns
    /// NULL.
    Config::Device* get_device(wlr::input_device_t& device) noexcept;

    /// Get configuration for the keyboard. If the keyboard is not configured,
    /// returns NULL. A NULL device returns the default config for keyboards.
    Config::Keyboard* get_keyboard(wlr::input_device_t* device) noexcept;

    /// Get configuration for the cursor. If the cursor is not configured, returns
    /// NULL. A NULL seat_name returns the default config for cursors.
    Config::Cursor* get_cursor(std::string_view seat_name = default_seat_name) noexcept;

    auto run_command(std::string_view command) -> void;

    bool xwayland = true;
    bool xwayland_lazy = false;

    std::vector<Output> outputs;
    std::vector<Device> devices;
    std::vector<Binding> bindings;
    std::vector<Keyboard> keyboards;
    std::vector<Cursor> cursors;

    std::string config_path;
    std::string startup_cmd;
    bool debug_damage_tracking = false;
  };

  inline bool operator==(const Config::KeyCombo& lhs, const Config::KeyCombo& rhs) {
    return lhs.modifiers == rhs.modifiers && std::equal(lhs.keys.begin(), lhs.keys.end(), rhs.keys.begin(), rhs.keys.end());
  }

} // namespace cloth
