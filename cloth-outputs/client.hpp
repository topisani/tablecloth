#pragma once

#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>
#include <thread>

#include <protocols.hpp>

#include "util/ptr_vec.hpp"

#include "gdkwayland.hpp"

namespace cloth::outputs {

  namespace wl = wayland;
  struct Client;

  struct Position {
    int x = 0;
    int y = 0;
  };

  struct Size {
    int width = 0;
    int height = 0;
  };

  struct Output {

    Output(Client& client, std::unique_ptr<wl::output_t>&& output);

    Client& client;
    std::unique_ptr<wl::output_t> output;
    wl::zxdg_output_v1_t xdg_output;

    Position logical_position;
    Size logical_size;
    int scale = 0;
    std::string name;
    std::string description;
    bool done = false;

    operator Gtk::Widget&();

  private:
    Gtk::ListBoxRow lbr;
  };

  struct Client {
    int height = 26;
    bool show_help = false;
    std::string css_file = "./cloth-outputs/resources/style.css";

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::zxdg_output_manager_v1_t output_manager;
    util::ptr_vec<Output> outputs;

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
