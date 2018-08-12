#include "client.hpp"

#include "util/logging.hpp"

namespace cloth::kbd {

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      LOGD("Global: {}", interface);
      if (interface == virtual_keyboard_manager.interface_name) {
        registry.bind(name, virtual_keyboard_manager, version);
      } else if (interface == layer_shell.interface_name) {
        registry.bind(name, layer_shell, version);
      } else if (interface == seat.interface_name) {
        registry.bind(name, seat, version);
      }
    };
    display.roundtrip();
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

    bind_interfaces();

    VirtualKeyboard virtkbd{*this};

    gtk_main.run();
    return 0;
  }

} // namespace cloth::kbd
