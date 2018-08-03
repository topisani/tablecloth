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
        workspaces.on_state() = [&](uint32_t current, uint32_t count) {
          std::cout << fmt::format("workspace {}:{}", current + 1, count) << std::endl;
        };
      } else if (interface == window_manager.interface_name) {
        registry.bind(name, window_manager, version);
      } else if (interface == layer_shell.interface_name) {
        registry.bind(name, layer_shell, version);
      } else if (interface == wl::output_t::interface_name) {
        auto output = std::make_unique<wl::output_t>();
        registry.bind(name, *output, version);
        auto& bar = bars.emplace_back(*this, std::move(output));
        bar.output->on_mode() = [&bar](wl::output_mode, int32_t w, int32_t h, int32_t refresh) { 
                                         LOGI("Bar width configured: {}", w);
                                         bar.set_width(w); };
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

    gtk_main.run();
    return 0;
  }

} // namespace cloth::bar
