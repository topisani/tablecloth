#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "util/ptr_vec.hpp"

#include "gdkwayland.hpp"

#include "keyboard.hpp"

namespace cloth::kbd {

  namespace wl = wayland;

  struct Client {
    bool show_help = false;
    std::string css_file = "./cloth-kbd/resources/style.css";

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::zwlr_layer_shell_v1_t layer_shell;
    wl::zwp_virtual_keyboard_manager_v1_t virtual_keyboard_manager;
    wl::seat_t seat;

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

    auto make_cli() 
    {
      using namespace clara;
      // clang-format off
      auto cli = Parser{} | Help(show_help)
                 | Opt(css_file, "css_file")
                   ["--css"]
                   ("Path to css file");
      // clang-format on
      return cli;
    }

    int main(int argc, char* argv[]);
  };
}
