#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <thread>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "util/logging.hpp"
#include "util/ptr_vec.hpp"

#include "gdkwayland.hpp"

#include "notification-server.hpp"

namespace cloth::notifications {

  namespace wl = wayland;

  struct Client {
    int height = 26;
    bool show_help = false;
    std::string css_file = "./cloth-notifications/resources/style.css";

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::zwlr_layer_shell_v1_t layer_shell;
    DBus::BusDispatcher dispatcher;
    std::thread dbus_thread;

    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;

    Client(int argc, char* argv[])
      : gtk_main(argc, argv),
        gdk_display(Gdk::Display::get_default()),
        display(gdk_wayland_display_get_wl_display(gdk_display->gobj())),
        style_context(Gtk::StyleContext::create()),
        css_provider(Gtk::CssProvider::create())
    {
      if (!css_provider->load_from_path(css_file)) {
        LOGE("Error loading CSS file");
      }
    }

    auto dbus_main() -> void;

    auto bind_interfaces();

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
} // namespace cloth::notifications
