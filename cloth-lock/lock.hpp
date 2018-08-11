#pragma once

#include <gtkmm.h>

#include <protocols.hpp>

namespace cloth::lock {

  namespace wl = wayland;

  struct Client;

  struct LockScreen {
    LockScreen(Client& client, std::unique_ptr<wl::output_t>&& output);

    LockScreen(const LockScreen&) = delete;

    Client& client;

    Gtk::Window window;
    wl::surface_t surface;
    wl::zwlr_layer_surface_v1_t layer_surface;
    std::unique_ptr<wl::output_t> output;

    auto set_size(int width, int height) -> void;

    auto submit() -> void;

  private:
    auto setup_widgets() -> void;
    auto setup_css() -> void;

    int width = 10;
    int height = 10;
    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;

    Gtk::Box box;
    Gtk::Entry user_prompt;
    Gtk::Entry password_prompt;
    Gtk::Button login_button;

    Gtk::Box hbox;
    Gtk::Box vbox;
  };

}
