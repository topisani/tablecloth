#include <clara.hpp>

#include <gtkmm.h>
#include <tablecloth-shell-protocol.hpp>
#include <wayland-client.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

namespace cloth::bar {

  using namespace clara;
  namespace wl = wayland;

  struct Panel;

  struct Client {
    bool show_help = false;

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    wl::display_t display;
    wl::registry_t registry;
    wl::workspace_manager_t workspaces;
    wl::cloth_window_manager_t cloth_windows;

    std::unique_ptr<Panel> panel;

    Client(int argc, char* argv[])
      : gtk_main(argc, argv),
        gdk_display(Gdk::Display::get_default()),
        display(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
    {}

    auto bind_interfaces()
    {
      registry = display.get_registry();
      registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
        if (interface == workspaces.interface_name) {
          registry.bind(name, workspaces, version);
          workspaces.on_state() = [&](uint32_t current, uint32_t count) {
            std::cout << fmt::format("workspace {}:{}", current + 1, count) << std::endl;
          };
        } else if (interface == cloth_windows.interface_name) {
          registry.bind(name, cloth_windows, version);
          cloth_windows.on_focused_window_name() = [&](const std::string& name, uint32_t ws) {
            std::cout << fmt::format("focused {}:{}", ws + 1, name) << std::endl;
          };
        }
      };
      display.roundtrip();
    }

    auto make_cli()
    {
      // clang-format off
      auto cli = Parser{} | Help(show_help);
      // clang-format on
      return cli;
    }

    int main(int argc, char* argv[]);
  };

  struct Panel {
    Client& client;

    Gtk::Window window;
    wl::surface_t surface;
    wl::output_t output;

    Panel(Client& client) : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL}
    {
      window.set_title("tablecloth panel");
      window.set_decorated(false);

      auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
      window.add(box1);

      auto& button = *Gtk::manage(new Gtk::Button("TERM"));
      button.signal_clicked().connect(
        [&]() { client.cloth_windows.run_command("exec weston-terminal"); });
      box1.pack_start(button, true, false, 0);
      button.show();
      box1.show();

      window.show_all();

      window.set_size_request(16, 16);

      // Gdk::wayland::window::set_use_custom_surface(window);
      // surface = Gdk::wayland::window::get_wl_surface(window);
      // client.cloth_windows.set_panel_surface(output, surface);
    }
  };

  int Client::main(int argc, char* argv[])
  {
    auto cli = make_cli();
    auto result = cli.parse(Args(argc, argv));
    if (!result) {
      LOGE("Error in command line: {}", result.errorMessage());
      return 1;
    }
    if (show_help) {
      std::cout << cli;
      return 1;
    }

    bind_interfaces();

    panel = std::make_unique<Panel>(*this);

    gtk_main.run(panel->window);
    return 0;
  }
} // namespace cloth::bar

int main(int argc, char* argv[])
{
  try {
    cloth::bar::Client c(argc, argv);
    return c.main(argc, argv);
  } catch (std::runtime_error& e) {
    LOGE(e.what());
    return 1;
  }
}
