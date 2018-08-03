#include "bar.hpp"

#include "client.hpp"

#include "gdkwayland.hpp"

namespace cloth::bar {

  Bar::Bar(Client& client, std::unique_ptr<wl::output_t>&& p_output)
    : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL}, output(std::move(p_output))
  {
    window.set_title("tablecloth panel");
    window.set_decorated(false);

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = client.layer_shell.get_layer_surface(
      surface, *output, wl::zwlr_layer_shell_v1_layer::bottom, "cloth.panel");
    layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::top |
                             wl::zwlr_layer_surface_v1_anchor::left);
    layer_surface.set_size(client.height, 16);
    layer_surface.set_exclusive_zone(0);
    layer_surface.set_keyboard_interactivity(0);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      layer_surface.ack_configure(serial);
      window.show_all();
    };
    layer_surface.on_closed() = [&] { window.close(); };

    surface.commit();

    setup_widgets();
  }

  auto Bar::set_width(int width) -> void {
    this->width = width;
    window.set_size_request(width);
    window.resize(width, client.height);
    layer_surface.set_size(width, client.height);
  }

  auto Bar::setup_widgets() -> void {
    auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    window.add(box1);

    auto& focused_window = *Gtk::manage(new Gtk::Label());
    box1.pack_start(focused_window, true, false, 0);

    auto& button = *Gtk::manage(new Gtk::Button("TERM"));
    button.signal_clicked().connect(
      [&]() { client.window_manager.run_command("exec weston-terminal"); });
    box1.pack_start(button, true, false, 0);

    client.window_manager.on_focused_window_name() = [&focused_window] (std::string focused_window_name, uint32_t ws) {
      focused_window.set_text(focused_window_name);
    };
  }

} // namespace cloth::bar
