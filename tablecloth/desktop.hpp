#pragma once

#include <chrono>

#include "wlroots.hpp"

#include "util/chrono.hpp"

#include "config.hpp"
#include "output.hpp"
#include "view.hpp"
#include "workspace.hpp"

namespace cloth {

  struct Server;

  struct Desktop {
    Desktop(Server& server, Config& config) noexcept;

    ~Desktop() noexcept;

    Output* output_from_wlr_output(struct wlr_output* output);

    wlr::surface_t* surface_at(double lx, double ly, double& sx, double& sy, View*& view);

    util::ref_vec<View> visible_views();
    Workspace& current_workspace();
    Workspace& switch_to_workspace(int idx);

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

    static constexpr std::size_t workspace_count = 10;

    std::array<Workspace, workspace_count> workspaces;

    util::ptr_vec<Output> outputs;
    chrono::time_point last_frame;

    Server& server;
    Config& config;

    wlr::output_layout_t* layout = nullptr;
    wlr::xcursor_manager_t* xcursor_manager = nullptr;

    wlr::compositor_t* compositor = nullptr;
    wlr::wl_shell_t* wl_shell = nullptr;
    wlr::xdg_shell_v6_t* xdg_shell_v6 = nullptr;
    wlr::xdg_shell_t* xdg_shell = nullptr;
    wlr::gamma_control_manager_t* gamma_control_manager = nullptr;
    wlr::screenshooter_t* screenshooter = nullptr;
    wlr::export_dmabuf_manager_v1_t* export_dmabuf_manager_v1 = nullptr;
    wlr::server_decoration_manager_t* server_decoration_manager = nullptr;
    wlr::primary_selection_device_manager_t* primary_selection_device_manager = nullptr;
    wlr::idle_t* idle = nullptr;
    wlr::idle_inhibit_manager_v1_t* idle_inhibit = nullptr;
    wlr::input_inhibit_manager_t* input_inhibit = nullptr;
    wlr::linux_dmabuf_t* linux_dmabuf = nullptr;
    wlr::layer_shell_t* layer_shell = nullptr;
    wlr::virtual_keyboard_manager_v1_t* virtual_keyboard = nullptr;
    wlr::screencopy_manager_v1_t* screencopy = nullptr;

  protected:
    wl::Listener on_new_output;
    wl::Listener on_layout_change;
    wl::Listener on_xdg_shell_v6_surface;
    wl::Listener on_xdg_shell_surface;
    wl::Listener on_wl_shell_surface;
    wl::Listener on_layer_shell_surface;
    wl::Listener on_decoration_new;
    wl::Listener on_input_inhibit_activate;
    wl::Listener on_input_inhibit_deactivate;
    wl::Listener on_virtual_keyboard_new;

    wl::listener_t test;

#ifdef WLR_HAS_XWAYLAND
  public:
    wlr::xwayland_t* xwayland = nullptr;

  protected:
    wl::Listener on_xwayland_surface;
#endif
  };

} // namespace cloth
