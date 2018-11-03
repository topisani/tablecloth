#include "output.hpp"

#include "wlroots.hpp"

#include "util/logging.hpp"

#include "config.hpp"
#include "layers.hpp"
#include "output.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"

namespace cloth {

  auto get_render_data(View& v) -> render::RenderData
  {
    return {.layout = {.x = v.x,
                       .y = v.y,
                       .width = (double) v.width,
                       .height = (double) v.height,
                       .rotation = v.rotation},
            .alpha = v.alpha};
  }

  auto Output::render() -> void
  {
    if (!wlr_output.enabled) {
      return;
    }

    context.reset();

    if (prev_workspace != workspace && prev_workspace && ws_alpha >= 1.f) {
      ws_alpha = 0;
    }
    if (ws_alpha <= 1.f) ws_alpha += 0.1;
    if (ws_alpha > 1.f) ws_alpha = 1.f;

    if (prev_workspace == workspace && workspace->fullscreen_view) {
      context.fullscreen_view = workspace->fullscreen_view;
    } else {
      float prev_ws_alpha = 1.f - ws_alpha;
      if (prev_ws_alpha > 0 && prev_workspace) {
        double dx = wlr_output.width * ws_alpha;
        if (workspace->index < prev_workspace->index) dx = -dx;
        for (auto& v : prev_workspace->visible_views()) {
          auto data = get_render_data(v);
          data.alpha *= prev_ws_alpha;
          data.layout.x -= dx;
          context.views.emplace_back(v, data);
        }
      }

      if (ws_alpha >= 1.f) {
        prev_workspace = workspace;
      }

      if (ws_alpha > 0) {
        double dx = wlr_output.width * (1 - ws_alpha);
        if (prev_workspace && workspace->index < prev_workspace->index) dx = -dx;
        for (auto& v : workspace->visible_views()) {
          auto data = get_render_data(v);
          data.alpha *= ws_alpha;
          data.layout.x += dx;
          context.views.emplace_back(v, data);
        }
      }
    }

    context.do_render();

    if (ws_alpha < 1.f) context.damage_whole();
  }

  static void set_mode(wlr::output_t& output, Config::Output& oc)
  {
    int mhz = (int) (oc.mode.refresh_rate * 1000);

    if (wl_list_empty(&output.modes)) {
      // Output has no mode, try setting a custom one
      wlr_output_set_custom_mode(&output, oc.mode.width, oc.mode.height, mhz);
      return;
    }

    wlr::output_mode_t *mode, *best = nullptr;
    wl_list_for_each(mode, &output.modes, link)
    {
      if (mode->width == oc.mode.width && mode->height == oc.mode.height) {
        if (mode->refresh == mhz) {
          best = mode;
          break;
        }
        best = mode;
      }
    }
    if (!best) {
      cloth_error("Configured mode for {} not available", output.name);
    } else {
      cloth_debug("Assigning configured mode to {}", output.name);
      wlr_output_set_mode(&output, best);
    }
  }

  struct PresentationData {
    render::LayoutData layout;
    Output& output;
    wlr::presentation_event_t* event;
  };

  // TODO:
  // static void surface_send_presented(struct wlr_surface* surface, int sx, int sy, void* _data)
  // {
  //   auto& data = *(PresentationData*) _data;
  //   auto& output = data.output;
  //   float rotation = data.layout.rotation;
  //   double lx, ly;
  //   get_layout_position(data.layout, &lx, &ly, surface, sx, sy);
  //   if (!surface_intersect_output(surface, output->desktop->layout, output->wlr_output, lx, ly,
  //                                 rotation, NULL)) {
  //     return;
  //   }
  //   wlr_presentation_send_surface_presented(output->desktop->presentation, surface, data->event);
  // }

  Output::Output(Desktop& p_desktop, Workspace& ws, wlr::output_t& wlr) noexcept
    : desktop(p_desktop), workspace(&ws), wlr_output(wlr), last_frame(chrono::clock::now())
  {
    wlr_output.data = this;

    cloth_debug("Output '{}' added", wlr_output.name);
    cloth_debug("'{} {} {}' {}mm x {}mm", wlr_output.make, wlr_output.model, wlr_output.serial,
                wlr_output.phys_width, wlr_output.phys_height);

    on_destroy.add_to(wlr_output.events.destroy);
    on_destroy = [this] { util::erase_this(desktop.outputs, this); };

    on_mode.add_to(wlr_output.events.mode);
    on_mode = [this] { arrange_layers(*this); };

    on_transform.add_to(wlr_output.events.transform);
    on_transform = [this] { arrange_layers(*this); };

    on_damage_frame.add_to(context.damage->events.frame);
    on_damage_frame = [this] { render(); };

    on_damage_destroy.add_to(context.damage->events.destroy);
    on_damage_destroy = [this] { util::erase_this(desktop.outputs, this); };

    on_present.add_to(wlr_output.events.present);
    on_present = [] {
      // TODO:
      // struct wlr_output_event_present* output_event = data;
      // struct wlr_presentation_event event = {
      //   .output = output->wlr_output,
      //   .tv_sec = (uint64_t) output_event->when->tv_sec,
      //   .tv_nsec = (uint32_t) output_event->when->tv_nsec,
      //   .refresh = (uint32_t) output_event->refresh,
      //   .seq = (uint64_t) output_event->seq,
      //   .flags = output_event->flags,
      // };
      // struct presentation_data presentation_data = {
      //   .output = output,
      //   .event = &event,
      // };
      // output_for_each_surface(output, surface_send_presented, &presentation_data.layout,
      //                         &presentation_data);
    };

    Config::Output* output_config = desktop.config.get_output(wlr_output);
    if ((!output_config || output_config->enable) && !wl_list_empty(&wlr_output.modes)) {
      wlr::output_mode_t* mode = wl_container_of((&wlr_output.modes)->prev, mode, link);
      wlr_output_set_mode(&wlr_output, mode);
    }
    if (output_config) {
      if (output_config->enable) {
        if (wlr_output_is_drm(&wlr_output)) {
          for (auto& mode : output_config->modes) {
            wlr_drm_connector_add_mode(&wlr_output, &mode.info);
          }
        } else if (!output_config->modes.empty()) {
          cloth_error("Can only add modes for DRM backend");
        }

        if (output_config->mode.width) {
          set_mode(wlr_output, *output_config);
        }

        wlr_output_set_scale(&wlr_output, output_config->scale);
        wlr_output_set_transform(&wlr_output, output_config->transform);
        wlr_output_layout_add(desktop.layout, &wlr_output, output_config->x, output_config->y);
      } else {
        wlr_output_enable(&wlr_output, false);
      }
    } else {
      wlr_output_layout_add_auto(desktop.layout, &wlr_output);
    }

    arrange_layers(*this);
    context.damage_whole();
  }
} // namespace cloth
