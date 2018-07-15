#pragma once

#include <chrono>

#include "wlroots.hpp"

#include "util/chrono.hpp"

#include "config.hpp"
#include "output.hpp"
#include "view.hpp"

namespace cloth {

  struct Server;

  struct Desktop {
    Desktop(Server& server, Config& config) noexcept;

    ~Desktop() noexcept;

    Output* output_from_wlr_output(struct wlr_output* output);

    wlr::surface_t* surface_at(double lx,
                               double ly,
                               double& sx,
                               double& sy,
                               View*& view);

  private:
    View* view_at(double lx, double ly, wlr::surface_t*& surface, double& sx, double& sy);

    // These are implemented in the src/*_shell.cpp files
    void handle_xdg_shell_v6_surface(void* data);
    void handle_xdg_shell_surface(void* data);
    void handle_wl_shell_surface(void* data);
    void handle_layer_shell_surface(void* data);
    void handle_xwayland_surface(void* data);

  public:

    // DATA //

    std::vector<View> views; // roots_view::link

    std::vector<Output> outputs; // roots_output::link
    chrono::time_point last_frame;

    Server& server;
    Config& config;

    wlr::output_layout_t* layout;
    wlr::xcursor_manager_t* xcursor_manager;

    wlr::compositor_t* compositor;
    wlr::wl_shell_t* wl_shell;
    wlr::xdg_shell_v6_t* xdg_shell_v6;
    wlr::xdg_shell_t* xdg_shell;
    wlr::gamma_control_manager_t* gamma_control_manager;
    wlr::screenshooter_t* screenshooter;
    wlr::export_dmabuf_manager_v1_t* export_dmabuf_manager_v1;
    wlr::server_decoration_manager_t* server_decoration_manager;
    wlr::primary_selection_device_manager_t* primary_selection_device_manager;
    wlr::idle_t* idle;
    wlr::idle_inhibit_manager_v1_t* idle_inhibit;
    wlr::input_inhibit_manager_t* input_inhibit;
    wlr::linux_dmabuf_t* linux_dmabuf;
    wlr::layer_shell_t* layer_shell;
    wlr::virtual_keyboard_manager_v1_t* virtual_keyboard;
    wlr::screencopy_manager_v1_t* screencopy;

    wlr::Listener on_new_output;
    wlr::Listener on_layout_change;
    wlr::Listener on_xdg_shell_v6_surface;
    wlr::Listener on_xdg_shell_surface;
    wlr::Listener on_wl_shell_surface;
    wlr::Listener on_layer_shell_surface;
    wlr::Listener on_decoration_new;
    wlr::Listener on_input_inhibit_activate;
    wlr::Listener on_input_inhibit_deactivate;
    wlr::Listener on_virtual_keyboard_new;

#ifdef WLR_HAS_XWAYLAND
    wlr::xwayland_t* xwayland;
    wl::Listener on_xwayland_surface;
#endif
  };

} // namespace cloth
