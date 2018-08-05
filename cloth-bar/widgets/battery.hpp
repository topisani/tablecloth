#pragma once

#include <filesystem>
#include <fstream>

#include <gtkmm.h>

#include "util/logging.hpp"
#include "util/chrono.hpp"

namespace cloth::bar::widgets {

  namespace fs = std::filesystem;

  struct Battery {
    static inline const fs::path data_dir = "/sys/class/power_supply/";

    std::vector<fs::path> batteries;

    Battery()
    {
      try {
        for (auto& node : fs::directory_iterator(data_dir)) {
          if (fs::is_directory(node) && fs::exists(node / "charge_now") &&
              fs::exists(node / "charge_full")) {
            batteries.push_back(node);
          }
        }

      } catch (fs::filesystem_error& e) {
        LOGE(e.what());
      }

      label.get_style_context()->add_class("battery-status");

      thread = [this] {
        update();
        thread.sleep_for(chrono::minutes(1));
      };
    }

    auto update() -> void
    {
      try {
        for (auto& bat : batteries) {
          int full, now;
          std::ifstream(bat / "charge_now") >> now;
          std::ifstream(bat / "charge_full") >> full;
          int pct = float(now) / float(full) * 100.f;
          label.set_text(fmt::format("{}%", pct));
        }
      } catch (std::exception& e) {
        LOGE(e.what());
      }
    }

    operator Gtk::Widget&()
    {
      return label;
    }

    util::SleeperThread thread;
    Gtk::Label label;
  };

} // namespace cloth::bar::widgets
