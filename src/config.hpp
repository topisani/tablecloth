#pragma once

#include <vector>
#include <string_view>

#include <xf86drmMode.h>
#include "wlroots.hpp"

namespace cloth {

  struct Config {

    static constexpr std::string_view default_seat_name = "seat0";

    struct OutputMode {
      drmModeModeInfo info;
    };

    struct Output {
      std::string name;
      bool enable;
      wl::output_transform_t transform;
      int x, y;
      float scale;
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
      bool tap_enabled;
      wlr::box_t mapped_box;
    };

    struct Binding {
      uint32_t modifiers;
      std::vector<xkb_keysym_t> keysyms;
      size_t keysyms_len;
      std::string command;
    };

    struct Keyboard {
      std::string name;
      std::string seat;
      uint32_t meta_key;
      std::string rules;
      std::string model;
      std::string layout;
      std::string variant;
      std::string options;
      int repeat_rate, repeat_delay;
    };

    struct Cursor {
      std::string seat;
      std::string mapped_output;
      wlr::box_t mapped_box;
      std::string theme;
      std::string default_image;
    };

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

    bool xwayland;
    bool xwayland_lazy;

    std::vector<Output> outputs;
    std::vector<Device> devices;
    std::vector<Binding> bindings;
    std::vector<Keyboard> keyboards;
    std::vector<Cursor> cursors;

    std::string config_path;
    std::string startup_cmd;
    bool debug_damage_tracking;
  };

} // namespace cloth
