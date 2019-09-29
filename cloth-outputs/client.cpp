#include "client.hpp"

#include <iostream>

#include "util/logging.hpp"

namespace cloth::outputs {

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      cloth_debug("Global: {}", interface);
      if (interface == output_manager.interface_name) {
        registry.bind(name, output_manager, version);
      } else if (interface == wl::output_t::interface_name) {
        auto output = std::make_unique<wl::output_t>();
        registry.bind(name, *output, version);
        outputs.emplace_back(*this, std::move(output));
      }
    };
    display.roundtrip();
  }

  int Client::main(int argc, char* argv[])
  {
    auto cli = make_cli();
    auto result = cli.parse(clara::Args(argc, argv));
    wlr_log_init(WLR_DEBUG, nullptr);

    if (!result) {
      cloth_error("Error in command line: {}", result.errorMessage());
      return 1;
    }
    if (show_help) {
      std::cout << cli;
      return 1;
    }

    bind_interfaces();
    setup_gui();

    gtk_main.run();
    return 0;
  }

  auto Client::setup_gui() -> void
  {
    window.set_title("Output settings");
    window.show();

    window.add(outputs_widget);

    signals.output_list_updated.connect([&] {
    });

    window.show_all();
  }

} // namespace cloth::outputs
