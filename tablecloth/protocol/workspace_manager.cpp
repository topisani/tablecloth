#include "workspace_manager.hpp"

#include "util/logging.hpp"

#include "server.hpp"

#include <tablecloth-shell-server-protocol.h>

namespace cloth {

  static const struct workspace_manager_interface workspace_manager_impl = {
    .switch_to = [] (wl::client_t*, wl::resource_t* resource, uint32_t ws) {
      static_cast<WorkspaceManager*>(resource->data)->switch_to(ws);
    },
    .move_surface = [] (wl::client_t*, wl::resource_t* resource, wl::resource_t* surface, uint32_t ws) {
      static_cast<WorkspaceManager*>(resource->data)->move_surface(surface, ws);
    }
  };

  static void bind_workspace_manager(wl::client_t* client, void* data, uint32_t version, uint32_t id)
  {
    if (version > 1) version = 1;

    wl::resource_t* resource = wl_resource_create(client, &workspace_manager_interface, version, id);
    wl_resource_set_implementation(resource, &workspace_manager_impl, data, nullptr);
    resource->destroy = [] (wl::resource_t* res) {
      auto& bound_clients = static_cast<WorkspaceManager*>(res->data)->bound_clients;
      bound_clients.erase(util::remove(bound_clients, res), bound_clients.end());
    };
    static_cast<WorkspaceManager*>(data)->bound_clients.push_back(resource);
  }

  WorkspaceManager::WorkspaceManager(Server& server) 
    : server(server),
      global (wl_global_create(server.wl_display, &workspace_manager_interface, 1, this, &bind_workspace_manager))
  {}

  WorkspaceManager::~WorkspaceManager() noexcept {
    wl_global_destroy(global);
    for (auto* res : bound_clients) {
      wl_resource_destroy(res);
    }
  }

  // Implementations // 

  auto WorkspaceManager::switch_to(int idx) -> void {
    server.desktop.switch_to_workspace(idx);
  }

  auto WorkspaceManager::move_surface(wl::resource_t* surface_resource, int ws_idx) -> void {
    auto surface = (wlr::surface_t*) wl_resource_get_user_data(surface_resource);
    auto& dst_ws = server.desktop.workspaces.at(ws_idx);
    for (auto& ws : server.desktop.workspaces) {
      auto found = util::find_if(ws.views(), [&] (View& v) { return v.wlr_surface == surface; });
      if (found != ws.views().end()) {
        dst_ws.add_view(ws.erase_view(*found));
        return;
      }
    }
    LOGE("Surface not found");
  }

  auto WorkspaceManager::send_state() -> void {
    for (auto* resource : bound_clients) {
      workspace_manager_send_state(resource, server.desktop.current_workspace().index, Desktop::workspace_count);
    }
  }

};
