#include <clara.hpp>

#include <wayland-client.hpp>
#include <workspaces-protocol.hpp>

#include "util/logging.hpp"


namespace cloth::msg {

  using namespace clara;
  namespace wl = wayland;

  struct Client {
    int workspace = 0;
    bool show_help = false;
    bool listen;

    wl::display_t display;
    wl::registry_t registry;
    wl::workspace_manager_t workspaces;

    auto bind_interfaces()
    {
      registry = display.get_registry();
      registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version) {
        if (interface == workspaces.interface_name) {
          registry.bind(name, workspaces, version);
          if (listen) workspaces.on_state() = [&] (uint32_t current, uint32_t count) {
            std::cout << fmt::format("workspace {}/{}", current, count) << std::endl;
          };
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
    }

    int main(int argc, char* argv[])
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
    LOGE(e.what());
    return 1;
  }
}
