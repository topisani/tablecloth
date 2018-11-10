#pragma once

#include <gtkmm.h>

#include <protocols.hpp>

namespace cloth::bar {

  namespace wl = wayland;

  struct Client;

  struct Bar {
    Bar(Client& client, std::unique_ptr<wl::output_t>&& output);

    Bar(const Bar&) = delete;

    Client& client;

    Gtk::Window window;
    wl::surface_t surface;
    wl::zwlr_layer_surface_v1_t layer_surface;
    std::unique_ptr<wl::output_t> output;
    wl::zxdg_output_v1_t xdg_output;
    std::string output_name;
    bool visible = true;
    int current_workspace = 0;

    auto set_width(int) -> void;

    auto toggle() -> void;

  private:
    auto setup_widgets() -> void;
    auto setup_css() -> void;
    auto& cmd_button(const std::string& label,
                     const std::string& cmd,
                     const std::string& klass = "");

    int width = 10;
    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;
  };
} // namespace cloth::bar
