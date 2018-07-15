#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>

#include "server.hpp"

namespace cloth {

  Server::Server() noexcept
  { }

  Server::~Server() noexcept {
    if (wl_display) {
      wl_display_destroy_clients(wl_display);
      wl_display_destroy(wl_display);
    }
    if (wl_event_loop) wl_event_loop_destroy(wl_event_loop);
    if (backend) wlr_backend_destroy(backend);
    if (renderer) wlr_renderer_destroy(renderer);
    if (data_device_manager) wlr_data_device_manager_destroy(data_device_manager);
  }

  void Output::frame_notify(void* data)
  {
    struct wlr_renderer* renderer = wlr_backend_get_renderer(wlr_output->backend);

    auto now = chrono::clock::now();
    auto ts_now = chrono::to_timespec(now);

    wlr_output_make_current(wlr_output, NULL);
    wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

    float color[4] = {0.4f, 0.4f, 0.4f, 1.0f};
    wlr_renderer_clear(renderer, color);

    static int frame_num = 0;

    struct wl_resource* _surface;
    wl_resource_for_each(_surface, &server.compositor->surface_resources)
    {
      struct wlr_surface* surface = wlr_surface_from_resource(_surface);
      if (!wlr_surface_has_buffer(surface)) {
        continue;
      }
      struct wlr_box render_box = {.x = 20,
                                   .y = 20,
                                   .width = surface->current.width,
                                   .height = surface->current.height};
      float matrix[16];
      wlr_matrix_project_box(matrix, &render_box, surface->current.transform, 0,
                             wlr_output->transform_matrix);
      wlr_render_texture_with_matrix(renderer, surface->buffer->texture, matrix,
                                     0.5f + std::sin(frame_num / 10.f) / 2.f);
      wlr_surface_send_frame_done(surface, &ts_now);
    }

    wlr_output_swap_buffers(wlr_output, NULL, NULL);
    wlr_renderer_end(renderer);

    last_frame = now;
    frame_num++;
  }

  void new_output_notify(Server& server, void* data)
  {
    struct wlr_output* wlr_output = (struct wlr_output*) data;

    if (wl_list_length(&wlr_output->modes) > 0) {
      struct wlr_output_mode* mode = wl_container_of((&wlr_output->modes)->prev, mode, link);
      wlr_output_set_mode(wlr_output, mode);
    }

    auto& output = *server.outputs.emplace_back(std::make_unique<Output>(wlr_output, server));

    output.destroy = [&](void*) {
      server.outputs.erase(std::find_if(server.outputs.begin(), server.outputs.end(),
                                        [&](auto& uptr) { return &output == uptr.get(); }));
    };
    output.destroy.add_to(wlr_output->events.destroy);
    output.frame = [&](void* data) { output.frame_notify(data); };
    output.frame.add_to(wlr_output->events.frame);

    wlr_output_create_global(wlr_output);
  }
} // namespace cloth
