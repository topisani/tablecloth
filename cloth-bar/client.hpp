#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "gdkwayland.hpp"

#include "bar.hpp"

namespace cloth::bar {

  namespace wl = wayland;

  struct Client {
    int height = 16;
    bool show_help = false;

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::workspace_manager_t workspaces;
    wl::cloth_window_manager_t window_manager;
    wl::zwlr_layer_shell_v1_t layer_shell;
    std::vector<Bar> bars;

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
                 | Opt(height, "height")
                   ["--height"]
                   ("Bar Height");
      // clang-format on
      return cli;
    }

    int main(int argc, char* argv[]);
  };
}
