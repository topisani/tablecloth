#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/param.h>
#include <unistd.h>
#include <cstring>
#include <functional>

#include "util/algorithm.hpp"
#include "util/exception.hpp"
#include "util/logging.hpp"

#include "wlroots.hpp"

#include "config.hpp"
#include "ini.hpp"
#include "input.hpp"
#include "keyboard.hpp"

namespace cloth {

  namespace {
    enum struct ErrorCode { invalid_input };

    using exception = util::as_exception<ErrorCode>;

    void usage(const char* name, int ret)
    {
      fprintf(stderr,
              "usage: %s [-C <FILE>] [-E <COMMAND>]\n"
              "\n"
              " -C <FILE>      Path to the configuration file\n"
              "                (default: tablecloth.ini).\n"
              "                See `tablecloth.ini.example` for config\n"
              "                file documentation.\n"
              " -E <COMMAND>   Command that will be ran at startup.\n"
              " -D             Enable damage tracking debugging.\n",
              name);

      exit(ret);
    }

    static wlr::box_t parse_geometry(std::string_view str)
    {
      // format: {width}x{height}+{x}+{y}
      std::string buf(str);
      wlr::box_t box;

      bool has_width = false;
      bool has_height = false;
      bool has_x = false;
      bool has_y = false;

      char* pch = std::strtok(buf.data(), "x+");
      while (pch != nullptr) {
        errno = 0;
        char* endptr;
        long val = strtol(pch, &endptr, 0);

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
          throw exception(ErrorCode::invalid_input, "Invalid input: {}", str);
        }

        if (endptr == pch) {
          throw exception(ErrorCode::invalid_input, "Invalid input: {}", str);
        }

        if (!has_width) {
          box.width = val;
          has_width = true;
        } else if (!has_height) {
          box.height = val;
          has_height = true;
        } else if (!has_x) {
          box.x = val;
          has_x = true;
        } else if (!has_y) {
          box.y = val;
          has_y = true;
        } else {
          break;
        }
        pch = strtok(nullptr, "x+");
      }

      if (!has_width || !has_height) {
        throw exception(ErrorCode::invalid_input, "Invalid input: {}", str);
      }

      return box;
    }

    unsigned parse_modifier(std::string_view symname)
    {
      if (symname == "Shift") {
        return WLR_MODIFIER_SHIFT;
      } else if (symname == "Caps") {
        return WLR_MODIFIER_CAPS;
      } else if (symname == "Ctrl") {
        return WLR_MODIFIER_CTRL;
      } else if (symname == "Alt") {
        return WLR_MODIFIER_ALT;
      } else if (symname == "Mod2") {
        return WLR_MODIFIER_MOD2;
      } else if (symname == "Mod3") {
        return WLR_MODIFIER_MOD3;
      } else if (symname == "Logo") {
        return WLR_MODIFIER_LOGO;
      } else if (symname == "Mod5") {
        return WLR_MODIFIER_MOD5;
      } else {
        return 0;
      }
    }

    bool parse_modeline(const char* s, drmModeModeInfo* mode)
    {
      char hsync[16];
      char vsync[16];
      float fclock;

      mode->type = DRM_MODE_TYPE_USERDEF;

      if (sscanf(s, "%f %hd %hd %hd %hd %hd %hd %hd %hd %15s %15s", &fclock, &mode->hdisplay,
                 &mode->hsync_start, &mode->hsync_end, &mode->htotal, &mode->vdisplay,
                 &mode->vsync_start, &mode->vsync_end, &mode->vtotal, hsync, vsync) != 11) {
        return false;
      }

      mode->clock = fclock * 1000;
      mode->vrefresh = mode->clock * 1000.0 * 1000.0 / mode->htotal / mode->vtotal;
      if (util::iequals(hsync, "+hsync")) {
        mode->flags |= DRM_MODE_FLAG_PHSYNC;
      } else if (util::iequals(hsync, "-hsync")) {
        mode->flags |= DRM_MODE_FLAG_NHSYNC;
      } else {
        return false;
      }

      if (util::iequals(vsync, "+vsync")) {
        mode->flags |= DRM_MODE_FLAG_PVSYNC;
      } else if (util::iequals(vsync, "-vsync")) {
        mode->flags |= DRM_MODE_FLAG_NVSYNC;
      } else {
        return false;
      }

      snprintf(mode->name, sizeof(mode->name), "%dx%d@%d", mode->hdisplay, mode->vdisplay,
               mode->vrefresh / 1000);

      return true;
    }

    Config::KeyCombo parse_key_combo(ArgList& args)
    {
      Config::KeyCombo res;
      auto symnames = std::string(args.next());
      char* symname = strtok(symnames.data(), "+");
      while (symname) {
        uint32_t modifier = parse_modifier(symname);
        if (modifier != 0) {
          res.modifiers |= modifier;
        } else {
          xkb_keysym_t sym = xkb_keysym_from_name(symname, XKB_KEYSYM_NO_FLAGS);
          if (sym == XKB_KEY_NoSymbol) {
            throw util::exception("got unknown key binding symbol: {}", symname);
          }
          res.keys.push_back(sym);
        }
        symname = strtok(nullptr, "+");
      }
      return res;
    }

    Config::Binding parse_binding(ArgList& args)
    {
      return Config::Binding{parse_key_combo(args), std::string(args.remaining())};
    } // namespace

    void add_binding_config(Config& config, std::string_view combination, std::string_view command)
    {
      std::string tmp = fmt::format("{} {}", combination, command);
      ArgList args = ArgList(tmp);
      config.bindings.push_back(parse_binding(args));
    }

    void config_handle_cursor(Config& config,
                              std::string_view seat_name,
                              std::string_view name,
                              std::string_view value)
    {
      auto* found = &*util::find_if(config.cursors, [&](auto& c) { return c.seat == seat_name; });

      if (found == &*config.cursors.end()) {
        found = &config.cursors.emplace_back();
        found->seat = seat_name;
      }

      if (name == "map-to-output") {
        found->mapped_output = value;
      } else if (name == "geometry") {
        found->mapped_box = parse_geometry(value);
      } else if (name == "theme") {
        found->theme = value;
      } else if (name == "default-image") {
        found->default_image = value;
      } else {
        cloth_error("got unknown cursor config: {}", name);
      }
    }

    void config_handle_keyboard(Config& config,
                                std::string_view device_name,
                                std::string_view name,
                                std::string_view value)
    {
      auto* found =
        &*util::find_if(config.keyboards, [&](auto& c) { return c.seat == device_name; });

      if (found == &*config.keyboards.end()) {
        found = &config.keyboards.emplace_back();
        found->name = device_name;
      }

      std::string val_str = std::string{value};

      if (name == "meta-key") {
        found->meta_key = parse_modifier(value);
        if (found->meta_key == 0) {
          cloth_error("got unknown meta key: {}", name);
        }
      } else if (name == "rules") {
        found->rules = value;
      } else if (name == "model") {
        found->model = value;
      } else if (name == "layout") {
        found->layout = value;
      } else if (name == "variant") {
        found->variant = value;
      } else if (name == "options") {
        found->options = value;
      } else if (name == "repeat-rate") {
        found->repeat_rate = std::strtol(val_str.c_str(), nullptr, 10);
      } else if (name == "repeat-delay") {
        found->repeat_delay = std::strtol(val_str.c_str(), nullptr, 10);
      } else {
        cloth_error("got unknown keyboard config: {}", name);
      }
    }

    static std::string_view output_prefix = "output:";
    static std::string_view device_prefix = "device:";
    static std::string_view keyboard_prefix = "keyboard:";
    static std::string_view cursor_prefix = "cursor:";

    int config_ini_handler(Config& config,
                           std::string_view section,
                           std::string_view name,
                           std::string_view value)
    {
      if (section == "core") {
        if (name == "xwayland") {
          if (util::iequals(value, "true")) {
            config.xwayland = true;
          } else if (util::iequals(value, "immediate")) {
            config.xwayland = true;
            config.xwayland_lazy = false;
          } else if (util::iequals(value, "false")) {
            config.xwayland = false;
          } else {
            cloth_error("got unknown xwayland value: {}", value);
          }
        } else {
          cloth_error("got unknown core config: {}", name);
        }
      } else if (util::starts_with(output_prefix, section)) {
        auto output_name = section.substr(output_prefix.size());

        auto* found =
          &*util::find_if(config.outputs, [&](auto& o) { return o.name == output_name; });

        if (found == &*config.outputs.end()) {
          found = &config.outputs.emplace_back();
          found->name = output_name;
          found->transform = WL_OUTPUT_TRANSFORM_NORMAL;
          found->scale = 1;
          found->enable = true;
        }

        auto val_str = std::string{value};

        if (name == "enable") {
          if (util::iequals(value, "true")) {
            found->enable = true;
          } else if (util::iequals(value, "false")) {
            found->enable = false;
          } else {
            cloth_error("got invalid output enable value: {}", value);
          }
        } else if (name == "x") {
          found->x = strtol(val_str.c_str(), nullptr, 10);
        } else if (name == "y") {
          found->y = std::strtol(val_str.c_str(), nullptr, 10);
        } else if (name == "scale") {
          found->scale = std::strtof(val_str.c_str(), nullptr);
          assert(found->scale > 0);
        } else if (name == "rotate") {
          if (value == "normal") {
            found->transform = WL_OUTPUT_TRANSFORM_NORMAL;
          } else if (value == "90") {
            found->transform = WL_OUTPUT_TRANSFORM_90;
          } else if (value == "180") {
            found->transform = WL_OUTPUT_TRANSFORM_180;
          } else if (value == "270") {
            found->transform = WL_OUTPUT_TRANSFORM_270;
          } else if (value == "flipped") {
            found->transform = WL_OUTPUT_TRANSFORM_FLIPPED;
          } else if (value == "flipped-90") {
            found->transform = WL_OUTPUT_TRANSFORM_FLIPPED_90;
          } else if (value == "flipped-180") {
            found->transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
          } else if (value == "flipped-270") {
            found->transform = WL_OUTPUT_TRANSFORM_FLIPPED_270;
          } else {
            cloth_error("got unknown transform value: {}", value);
          }
        } else if (name == "mode") {
          char* end;
          found->mode.width = std::strtol(val_str.c_str(), &end, 10);
          assert(*end == 'x');
          ++end;
          found->mode.height = std::strtol(end, &end, 10);
          if (*end) {
            assert(*end == '@');
            ++end;
            found->mode.refresh_rate = std::strtof(end, &end);
            assert("Hz" == end);
          }
          cloth_debug("Configured output {} with mode {}x{}@{}", found->name, found->mode.width,
                      found->mode.height, found->mode.refresh_rate);
        } else if (name == "modeline") {
          Config::OutputMode mode;
          if (parse_modeline(val_str.c_str(), &mode.info)) {
            found->modes.emplace_back(std::move(mode));
          } else {
            cloth_error("Invalid modeline: {}", value);
          }
        }
      } else if (util::starts_with(cursor_prefix, section)) {
        auto seat_name = section.substr(cursor_prefix.size());
        config_handle_cursor(config, seat_name, name, value);
      } else if (section == "cursor") {
        config_handle_cursor(config, Config::default_seat_name, name, value);
      } else if (util::starts_with(device_prefix, section)) {
        auto device_name = section.substr(device_prefix.size());

        auto* dc = &*util::find_if(config.devices, [&](auto& d) { return d.name == device_name; });

        if (dc == &*config.devices.end()) {
          dc = &config.devices.emplace_back();
          dc->name = device_name;
          dc->seat = Config::default_seat_name;
        }
        if (name == "map-to-output") {
          dc->mapped_output = value;
        } else if (name == "geometry") {
          dc->mapped_box = parse_geometry(value);
        } else if (name == "seat") {
          dc->seat = value;
        } else if (name == "tap_enabled") {
          if (util::iequals(value, "true")) {
            dc->tap_enabled = true;
          } else if (util::iequals(value, "false")) {
            dc->tap_enabled = false;
          } else {
            cloth_error("got unknown tap_enabled value: {}", value);
          }
        } else if (name == "natural_scroll") {
          if (util::iequals(value, "true")) {
            dc->natural_scroll = true;
          } else if (util::iequals(value, "false")) {
            dc->natural_scroll = false;
          } else {
            cloth_error("got unknown natural_scroll value: {}", value);
          }
        } else {
          cloth_error("got unknown device config: {}", name);
        }
      } else if (section == "keyboard") {
        config_handle_keyboard(config, "", name, value);
      } else if (util::starts_with(keyboard_prefix, section)) {
        auto device_name = section.substr(keyboard_prefix.size());
        config_handle_keyboard(config, device_name, name, value);
      } else if (section == "bindings") {
        add_binding_config(config, name, value);
      } else {
        cloth_error("got unknown config section: {}", section);
      }

      return 1;
    }

  } // namespace

  Config::Config(int argc, char* argv[]) noexcept : xwayland(true), xwayland_lazy(true)
  {
    int c;
    while ((c = getopt(argc, argv, "C:E:hD")) != -1) {
      switch (c) {
      case 'C': config_path = optarg; break;
      case 'E': startup_cmd = optarg; break;
      case 'D': debug_damage_tracking = true; break;
      case 'h':
      case '?': usage(argv[0], c != 'h');
      }
    }

    if (config_path.empty()) {
      // get the config path from the current directory
      char cwd[MAXPATHLEN];
      if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        char buf[MAXPATHLEN];
        if (snprintf(buf, MAXPATHLEN, "%s/%s", cwd, "tablecloth.ini") >= MAXPATHLEN) {
          cloth_error("config path too long");
          exit(1);
        }
        config_path = buf;
      } else {
        cloth_error("could not get cwd");
        exit(1);
      }
    }

    int result =
      ini_parse(config_path.c_str(),
                [](void* data, const char* section, const char* name, const char* value) {
                  return config_ini_handler(*(Config*) data, section, name, value);
                },
                this);

    if (result == -1) {
      cloth_debug("No config file found. Using sensible defaults.");
      add_binding_config(*this, "Logo+Shift+E", "exit");
      add_binding_config(*this, "Ctrl+q", "close");
      add_binding_config(*this, "Alt+Tab", "next_window");
      add_binding_config(*this, "Logo+Escape", "break_pointer_constraint");
      auto& kc = keyboards.emplace_back();
      kc.meta_key = WLR_MODIFIER_LOGO;
      kc.name = "";
    } else if (result == -2) {
      cloth_error("Could not allocate memory to parse config file");
      exit(1);
    } else if (result != 0) {
      cloth_error("Could not parse config file");
      exit(1);
    }
  }

  Config::~Config() noexcept {}


  Config::Output* Config::get_output(wlr::output_t& output) noexcept
  {
    auto found = util::find_if(outputs, [&](auto& el) { return el.name == output.name; });
    if (found != outputs.end()) return &*found;
    return nullptr;
  }

  Config::Device* Config::get_device(wlr::input_device_t& device) noexcept
  {
    auto found = util::find_if(devices, [&](auto& el) { return el.name == device.name; });
    if (found != devices.end()) return &*found;
    return nullptr;
  }

  Config::Keyboard* Config::get_keyboard(wlr::input_device_t* device) noexcept
  {
    auto name = device ? device->name : "";
    auto found = util::find_if(keyboards, [&](auto& el) { return el.name == name; });
    if (found != keyboards.end()) return &*found;
    return nullptr;
  }

  Config::Cursor* Config::get_cursor(std::string_view seat_name) noexcept
  {
    auto name = seat_name.empty() ? Config::default_seat_name : seat_name;
    auto found = util::find_if(cursors, [&](auto& el) { return el.seat == name; });
    if (found != cursors.end()) return &*found;
    return nullptr;
  }

  static void run_bind(Config& config, ArgList args)
  {
    config.bindings.push_back(parse_binding(args));
  }

  static void run_unbind(Config& config, ArgList args)
  {
    config.bindings.erase(util::remove_if(config.bindings, [combo = parse_key_combo(args)] (auto& binding) { return binding.combo == combo; }), config.bindings.end());
  }

  struct Command {
    std::string_view name;
    std::function<void(Config&, ArgList)> run;
  };

  std::array<Command, 2> commands = {{{"bind", run_bind}, {"unbind", run_unbind}}};

  auto Config::run_command(std::string_view command) -> void
  {
    auto args = ArgList(command);
    auto name = args.next();
    args = ArgList(args.remaining());
    auto iter = util::find_if(commands, [&](auto& cmd) { return cmd.name == name; });
    if (iter != commands.end()) {
      iter->run(*this, args);
    }
  }

  bool is_whitespace(char c)
  {
    return c == ' ' || c == '\t';
  }

  auto ArgList::peek() -> std::string_view
  {
    auto first = _last;
    while (first != _str.end() && is_whitespace(*first)) {
      ++first;
    }
    auto last = first;
    while (last != _str.end() && !is_whitespace(*last)) {
      ++last;
    }
    return std::string_view(&*first, last - first);
  }

  auto ArgList::next() -> std::string_view
  {
    auto next = peek();
    _first = next.begin();
    _last = next.end();
    return current();
  }

  auto ArgList::current() -> std::string_view
  {
    return std::string_view(&*_first, _last - _first);
  }

  auto ArgList::data() -> std::string_view
  {
    return _str;
  }

  auto ArgList::remaining() -> std::string_view
  {
    return std::string_view(&*_last, _str.end() - _last);
  }

} // namespace cloth
