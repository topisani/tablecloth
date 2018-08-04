#pragma once

#include <wayland-server.h>

#include "wlroots.hpp"

namespace cloth {

  struct Server;
  struct Workspace;

  struct WindowManager {
    auto cycle_focus() -> void;
    auto run_command(const char*) -> void;

    auto send_focused_window_name(Workspace& ws) -> void;

    WindowManager(Server&);
    ~WindowManager() noexcept;

    Server& server;
    wl::global_t* global;
    std::vector<wl::resource_t*> bound_clients;
  };

}
