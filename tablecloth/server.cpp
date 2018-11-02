#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>

#include "util/logging.hpp"

#include "server.hpp"

namespace cloth {

  Server::Server(int argc, char* argv[]) noexcept
    : wl_display (wl_display_create()),
      wl_event_loop(wl_display_get_event_loop(wl_display)),
      backend(wlr_backend_autocreate(wl_display, nullptr)),
      renderer(wlr_backend_get_renderer(backend)),
      data_device_manager(wlr_data_device_manager_create(wl_display)),
      config(argc, argv), desktop(*this, config), input(*this, config),
      workspace_manager(*this),
      window_manager(*this)
  {
    assert(wl_display && wl_event_loop);

    if (backend == nullptr) {
      cloth_error("could not start backend");
    }
    assert(renderer);

    wlr_renderer_init_wl_display(renderer, wl_display);
  }

  Server::~Server() noexcept
  {
    if (wl_display) {
      wl_display_destroy_clients(wl_display);
      wl_display_destroy(wl_display);
    }
    if (wl_event_loop) wl_event_loop_destroy(wl_event_loop);
    if (backend) wlr_backend_destroy(backend);
    if (renderer) wlr_renderer_destroy(renderer);
    if (data_device_manager) wlr_data_device_manager_destroy(data_device_manager);
  }

} // namespace cloth
