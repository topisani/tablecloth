#include "client.hpp"

#include "util/logging.hpp"

namespace cloth::bar {

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      LOGD("Global: {}", interface);
      if (interface == workspaces.interface_name) {
        registry.bind(name, workspaces, version);
        workspaces.on_state() = [&] (unsigned current, unsigned count) {
          signals.workspace_state.emit(current, count);
        };
      } else if (interface == window_manager.interface_name) {
        registry.bind(name, window_manager, version);
        window_manager.on_focused_window_name() = [&] (const std::string& name, unsigned ws) {
          signals.focused_window_name.emit(name);
        };
      } else if (interface == layer_shell.interface_name) {
        registry.bind(name, layer_shell, version);
      } else if (interface == wl::output_t::interface_name) {
        auto output = std::make_unique<wl::output_t>();
        registry.bind(name, *output, version);
        bars.emplace_back(*this, std::move(output));
      }
    };
    display.roundtrip();
  }

  auto Client::dbus_main() -> void
  {
    DBus::default_dispatcher = &dispatcher;

    dispatcher.enter();
  }

  int Client::main(int argc, char* argv[])
  {
    auto cli = make_cli();
    auto result = cli.parse(clara::Args(argc, argv));

    if (!result) {
      LOGE("Error in command line: {}", result.errorMessage());
      return 1;
    }
    if (show_help) {
      std::cout << cli;
      return 1;
    }
    dbus_thread = std::thread(&Client::dbus_main, this);

    bind_interfaces();

    gtk_main.run();
    return 0;
  }

} // namespace cloth::bar
