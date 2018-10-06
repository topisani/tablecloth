#include "window_manager.hpp"

#include "util/logging.hpp"

#include "server.hpp"
#include "seat.hpp"
#include "layers.hpp"

#include <tablecloth-shell-server-protocol.h>

namespace cloth {

  static const struct cloth_window_manager_interface cloth_window_manager_impl = {
    .cycle_focus = [] (wl::client_t*, wl::resource_t* resource) {
      static_cast<WindowManager*>(resource->data)->cycle_focus();
    },
    .run_command = [] (wl::client_t*, wl::resource_t* resource, const char* commands) {
      static_cast<WindowManager*>(resource->data)->run_command(commands);
    },
  };

  static void bind_cloth_window_manager(wl::client_t* client, void* data, uint32_t version, uint32_t id)
  {
    if (version > 1) version = 1;

    wl::resource_t* resource = wl_resource_create(client, &cloth_window_manager_interface, version, id);
    wl_resource_set_implementation(resource, &cloth_window_manager_impl, data, nullptr);
    resource->destroy = [] (wl::resource_t* res) {
      auto& bound_clients = static_cast<WindowManager*>(res->data)->bound_clients;
      bound_clients.erase(util::remove(bound_clients, res), bound_clients.end());
    };
    auto& wm = *static_cast<WindowManager*>(data);
    wm.bound_clients.push_back(resource);
    wm.send_focused_window_name(wm.server.desktop.current_workspace());
  }

  WindowManager::WindowManager(Server& server) 
    : server(server),
      global (wl_global_create(server.wl_display, &cloth_window_manager_interface, 1, this, &bind_cloth_window_manager))
  {}

  WindowManager::~WindowManager() noexcept {
    wl_global_destroy(global);
    for (auto* res : bound_clients) {
      wl_resource_destroy(res);
    }
  }

  // Implementations // 

  auto WindowManager::cycle_focus() -> void {
    server.desktop.current_workspace().cycle_focus();
  }

  auto WindowManager::run_command(const char* command) -> void {
    LOGD("Running command {}", command);
    server.desktop.run_command(command);
    LOGE("No keyboard found");
  }

  auto WindowManager::send_focused_window_name(Workspace& ws) -> void {
    auto* view = ws.focused_view();
    auto name = view == nullptr ? "" : view->get_name();
    for (auto* resource : bound_clients) {
      cloth_window_manager_send_focused_window_name(resource, name.c_str(), ws.index);
    }
  }

};
