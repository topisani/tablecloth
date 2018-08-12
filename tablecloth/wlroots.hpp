#pragma once

#include <functional>

// All includes used in wlr files
// This is done to make sure the keyword macros don't bleed into
// these files, which may be c++-aware
//
// Generate:
// grep -rP "#include <(?\!wlr)" subprojects/wlroots/include/wlr | cut -d: -f2 | sort | uniq

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <errno.h>
#include <libinput.h>
#include <libudev.h>
#include <pixman.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xcb/xcb.h>
#include <xf86drmMode.h>
#include <xkbcommon/xkbcommon.h>

// Rename fields which use reserved names
#define class class_
#define namespace namespace_
// wlroots does not do this itself
extern "C" {

#define WLR_USE_UNSTABLE 1

#include <backend/drm/drm.h>
#include <wlr/backend.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_list.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/util/region.h>

#ifdef WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
}

// make sure to undefine these again
#undef class
#undef namespace

#include "util/bindings.hpp"

#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif

#undef Bool
#undef True
#undef False
#undef Status

#include "wayland.hpp"

namespace cloth::wl {} // namespace cloth::wl

namespace cloth::wlr {


  using axis_source_t = enum wlr_axis_source;
  using backend_impl_t = struct wlr_backend_impl;
  using backend_t = struct wlr_backend;
  using box_t = struct wlr_box;
  using buffer_t = struct wlr_buffer;
  using button_state_t = enum wlr_button_state;
  using compositor_t = struct wlr_compositor;
  using cursor_state_t = struct wlr_cursor_state;
  using cursor_t = struct wlr_cursor;
  using data_device_manager_t = struct wlr_data_device_manager;
  using data_offer_t = struct wlr_data_offer;
  using data_source_impl_t = struct wlr_data_source_impl;
  using data_source_t = struct wlr_data_source;
  using device_t = struct wlr_device;
  using direction_t = enum wlr_direction;
  using dmabuf_buffer_t = struct wlr_dmabuf_buffer;
  using drag_icon_t = struct wlr_drag_icon;
  using drag_motion_event_t = struct wlr_drag_motion_event;
  using drag_t = struct wlr_drag;
  using drm_backend_t = struct wlr_drm_backend;
  using drm_connector_t = struct wlr_drm_connector;
  using drm_crtc_props_t = union wlr_drm_crtc_props;
  using drm_crtc_t = struct wlr_drm_crtc;
  using drm_interface_t = struct wlr_drm_interface;
  using drm_mode_t = struct wlr_drm_mode;
  using drm_plane_t = struct wlr_drm_plane;
  using drm_renderer_t = struct wlr_drm_renderer;
  using drm_surface_t = struct wlr_drm_surface;
  using edges_t = enum wlr_edges;
  using egl_t = struct wlr_egl;
  using event_keyboard_key_t = struct wlr_event_keyboard_key;
  using event_pointer_axis_t = struct wlr_event_pointer_axis;
  using event_pointer_button_t = struct wlr_event_pointer_button;
  using event_pointer_motion_absolute_t = struct wlr_event_pointer_motion_absolute;
  using event_pointer_motion_t = struct wlr_event_pointer_motion;
  using event_tablet_tool_axis_t = struct wlr_event_tablet_tool_axis;
  using event_tablet_tool_tip_t = struct wlr_event_tablet_tool_tip;
  using event_tablet_tool_button_t = struct wlr_event_tablet_tool_button;
  using event_tablet_tool_proximity_t = struct wlr_event_tablet_tool_proximity;
  using event_tablet_pad_ring_t = struct wlr_event_tablet_pad_ring;
  using event_tablet_pad_strip_t = struct wlr_event_tablet_pad_strip;
  using event_tablet_pad_button_t = struct wlr_event_tablet_pad_button;
  using event_touch_down_t = struct wlr_event_touch_down;
  using event_touch_motion_t = struct wlr_event_touch_motion;
  using event_touch_up_t = struct wlr_event_touch_up;
  using export_dmabuf_manager_v1_t = struct wlr_export_dmabuf_manager_v1;
  using gamma_control_manager_t = struct wlr_gamma_control_manager;
  using gamma_control_manager_v1_t = struct wlr_gamma_control_manager_v1;
  using gamma_control_t = struct wlr_gamma_control;
  using gamma_control_v1_t = struct wlr_gamma_control;
  using idle_inhibit_manager_v1_t = struct wlr_idle_inhibit_manager_v1;
  using idle_inhibitor_v1_t = struct wlr_idle_inhibitor_v1;
  using idle_timeout_t = struct wlr_idle_timeout;
  using idle_t = struct wlr_idle;
  using input_device_impl_t = struct wlr_input_device_impl;
  using input_device_t = struct wlr_input_device;
  using input_inhibit_manager_t = struct wlr_input_inhibit_manager;
  using keyboard_impl_t = struct wlr_keyboard_impl;
  using keyboard_led_t = enum wlr_keyboard_led;
  using keyboard_t = struct wlr_keyboard;
  using key_state_t = enum wlr_key_state;
  using layer_shell_t = struct wlr_layer_shell;
  using layer_surface_state_t = struct wlr_layer_surface_state;
  using layer_surface_t = struct wlr_layer_surface;
  using linux_dmabuf_t = struct wlr_linux_dmabuf;
  using linux_dmabuf_v1_t = struct wlr_linux_dmabuf_v1;
  using list_t = struct wlr_list;
  using output_cursor_t = struct wlr_output_cursor;
  using output_damage_t = struct wlr_output_damage;
  using output_impl_t = struct wlr_output_impl;
  using output_layout_output_state_t = struct wlr_output_layout_output_state;
  using output_layout_output_t = struct wlr_output_layout_output;
  using output_layout_state_t = struct wlr_output_layout_state;
  using output_layout_t = struct wlr_output_layout;
  using output_mode_t = struct wlr_output_mode;
  using output_t = struct wlr_output;
  using pointer_impl_t = struct wlr_pointer_impl;
  using pointer_t = struct wlr_pointer;
  using primary_selection_device_manager_t = struct wlr_primary_selection_device_manager;
  using renderer_impl_t = struct wlr_renderer_impl;
  using renderer_t = struct wlr_renderer;
  using screencopy_frame_v1_t = struct wlr_screencopy_frame_v1;
  using screencopy_manager_v1_t = struct wlr_screencopy_manager_v1;
  using screenshooter_t = struct wlr_screenshooter;
  using screenshot_t = struct wlr_screenshot;
  using seat_client_t = struct wlr_seat_client;
  using seat_keyboard_grab_t = struct wlr_seat_keyboard_grab;
  using seat_keyboard_state_t = struct wlr_seat_keyboard_state;
  using seat_pointer_request_set_cursor_event_t = struct wlr_seat_pointer_request_set_cursor_event;
  using seat_pointer_state_t = struct wlr_seat_pointer_state;
  using seat_touch_grab_t = struct wlr_seat_touch_grab;
  using seat_t = struct wlr_seat;
  using server_decoration_manager_t = struct wlr_server_decoration_manager;
  using server_decoration_t = struct wlr_server_decoration;
  using session_t = struct wlr_session;
  using subcompositor_t = struct wlr_subcompositor;
  using subsurface_state_t = struct wlr_subsurface_state;
  using subsurface_t = struct wlr_subsurface;
  using surface_role_t = struct wlr_surface_role;
  using surface_state_t = struct wlr_surface_state;
  using surface_t = struct wlr_surface;
  using tablet_client_v2_t = struct wlr_tablet_client_v2;
  using tablet_impl_t = struct wlr_tablet_impl;
  using tablet_manager_v2_t = struct wlr_tablet_manager_v2;
  using tablet_pad_client_v2_t = struct wlr_tablet_pad_client_v2;
  using tablet_pad_impl_t = struct wlr_tablet_pad_impl;
  using tablet_pad_t = struct wlr_tablet_pad;
  using tablet_tool_client_v2_t = struct wlr_tablet_tool_client_v2;
  using tablet_tool_proximity_state_t = enum wlr_tablet_tool_proximity_state;
  using tablet_tool_t = struct wlr_tablet_tool;
  using tablet_t = struct wlr_tablet;
  using tablet_v2_event_cursor_t = struct wlr_tablet_v2_event_cursor;
  using tablet_v2_event_feedback_t = struct wlr_tablet_v2_event_feedback;
  using tablet_v2_tablet_pad_t = struct wlr_tablet_v2_tablet_pad;
  using tablet_v2_tablet_tool_t = struct wlr_tablet_v2_tablet_tool;
  using tablet_v2_tablet_t = struct wlr_tablet_v2_tablet;
  using texture_impl_t = struct wlr_texture_impl;
  using texture_t = struct wlr_texture;
  using touch_impl_t = struct wlr_touch_impl;
  using touch_point_t = struct wlr_touch_point;
  using touch_t = struct wlr_touch;
  using virtual_keyboard_manager_v1_t = struct wlr_virtual_keyboard_manager_v1;
  using virtual_keyboard_v1_t = struct wlr_virtual_keyboard_v1;
  using wl_shell_popup_grab_t = struct wlr_wl_shell_popup_grab;
  using wl_shell_surface_maximize_event_t = struct wlr_wl_shell_surface_maximize_event;
  using wl_shell_surface_move_event_t = struct wlr_wl_shell_surface_move_event;
  using wl_shell_surface_popup_state_t = struct wlr_wl_shell_surface_popup_state;
  using wl_shell_surface_resize_event_t = struct wlr_wl_shell_surface_resize_event;
  using wl_shell_surface_set_fullscreen_event_t = struct wlr_wl_shell_surface_set_fullscreen_event;
  using wl_shell_surface_transient_state_t = struct wlr_wl_shell_surface_transient_state;
  using wl_shell_surface_t = struct wlr_wl_shell_surface;
  using wl_shell_t = struct wlr_wl_shell;
  using xcursor_image_t = struct wlr_xcursor_image;
  using xcursor_manager_t = struct wlr_xcursor_manager;
  using xcursor_theme_t = struct wlr_xcursor_theme;
  using xcursor_t = struct wlr_xcursor;
  using xdg_client_t = struct wlr_xdg_client;
  using xdg_client_v6_t = struct wlr_xdg_client_v6;
  using xdg_decoration_manager_v1_t = struct wlr_xdg_decoration_manager_v1;
  using xdg_output_manager_t = struct wlr_xdg_output_manager;
  using xdg_output_t = struct wlr_xdg_output;
  using xdg_popup_grab_t = struct wlr_xdg_popup_grab;
  using xdg_popup_grab_v6_t = struct wlr_xdg_popup_grab_v6;
  using xdg_popup_t = struct wlr_xdg_popup;
  using xdg_popup_v6_t = struct wlr_xdg_popup_v6;
  using xdg_positioner_t = struct wlr_xdg_positioner;
  using xdg_positioner_v6_t = struct wlr_xdg_positioner_v6;
  using xdg_shell_t = struct wlr_xdg_shell;
  using xdg_shell_v6_t = struct wlr_xdg_shell_v6;
  using xdg_surface_configure_t = struct wlr_xdg_surface_configure;
  using xdg_surface_role_t = enum wlr_xdg_surface_role;
  using xdg_surface_t = struct wlr_xdg_surface;
  using xdg_surface_v6_configure_t = struct wlr_xdg_surface_v6_configure;
  using xdg_surface_v6_role_t = enum wlr_xdg_surface_v6_role;
  using xdg_surface_v6_t = struct wlr_xdg_surface_v6;
  using xdg_toplevel_decoration_v1_configure_t = struct wlr_xdg_toplevel_decoration_v1_configure;
  using xdg_toplevel_decoration_v1_mode_t = enum wlr_xdg_toplevel_decoration_v1_mode;
  using xdg_toplevel_decoration_v1_t = struct wlr_xdg_toplevel_decoration_v1;
  using xdg_toplevel_move_event_t = struct wlr_xdg_toplevel_move_event;
  using xdg_toplevel_resize_event_t = struct wlr_xdg_toplevel_resize_event;
  using xdg_toplevel_set_fullscreen_event_t = struct wlr_xdg_toplevel_set_fullscreen_event;
  using xdg_toplevel_show_window_menu_event_t = struct wlr_xdg_toplevel_show_window_menu_event;
  using xdg_toplevel_state_t = struct wlr_xdg_toplevel_state;
  using xdg_toplevel_t = struct wlr_xdg_toplevel;
  using xdg_toplevel_v6_move_event_t = struct wlr_xdg_toplevel_v6_move_event;
  using xdg_toplevel_v6_resize_event_t = struct wlr_xdg_toplevel_v6_resize_event;
  using xdg_toplevel_v6_set_fullscreen_event_t = struct wlr_xdg_toplevel_v6_set_fullscreen_event;
  using xdg_toplevel_v6_show_window_menu_event_t =
    struct wlr_xdg_toplevel_v6_show_window_menu_event;
  using xdg_toplevel_v6_state_t = struct wlr_xdg_toplevel_v6_state;
  using xdg_toplevel_v6_t = struct wlr_xdg_toplevel_v6;
  using xwm_t = struct wlr_xwm;

#ifdef WLR_HAS_XWAYLAND
  using xwayland_cursor_t = struct wlr_xwayland_cursor;
  using xwayland_move_event_t = struct wlr_xwayland_move_event;
  using xwayland_resize_event_t = struct wlr_xwayland_resize_event;
  using xwayland_surface_configure_event_t = struct wlr_xwayland_surface_configure_event;
  using xwayland_surface_decorations_t = enum wlr_xwayland_surface_decorations;
  using xwayland_surface_hints_t = struct wlr_xwayland_surface_hints;
  using xwayland_surface_size_hints_t = struct wlr_xwayland_surface_size_hints;
  using xwayland_surface_t = struct wlr_xwayland_surface;
  using xwayland_t = struct wlr_xwayland;
#endif

  using Listener[[deprecated("Use wl::Listener instead")]] = wl::Listener;

  /// Linux input event buttons
  enum struct Button {
    misc = BTN_MISC,
    n0 = BTN_0,
    n1 = BTN_1,
    n2 = BTN_2,
    n3 = BTN_3,
    n4 = BTN_4,
    n5 = BTN_5,
    n6 = BTN_6,
    n7 = BTN_7,
    n8 = BTN_8,
    n9 = BTN_9,

    mouse = BTN_MOUSE,
    left = BTN_LEFT,
    right = BTN_RIGHT,
    middle = BTN_MIDDLE,
    side = BTN_SIDE,
    extra = BTN_EXTRA,
    forward = BTN_FORWARD,
    back = BTN_BACK,
    task = BTN_TASK,

    joystick = BTN_JOYSTICK,
    trigger = BTN_TRIGGER,
    thumb = BTN_THUMB,
    thumb2 = BTN_THUMB2,
    top = BTN_TOP,
    top2 = BTN_TOP2,
    pinkie = BTN_PINKIE,
    base = BTN_BASE,
    base2 = BTN_BASE2,
    base3 = BTN_BASE3,
    base4 = BTN_BASE4,
    base5 = BTN_BASE5,
    base6 = BTN_BASE6,
    dead = BTN_DEAD,

    gamepad = BTN_GAMEPAD,
    south = BTN_SOUTH,
    a = BTN_A,
    east = BTN_EAST,
    b = BTN_B,
    c = BTN_C,
    north = BTN_NORTH,
    x = BTN_X,
    west = BTN_WEST,
    y = BTN_Y,
    z = BTN_Z,
    tl = BTN_TL,
    tr = BTN_TR,
    tl2 = BTN_TL2,
    tr2 = BTN_TR2,
    select = BTN_SELECT,
    start = BTN_START,
    mode = BTN_MODE,
    thumbl = BTN_THUMBL,
    thumbr = BTN_THUMBR,

    digi = BTN_DIGI,
    tool_pen = BTN_TOOL_PEN,
    tool_rubber = BTN_TOOL_RUBBER,
    tool_brush = BTN_TOOL_BRUSH,
    tool_pencil = BTN_TOOL_PENCIL,
    tool_airbrush = BTN_TOOL_AIRBRUSH,
    tool_finger = BTN_TOOL_FINGER,
    tool_mouse = BTN_TOOL_MOUSE,
    tool_lens = BTN_TOOL_LENS,
    tool_quinttap = BTN_TOOL_QUINTTAP,
    stylus3 = BTN_STYLUS3,
    touch = BTN_TOUCH,
    stylus = BTN_STYLUS,
    stylus2 = BTN_STYLUS2,
    tool_doubletap = BTN_TOOL_DOUBLETAP,
    tool_tripletap = BTN_TOOL_TRIPLETAP,
    tool_quadtap = BTN_TOOL_QUADTAP,

    wheel = BTN_WHEEL,
    gear_down = BTN_GEAR_DOWN,
    gear_up = BTN_GEAR_UP,
  };

} // namespace cloth::wlr

inline bool operator==(const cloth::wlr::box_t& lhs, const cloth::wlr::box_t& rhs) noexcept
{
  return memcmp(&lhs, &rhs, sizeof(cloth::wlr::box_t)) == 0;
}

inline bool operator!=(const cloth::wlr::box_t& lhs, const cloth::wlr::box_t& rhs) noexcept
{
  return memcmp(&lhs, &rhs, sizeof(cloth::wlr::box_t)) != 0;
}
namespace cloth {
  CLOTH_ENABLE_BITMASK_OPS(wlr::edges_t);
}
