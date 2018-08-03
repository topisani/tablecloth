#pragma once

#include <gtkmm.h>

#include <protocols.hpp>

namespace cloth::bar {

  namespace wl = wayland;

  struct Client;

  struct Bar {
    Bar(Client& client, std::unique_ptr<wl::output_t>&& output);

    Client& client;

    Gtk::Window window;
    wl::surface_t surface;
    wl::zwlr_layer_surface_v1_t layer_surface;
    std::unique_ptr<wl::output_t> output;

    auto set_width(int) -> void;

  private:
    int width = 10;

    auto setup_widgets() -> void;
  };

}
