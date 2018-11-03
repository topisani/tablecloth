#include "util/logging.hpp"

#include "layers.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"
#include "wlroots.hpp"
#include "xcursor.hpp"

#include "util/exception.hpp"
#include "util/iterators.hpp"

#include <sys/wait.h>
#include <unistd.h>
#include <charconv>

#include "wlr-layer-shell-unstable-v1-protocol.h"

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
        wlr_layer_surface_v1_surface_at(&surface.layer_surface, _sx, _sy, &sx, &sy);

      if (sub) {
        return sub;
      }
    }
    return nullptr;
  }

  Output* Desktop::output_at(double x, double y)
  {
    wlr::output_t* wlr_output = wlr_output_layout_output_at(layout, x, y);
    if (wlr_output) return output_from_wlr_output(wlr_output);
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

      if ((surface = layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                                      ox, oy, sx, sy))) {
        return surface;
      }
      if ((surface = layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], ox,
                                      oy, sx, sy))) {
        return surface;
      }
    }

    View* _view;
    if ((_view = view_at(lx, ly, surface, sx, sy))) {
      view = _view;
      return surface;
    }

    if (wlr_output) {
      if ((surface = layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], ox,
                                      oy, sx, sy))) {
        return surface;
      }
      if ((surface = layer_surface_at(*output, output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
                                      ox, oy, sx, sy))) {
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
    cloth_debug("Initializing tablecloth desktop");

    on_new_output = [this](void* data) {
      cloth_debug("New output");
      outputs.emplace_back(*this, workspaces[0], *(wlr::output_t*) data);

      for (auto& seat : server.input.seats) {
        seat.configure_cursor();
        seat.configure_xcursor();
      }
    };
    on_new_output.add_to(server.backend->events.new_output);

    layout = wlr_output_layout_create();
    wlr_xdg_output_manager_v1_create(server.wl_display, layout);

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

    layer_shell = wlr_layer_shell_v1_create(server.wl_display);
    on_layer_shell_surface.add_to(layer_shell->events.new_surface);
    on_layer_shell_surface = [this](void* data) { handle_layer_shell_surface(data); };

    tablet_v2 = wlr_tablet_v2_create(server.wl_display);

    const char* cursor_theme = nullptr;
#ifdef WLR_HAS_XWAYLAND
    const char* cursor_default = xcursor_default;
#endif
    Config::Cursor* cc = config.get_cursor(Config::default_seat_name);
    if (cc != nullptr) {
      cursor_theme = cc->theme.c_str();
#ifdef WLR_HAS_XWAYLAND
      if (!cc->default_image.empty()) {
        cursor_default = cc->default_image.c_str();
      }
#endif
    }

    char cursor_size_fmt[16];
    snprintf(cursor_size_fmt, sizeof(cursor_size_fmt), "%d", xcursor_size);
    setenv("XCURSOR_SIZE", cursor_size_fmt, 1);
    if (cursor_theme != NULL) {
      setenv("XCURSOR_THEME", cursor_theme, 1);
    }
#ifdef WLR_HAS_XWAYLAND

    xcursor_manager = wlr_xcursor_manager_create(cursor_theme, xcursor_size);
    if (xcursor_manager == nullptr) {
      cloth_error("Cannot create XCursor manager for theme {}", cursor_theme);
    }

    if (config.xwayland) {
      xwayland = wlr_xwayland_create(server.wl_display, compositor, config.xwayland_lazy);
      on_xwayland_surface.add_to(xwayland->events.new_surface);
      on_xwayland_surface = [this](void* data) { handle_xwayland_surface(data); };

      if (wlr_xcursor_manager_load(xcursor_manager, 1)) {
        cloth_error("Cannot load XWayland XCursor theme");
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
    gamma_control_manager_v1 = wlr_gamma_control_manager_v1_create(server.wl_display);
    screenshooter = wlr_screenshooter_create(server.wl_display);
    export_dmabuf_manager_v1 = wlr_export_dmabuf_manager_v1_create(server.wl_display);
    server_decoration_manager = wlr_server_decoration_manager_create(server.wl_display);
    wlr_server_decoration_manager_set_default_mode(server_decoration_manager,
                                                   WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    on_server_decoration.add_to(server_decoration_manager->events.new_decoration);
    on_server_decoration = [](void* data) {
      auto* decoration = (wlr::server_decoration_t*) data;
      // set in View::map and View::unmap
      auto* view = (View*) decoration->surface->data;
      if (view) {
        cloth_debug("Updated server decoration for view");
        view->deco.set_visible(decoration->mode == WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
      } else {
        cloth_debug("No view found for server decoration");
      }
    };

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

    input_method = wlr_input_method_manager_v2_create(server.wl_display);
    text_input = wlr_text_input_manager_v3_create(server.wl_display);

    virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(server.wl_display);

    on_virtual_keyboard_new = [this](void* data) {
      cloth_debug("New virtual keyboard");
      auto& keyboard = *(wlr::virtual_keyboard_v1_t*) data;

      auto* seat = this->server.input.seat_from_wlr_seat(*keyboard.seat);
      if (!seat) {
        cloth_error("could not find tablecloth seat");
        return;
      }

      auto& device = seat->add_device(keyboard.input_device);
      device.on_device_destroy.add_to(keyboard.events.destroy);
    };
    on_virtual_keyboard_new.add_to(virtual_keyboard->events.new_virtual_keyboard);

    screencopy = wlr_screencopy_manager_v1_create(server.wl_display);

    xdg_decoration_manager_v1 = wlr_xdg_decoration_manager_v1_create(server.wl_display);
    on_xdg_toplevel_decoration = [this](void* data) { handle_xdg_toplevel_decoration(data); };
    on_xdg_toplevel_decoration.add_to(xdg_decoration_manager_v1->events.new_toplevel_decoration);

    pointer_constraints = wlr_pointer_constraints_v1_create(server.wl_display);
    on_pointer_constraint.add_to(pointer_constraints->events.new_constraint);
    on_pointer_constraint = [](void* data) {
      auto& wlr_constraint = *((wlr_pointer_constraint_v1*) data);
      new PointerConstraint(&wlr_constraint);

    };
    presentation = wlr_presentation_create(server.wl_display, server.backend);
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

  auto Desktop::current_output() -> Output&
  {
    auto* output = server.input.last_active_seat()->current_output();
    if (!output) output = &outputs[0];
    return *output;
  }

  auto Desktop::current_workspace() -> Workspace&
  {
    return *current_output().workspace;
  }

  auto Desktop::switch_to_workspace(int idx) -> Workspace&
  {
    assert(idx >= 0 && idx < workspace_count);
    for (auto& seat : server.input.seats) {
      seat.set_focus(workspaces[idx].focused_view());
    }
    auto& output = current_output();
    output.workspace = &workspaces[idx];
    output.context.damage_whole();
    for (auto& view : workspaces[idx].visible_views()) {
      view.arrange();
    }
    server.workspace_manager.send_state();
    server.window_manager.send_focused_window_name(workspaces[idx]);
    return workspaces[idx];
  }

  static bool outputs_enabled = true;

  static auto execute(const char* command)
  {
    auto pid = fork();
    if (pid < 0) {
      throw util::exception("Fork failed");
    }

    if (pid == 0) {
      pid = fork();
      if (pid == 0) {
        execl("/bin/sh", "/bin/sh", "-c", command, nullptr);
        _exit(EXIT_FAILURE);
      } else {
        _exit(pid == -1);
      }
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw util::exception("waitpid() on the first child failed: {}", std::strerror(errno));
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      throw util::exception("first child failed to fork command");
    }
  }

  void Desktop::run_command(std::string_view command_str)
  {
    Input& input = server.input;

    try {
      std::vector<std::string> args = util::split_string(std::string(command_str), " ");
      std::string command = args.at(0);
      args.erase(args.begin());

      if (command == "exit") {
        wl_display_terminate(input.server.wl_display);
      } else if (command == "close") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          focus->close();
        }
      } else if (command == "center") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          focus->center();
        }
      } else if (command == "fullscreen") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          bool is_fullscreen = focus->fullscreen_output != nullptr;
          focus->set_fullscreen(!is_fullscreen, nullptr);
        }
      } else if (command == "next_window") {
        current_workspace().cycle_focus();
      } else if (command == "alpha") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          focus->cycle_alpha();
        }
      } else if (command == "exec") {
        std::string shell_cmd = std::string(command_str.substr(strlen("exec ")));
        execute(shell_cmd.c_str());
      } else if (command == "maximize") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          focus->maximize(!focus->maximized);
        }
      } else if (command == "nop") {
        cloth_debug("nop command");
      } else if (command == "toggle_outputs") {
        outputs_enabled = !outputs_enabled;
        for (auto& output : outputs) {
          wlr_output_enable(&output.wlr_output, outputs_enabled);
        }
      } else if (command == "switch_workspace") {
        int workspace = -1;
        auto ws_str = args.at(0);
        if (ws_str == "next") // +10 fixes wrapping backwards
          workspace = (current_workspace().index + 1 + 10) % 10;
        else if (ws_str == "prev")
          workspace = (current_workspace().index - 1 + 10) % 10;
        else
          std::from_chars(&*ws_str.begin(), &*ws_str.end(), workspace);
        if (workspace >= 0 && workspace < 10) {
          switch_to_workspace(workspace);
        }
      } else if (command == "move_workspace") {
        View* focus = current_workspace().focused_view();
        if (focus != nullptr) {
          int workspace = -1;
          auto ws_str = args.at(0);
          if (ws_str == "next")
            workspace = (current_workspace().index + 1 + 10) % 10;
          else if (ws_str == "prev")
            workspace = (current_workspace().index - 1 + 10) % 10;
          else
            std::from_chars(&*ws_str.begin(), &*ws_str.end(), workspace);
          if (workspace >= 0 && workspace < 10) {
            workspaces.at(workspace).add_view(focus->workspace->erase_view(*focus));
          }
        }
      } else if (command == "toggle_decoration_mode") {
        View* focus = current_workspace().focused_view();
        if (auto xdg = dynamic_cast<XdgSurface*>(focus); xdg) {
          auto* decoration = xdg->xdg_toplevel_decoration.get();
          if (decoration) {
            auto mode = decoration->wlr_decoration.current_mode;
            mode = (mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
                     ? WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                     : WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
            wlr_xdg_toplevel_decoration_v1_set_mode(&decoration->wlr_decoration, mode);
          }
        }
      } else if (command == "rotate_output") {
        auto rotation = args.at(0);
        auto output_name = args.size() > 1 ? args.at(1) : "";
        auto output =
          util::find_if(outputs, [&](Output& o) { return o.wlr_output.name == output_name; });
        if (output == outputs.end()) output = outputs.begin();
        auto transform = [&] {
          if (rotation == "0") return WL_OUTPUT_TRANSFORM_NORMAL;
          if (rotation == "90") return WL_OUTPUT_TRANSFORM_90;
          if (rotation == "180") return WL_OUTPUT_TRANSFORM_180;
          if (rotation == "270") return WL_OUTPUT_TRANSFORM_270;
          throw util::exception("Invalid rotation. Expected 0,90,180 or 270. Got {}", rotation);
        }();
        wlr_output_set_transform(&output->wlr_output, transform);
      } else {
        cloth_error("unknown binding command: {}", command);
      }
    } catch (std::exception& e) {
      cloth_error("Error running command: {}", e.what());
    }
  }

} // namespace cloth
