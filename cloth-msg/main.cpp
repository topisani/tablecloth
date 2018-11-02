#include <clara.hpp>

#include <wayland-client.hpp>
#include <tablecloth-shell-protocol.hpp>

#include "util/logging.hpp"


namespace cloth::msg {

  using namespace clara;
  namespace wl = wayland;

  struct Client {
    int workspace = 0;
    bool show_help = false;
    bool listen = false;
    bool cycle_focus = false;

    std::string commands;

    wl::display_t display;
    wl::registry_t registry;
    wl::workspace_manager_t workspaces;
    wl::cloth_window_manager_t cloth_windows;
    std::vector<wl::output_t> outputs;

    auto bind_interfaces()
    {
      registry = display.get_registry();
      registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version) {
        if (interface == workspaces.interface_name) {
          registry.bind(name, workspaces, version);
          if (listen) workspaces.on_state() = [&] (uint32_t current, uint32_t count) {
            std::cout << fmt::format("workspace {}:{}", current + 1, count) << std::endl;
          };
        } else if (interface == cloth_windows.interface_name) {
          registry.bind(name, cloth_windows, version);
          if (listen) cloth_windows.on_focused_window_name() = [&] (const std::string& name, uint32_t ws) {
            std::cout << fmt::format("focused {}:{}", ws + 1, name) << std::endl;
          };
        } else if (interface == wl::output_t::interface_name) {
          auto& output = outputs.emplace_back();
          registry.bind(name, output, version);
        }
      };
      display.roundtrip();
    }

    auto make_cli()
    {
      // clang-format off

      auto cli = Opt(workspace, "workspace") 
               ["-s"]["--switch-ws"]
               ("Switch to a workspace")  
             | Opt(cycle_focus)
               ["--cycle-focus"]
               ("Cycle Focus")
             | Opt(commands, "commands")
               ["-r"]["--run-commands"]
               ("Run cloth commands")
             | Opt(listen)
               ["-l"]["--listen"]
               ("Listen for events")
             | Help(show_help);

      // clang-format on
      return cli;
    }

    auto send_messages()
    {
      if (workspace > 0) {
        workspaces.switch_to(workspace - 1);
      }
      if (cycle_focus) cloth_windows.cycle_focus();
      if (!commands.empty()) cloth_windows.run_command(commands);
      display.roundtrip();
    }

    int main(int argc, char* argv[])
    {
      auto cli = make_cli();
      auto result = cli.parse(Args(argc, argv));
      if (!result) {
        cloth_error("Error in command line: {}", result.errorMessage());
        return 1;
      }
      if (show_help) {
        std::cout << cli;
        return 1;
      }

      bind_interfaces();

      if (!cloth_windows || !workspaces) {
        cloth_error("Couldn't bind cloth interfaces");
        return 1;
      }

      send_messages();

      while (listen) display.dispatch();

      return 0;
    }
  };

} // namespace cloth::msg

int main(int argc, char* argv[])
{
  try {
    cloth::msg::Client c;
    return c.main(argc, argv);
  } catch(std::runtime_error& e) {
    cloth_error(e.what());
    return 1;
  }
}
