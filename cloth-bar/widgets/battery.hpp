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
          if (fs::is_directory(node) && fs::exists(node / "capacity") && fs::exists(node / "status")) {
            batteries.push_back(node);
          }
        }

      } catch (fs::filesystem_error& e) {
        LOGE(e.what());
      }

      label.get_style_context()->add_class("battery-status");

      thread = [this] {
        update();
        thread.sleep_for(chrono::seconds(30));
      };
    }

    auto update() -> void
    {
      try {
        std::ostringstream str;
        for (auto& bat : batteries) {
          int capacity;
          std::string status;
          std::ifstream(bat / "capacity") >> capacity;
          std::ifstream(bat / "status") >> status;
          label.set_text(fmt::format("{}% {} ", capacity, status));
          return;
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
