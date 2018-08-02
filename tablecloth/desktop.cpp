#include "util/logging.hpp"

#include "layers.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"
#include "wlroots.hpp"
#include "xcursor.hpp"

#include "util/iterators.hpp"

#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "workspaces-server-protocol.h"

namespace cloth {

  View* Desktop::view_at(double lx, double ly, wlr::surface_t*& surface, double& sx, double& sy)
  {
    wlr::output_t* wlr_output = wlr_output_layout_output_at(layout, lx, ly);
    if (wlr_output != nullptr) {
      Output* output = output_from_wlr_output(wlr_output);
      if (output != nullptr && output->workspace->fullscreen_view != nullptr) {
        if (output->workspace->fullscreen_view->at(lx, ly, surface, sx, sy)) {
          return output->workspace->fullscreen_view;
        } else {
          return nullptr;
        }
      }

      for (auto& view : util::view::reverse(output->workspace->visible_views())) {
        if (view.at(lx, ly, surface, sx, sy)) return &view;
      }
    }
    return nullptr;
  }

  static wlr::surface_t* layer_surface_at(Output& output,
                                          util::ptr_vec<LayerSurface>& layer,
                                          double ox,
                                          double oy,
                                          double& sx,
                                          double& sy)
  {
    for (auto& surface : util::view::reverse(layer)) {
      double _sx = ox - surface.geo.x;
      double _sy = oy - surface.geo.y;

      wlr::surface_t* sub =
        wlr_layer_surface_surface_at(&surface.layer_surface, _sx, _sy, &sx, &sy);

      if (sub) {
        return sub;
      }
    }
    return nullptr;
  }

  wlr::surface_t* Desktop::surface_at(double lx, double ly, double& sx, double& sy, View*& view)
  {
    wlr::surface_t* surface = nullptr;
    wlr::output_t* wlr_output = wlr_output_layout_output_at(layout, lx, ly);
    double ox = lx, oy = ly;
    view = nullptr;

    Output* output = nullptr;
    if (wlr_output) {
      output = (Output*) wlr_output->data;
      wlr_output_layout_output_coords(layout, wlr_output, &ox, &oy);

      if ((surface =
             layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                              ox, oy, sx, sy))) {
        return surface;
      }
      if ((surface = layer_surface_at(
             *output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], ox, oy, sx, sy))) {
        return surface;
      }
    }

    View* _view;
    if ((_view = view_at(lx, ly, surface, sx, sy))) {
      view = _view;
      return surface;
    }

    if (wlr_output) {
      if ((surface =
             layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
                              ox, oy, sx, sy))) {
        return surface;
      }
      if ((surface = layer_surface_at(
             *output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], ox, oy, sx,
             sy))) {
        return surface;
      }
    }
    return nullptr;
  }

  Desktop::Desktop(Server& p_server, Config& p_config) noexcept
    : workspaces(util::generate_array<10>([this](int n) { return Workspace(*this, n); })),
      server(p_server),
      config(p_config)
  {
    LOGD("Initializing tablecloth desktop");

    on_new_output = [this](void* data) {
      LOGD("New output");
      outputs.emplace_back(*this, workspaces[0], *(wlr::output_t*) data);

      for (auto& seat : server.input.seats) {
        seat.configure_cursor();
        seat.configure_xcursor();
      }
    };
    on_new_output.add_to(server.backend->events.new_output);

    layout = wlr_output_layout_create();
    wlr_xdg_output_manager_create(server.wl_display, layout);

    on_layout_change.add_to(layout->events.change);
    on_layout_change = [this](void* data) {
      wlr::output_t* center_output = wlr_output_layout_get_center_output(this->layout);
      if (center_output == nullptr) return;

      wlr::box_t* center_output_box = wlr_output_layout_get_box(this->layout, center_output);
      double center_x = center_output_box->x + center_output_box->width / 2;
      double center_y = center_output_box->y + center_output_box->height / 2;

      for (auto& output : outputs) {
        for (auto& view : output.workspace->views()) {
          auto box = view.get_box();
          if (wlr_output_layout_intersects(this->layout, nullptr, &box)) continue;
          view.move(center_x - box.width / 2, center_y - box.height / 2);
        }
      }
    };

    compositor = wlr_compositor_create(server.wl_display, server.renderer);

    xdg_shell_v6 = wlr_xdg_shell_v6_create(server.wl_display);
    on_xdg_shell_v6_surface = [this](void* data) { handle_xdg_shell_v6_surface(data); };
    on_xdg_shell_v6_surface.add_to(xdg_shell_v6->events.new_surface);

    xdg_shell = wlr_xdg_shell_create(server.wl_display);
    on_xdg_shell_surface.add_to(xdg_shell->events.new_surface);
    on_xdg_shell_surface = [this](void* data) { handle_xdg_shell_surface(data); };

    wl_shell = wlr_wl_shell_create(server.wl_display);
    on_wl_shell_surface.add_to(wl_shell->events.new_surface);
    on_wl_shell_surface = [this](void* data) { handle_wl_shell_surface(data); };

    layer_shell = wlr_layer_shell_create(server.wl_display);
    on_layer_shell_surface.add_to(layer_shell->events.new_surface);
    on_layer_shell_surface = [this](void* data) { handle_layer_shell_surface(data); };

#ifdef WLR_HAS_XWAYLAND
    const char* cursor_theme = nullptr;
    const char* cursor_default = xcursor_default;
    Config::Cursor* cc = config.get_cursor(Config::default_seat_name);
    if (cc != nullptr) {
      cursor_theme = cc->theme.c_str();
      if (!cc->default_image.empty()) {
        cursor_default = cc->default_image.c_str();
      }
    }

    xcursor_manager = wlr_xcursor_manager_create(cursor_theme, xcursor_size);
    if (xcursor_manager == nullptr) {
      LOGE("Cannot create XCursor manager for theme {}", cursor_theme);
    }

    if (config.xwayland) {
      xwayland = wlr_xwayland_create(server.wl_display, compositor, config.xwayland_lazy);
      on_xwayland_surface.add_to(xwayland->events.new_surface);
      on_xwayland_surface = [this](void* data) { handle_xwayland_surface(data); };

      if (wlr_xcursor_manager_load(xcursor_manager, 1)) {
        LOGE("Cannot load XWayland XCursor theme");
      }
      wlr::xcursor_t* xcursor = wlr_xcursor_manager_get_xcursor(xcursor_manager, cursor_default, 1);
      if (xcursor != nullptr) {
        wlr::xcursor_image_t* image = xcursor->images[0];
        wlr_xwayland_set_cursor(xwayland, image->buffer, image->width * 4, image->width,
                                image->height, image->hotspot_x, image->hotspot_y);
      }
    }
#endif

    gamma_control_manager = wlr_gamma_control_manager_create(server.wl_display);
    screenshooter = wlr_screenshooter_create(server.wl_display);
    export_dmabuf_manager_v1 = wlr_export_dmabuf_manager_v1_create(server.wl_display);
    server_decoration_manager = wlr_server_decoration_manager_create(server.wl_display);
    wlr_server_decoration_manager_set_default_mode(server_decoration_manager,
                                                   WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);
    primary_selection_device_manager =
      wlr_primary_selection_device_manager_create(server.wl_display);
    idle = wlr_idle_create(server.wl_display);
    idle_inhibit = wlr_idle_inhibit_v1_create(server.wl_display);

    input_inhibit = wlr_input_inhibit_manager_create(server.wl_display);
    on_input_inhibit_activate = [this](void* data) {
      for (auto& seat : this->server.input.seats) {
        seat.set_exclusive_client(this->input_inhibit->active_client);
      }
    };
    on_input_inhibit_activate.add_to(input_inhibit->events.activate);

    on_input_inhibit_deactivate = [this](void* data) {
      for (auto& seat : this->server.input.seats) {
        seat.set_exclusive_client(nullptr);
      }
    };
    on_input_inhibit_deactivate.add_to(input_inhibit->events.deactivate);

    linux_dmabuf = wlr_linux_dmabuf_create(server.wl_display, server.renderer);

    virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(server.wl_display);

    on_virtual_keyboard_new = [this](void* data) {
      auto& keyboard = *(wlr::virtual_keyboard_v1_t*) data;

      auto* seat = this->server.input.seat_from_wlr_seat(*keyboard.seat);
      if (!seat) {
        LOGE("could not find tablecloth seat");
        return;
      }

      seat->add_device(keyboard.input_device);
    };
    on_virtual_keyboard_new.add_to(virtual_keyboard->events.new_virtual_keyboard);

    screencopy = wlr_screencopy_manager_v1_create(server.wl_display);
  }

  Desktop::~Desktop() noexcept
  {
    // TODO
  }

  Output* Desktop::output_from_wlr_output(wlr::output_t* wlr_output)
  {
    for (auto& output : outputs) {
      if (&output.wlr_output == wlr_output) return &output;
    }
    return nullptr;
  }

  util::ref_vec<View> Desktop::visible_views()
  {
    util::ref_vec<View> res;
    for (auto& ws : workspaces) {
      if (!ws.is_visible()) continue;
      auto views = ws.visible_views();
      res.reserve(res.size() + views.size());
      util::copy(views.underlying(), std::back_inserter(res.underlying()));
    }
    return res;
  }

  auto Desktop::current_workspace() -> Workspace&
  {
    return *outputs.front().workspace;
  }

  auto Desktop::switch_to_workspace(int idx) -> Workspace&
  {
    assert(idx >= 0 && idx < workspace_count);
    for (auto& seat : server.input.seats) {
      seat.set_focus(workspaces[idx].focused_view());
    }
    outputs.front().workspace = &workspaces[idx];
    outputs.front().damage_whole();
    server.workspace_manager.send_state();
    return workspaces[idx];
  }

} // namespace cloth
