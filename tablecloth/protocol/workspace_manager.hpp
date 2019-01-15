#pragma once

#include <vector>
#include <wayland-server.h>

#include "wlroots.hpp"

namespace cloth {

  struct Server;

  struct WorkspaceManager {
    auto switch_to(int idx) -> void;
    auto move_surface(wl::resource_t*, int ws_idx) -> void;

    auto send_state() -> void;

    WorkspaceManager(Server&);
    ~WorkspaceManager() noexcept;

    Server& server;
    wl::global_t* global;
    std::vector<wl::resource_t*> bound_clients;
  };

}
