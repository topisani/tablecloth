#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>
#include <thread>

#include <protocols.hpp>

#include "util/ptr_vec.hpp"

#include "gdkwayland.hpp"

#include "bar.hpp"

#include <dbus-c++/dbus.h>

#include <widgets/status-icons.hpp>

namespace cloth::bar {

  namespace wl = wayland;

  struct Client {
    int height = 26;
    bool show_help = false;
    std::string css_file = "./cloth-bar/resources/style.css";

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::workspace_manager_t workspaces;
    wl::cloth_window_manager_t window_manager;
    wl::zwlr_layer_shell_v1_t layer_shell;
    util::ptr_vec<Bar> bars;
    DBus::BusDispatcher dispatcher;
    std::thread dbus_thread;

    struct {
      sigc::signal<void(int, int)> workspace_state;
      sigc::signal<void(std::string)> focused_window_name;
    } signals;

    Client(int argc, char* argv[])
      : gtk_main(argc, argv),
        gdk_display(Gdk::Display::get_default()),
        display(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
    {}

    auto bind_interfaces();

    auto dbus_main() -> void;

    auto setup_css();

    auto make_cli() 
    {
      using namespace clara;
      // clang-format off
      auto cli = Parser{} | Help(show_help)
                 | Opt(height, "height")
                   ["--height"]
                   ("Bar Height")
                 | Opt(css_file, "css_file")
                   ["--css"]
                   ("Path to css file");
      // clang-format on
      return cli;
    }

    int main(int argc, char* argv[]);
  };
}
