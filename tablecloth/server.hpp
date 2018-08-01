#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <wayland-server.h>
#include "wlroots.hpp"

#include "config.hpp"
#include "desktop.hpp"
#include "input.hpp"
#include "protocol/workspace_manager.hpp"
#include "protocol/window_manager.hpp"

namespace cloth {

  struct Server {
    ~Server() noexcept;

    wl::display_t* wl_display = nullptr;
    wl::event_loop_t* wl_event_loop = nullptr;

    wlr::backend_t* backend = nullptr;
    wlr::renderer_t* renderer = nullptr;

    wlr::data_device_manager_t* data_device_manager = nullptr;

    Config config;
    Desktop desktop;
    Input input;

    WorkspaceManager workspace_manager;
    WindowManager window_manager;

    Server(int argc, char* argv[]) noexcept;
  };

} // namespace cloth
