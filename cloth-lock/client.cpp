#include "client.hpp"

#include "util/logging.hpp"

namespace cloth::lock {

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      LOGD("Global: {}", interface);
      if (interface == input_inhibit_manager.interface_name) {
        registry.bind(name, input_inhibit_manager, version);
      } else if (interface == layer_shell.interface_name) {
        registry.bind(name, layer_shell, version);
      } else if (interface == wl::output_t::interface_name) {
        auto output = std::make_unique<wl::output_t>();
        registry.bind(name, *output, version);
        lock_screens.emplace_back(*this, std::move(output));
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

    auto inhibitor = input_inhibit_manager.get_inhibitor();

    gtk_main.run();
    return 0;
  }

} // namespace cloth::lock
