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

  auto get_render_data(View& v) -> RenderData {
    return {
      .layout = {
        .x = v.x,
        .y = v.y,
        .width = (double) v.width,
        .height = (double) v.height,
        .rotation = v.rotation
      },
      .alpha = v.alpha
    };
  }

  auto Output::render() -> void
  {
    if (!wlr_output.enabled) {
      return;
    }

    context.views.clear();

    if (prev_workspace != workspace && prev_workspace && ws_alpha >= 1.f) {
      ws_alpha = 0;
    }
    if (ws_alpha <= 1.f) ws_alpha += 0.075;
    if (ws_alpha > 1.f) ws_alpha = 1.f;

    float prev_ws_alpha = 1.f - ws_alpha;
    if (prev_ws_alpha > 0 && prev_workspace) {
      double dx = wlr_output.width * ws_alpha;
      if (workspace->index < prev_workspace->index) dx = -dx;
      for (auto& v : prev_workspace->visible_views()) {
        auto data = get_render_data(v);
        data.alpha *= prev_ws_alpha;
        data.layout.width = v.width * prev_ws_alpha;
        data.layout.height = v.height * prev_ws_alpha;
        data.layout.x -= dx;
        data.layout.x += v.width * ws_alpha / 2.f;
        data.layout.y += v.height * ws_alpha / 2.f;
        context.views.emplace_back(v, data);
      }
    }

    if (ws_alpha >= 1.f) {
      prev_workspace = workspace;
    }

    if (ws_alpha > 0) {
      double dx = wlr_output.width * (1 -ws_alpha);
      if (prev_workspace && workspace->index < prev_workspace->index) dx = -dx;
      for (auto& v : workspace->visible_views()) {
        auto data = get_render_data(v);
        data.alpha *= ws_alpha;
        if (ws_alpha != 1.f) {
          data.layout.width = v.width * ws_alpha;
          data.layout.height = v.height * ws_alpha;
          data.layout.y += v.height * prev_ws_alpha / 2.f;
          data.layout.x += v.width * prev_ws_alpha / 2.f;
          data.layout.x += dx;
        }
        context.views.emplace_back(v, data);
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
      LOGE("Configured mode for {} not available", output.name);
    } else {
      LOGD("Assigning configured mode to {}", output.name);
      wlr_output_set_mode(&output, best);
    }
  }

  Output::Output(Desktop& p_desktop, Workspace& ws, wlr::output_t& wlr) noexcept
    : desktop(p_desktop),
      workspace(&ws),
      wlr_output(wlr),
      last_frame(chrono::clock::now())
  {
    wlr_output.data = this;

    LOGD("Output '{}' added", wlr_output.name);
    LOGD("'{} {} {}' {}mm x {}mm", wlr_output.make, wlr_output.model, wlr_output.serial,
         wlr_output.phys_width, wlr_output.phys_height);

    if (!wl_list_empty(&wlr_output.modes)) {
      wlr::output_mode_t* mode = wl_container_of((&wlr_output.modes)->prev, mode, link);
      wlr_output_set_mode(&wlr_output, mode);
    }

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

    Config::Output* output_config = desktop.config.get_output(wlr_output);
    if (output_config) {
      if (output_config->enable) {
        if (wlr_output_is_drm(&wlr_output)) {
          for (auto& mode : output_config->modes) {
            wlr_drm_connector_add_mode(&wlr_output, &mode.info);
          }
        } else {
          if (output_config->modes.empty()) {
            LOGE("Can only add modes for DRM backend");
          }
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

  auto Output::damage_whole() -> void
  {
    context.damage_whole();
  }
  auto Output::damage_whole_view(View& view) -> void
  {
    context.damage_whole_view(view);
  }
  auto Output::damage_whole_decoration(View& view) -> void
  {
    context.damage_whole_decoration(view);
  }
  auto Output::damage_from_view(View& view) -> void
  {
    context.damage_from_view(view);
  }
  auto Output::damage_whole_drag_icon(DragIcon& icon) -> void
  {
    context.damage_whole_drag_icon(icon);
  }
  auto Output::damage_from_local_surface(wlr::surface_t& surface, double ox, double oy, float rotation)
    -> void
  {
    context.damage_from_local_surface(surface, ox, oy, rotation);
  }
  auto Output::damage_whole_local_surface(wlr::surface_t& surface, double ox, double oy, float rotation)
    -> void
  {
    context.damage_whole_local_surface(surface, ox, oy, rotation);
  }


} // namespace cloth
