#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <thread>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "util/ptr_vec.hpp"
#include "gdkwayland.hpp"

#include "widgets/outputs.hpp"

#include "output.hpp"

namespace cloth::outputs {

  namespace wl = wayland;

  struct Client {
    bool show_help = false;
    std::string css_file = "./cloth-outputs/resources/style.css";

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::zxdg_output_manager_v1_t output_manager;
    util::ptr_vec<Output> outputs;

    widgets::Outputs outputs_widget {outputs};

    Gtk::Window window;

    struct {
      sigc::signal<void()> output_list_updated;
    } signals;

    Client(int argc, char* argv[])
      : gtk_main(argc, argv),
        gdk_display(Gdk::Display::get_default()),
        display(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
    {}

    auto bind_interfaces();

    auto setup_gui() -> void;

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
} // namespace cloth::outputs
