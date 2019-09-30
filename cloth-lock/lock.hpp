#pragma once

#include <gtkmm.h>

#include "util/chrono.hpp"
#include "util/exception.hpp"

#include <protocols.hpp>

namespace cloth::lock {

  namespace wl = wayland;

  struct Client;

  struct ClockWidget {
    ClockWidget()
    {
      label.get_style_context()->add_class("clock-widget");
      thread = [this] {
        using namespace chrono;
        auto now = clock::now();
        auto t = std::time(nullptr);
        auto localtime = std::localtime(&t);
        label.set_text(fmt::format("{:02}:{:02}", localtime->tm_hour, localtime->tm_min));
        thread.sleep_until(floor<minutes>(now + minutes(1)));
      };
    };

    operator Gtk::Widget&()
    {
      return label;
    }

    Gtk::Label label;
    util::SleeperThread thread;
  };


  struct LockScreen {
    LockScreen(Client& client, std::unique_ptr<wl::output_t>&& output, bool show_widget);
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

    bool show_login_widgets = true;

    int width = 10;
    int height = 10;
    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;

    Gtk::Box box;
    Gtk::Box login_box;
    Gtk::Entry user_prompt;
    Gtk::Entry password_prompt;
    Gtk::Button login_button;

    ClockWidget clock_widget;

    Gtk::Box hbox;
    Gtk::Box vbox;
  };

}
