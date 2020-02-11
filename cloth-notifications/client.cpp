#include "client.hpp"

#include <dbus/dbus.h>
#include <iostream>

#include "util/logging.hpp"

namespace cloth::notifications {

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      cloth_debug("Global: {}", interface);
      if (interface == layer_shell.interface_name) {
        registry.bind(name, layer_shell, version);
      }
      if (interface == wl::output_t::interface_name) {
        registry.bind(name, output, version);
      }
    };
    display.roundtrip();
  }

  auto Client::dbus_main() -> void
  {
    DBus::default_dispatcher = &dispatcher;

    DBus::Connection conn = DBus::Connection::SessionBus();
    try {
        conn.request_name(NotificationServer::server_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING);
    } catch (...) {
        cloth_error("Could not acquire notification server name");
    }

    NotificationServer server(*this, conn);

    dispatcher.enter();
  }

  auto Client::setup_css() -> void
  {
    if (!css_provider->load_from_path(css_file)) {
      cloth_error("Error loading CSS file");
    }
  }

  int Client::main(int argc, char* argv[])
  {
    auto cli = make_cli();
    auto result = cli.parse(clara::Args(argc, argv));

    if (!result) {
      cloth_error("Error in command line: {}", result.errorMessage());
      return 1;
    }
    if (show_help) {
      std::cout << cli;
      return 1;
    }

    setup_css();

    bind_interfaces();

    dbus_thread = std::thread(&Client::dbus_main, this);

    gtk_main.run();

    dispatcher.leave();
    return 0;
  }

} // namespace cloth::notifications
