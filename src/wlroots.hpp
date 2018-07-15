#pragma once

#include <functional>

extern "C" {

#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_linux_dmabuf.h>
#include <wlr/types/wlr_list.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_shell_v6.h>

#ifdef WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
}

#include "util/bindings.hpp"

namespace cloth::wl {

  using argument_t = struct wl_argument;
  using array_t = struct wl_array;
  using buffer_t = struct wl_buffer;
  using callback_t = struct wl_callback;
  using client_t = struct wl_client;
  using compositor_t = struct wl_compositor;
  using data_device_t = struct wl_data_device;
  using data_offer_t = struct wl_data_offer;
  using data_source_t = struct wl_data_source;
  using display_error_t = enum wl_display_error;
  using display_t = struct wl_display;
  using event_loop_t = struct wl_event_loop;
  using event_source_t = struct wl_event_source;
  using global_t = struct wl_global;
  using interface_t = struct wl_interface;
  using keyboard_t = struct wl_keyboard;
  using listener_t = struct wl_listener;
  using list_t = struct wl_list;
  using message_t = struct wl_message;
  using object_t = struct wl_object;
  using output_mode_t = enum wl_output_mode;
  using output_transform_t = enum wl_output_transform;
  using output_t = struct wl_output;
  using pointer_axis_t = enum wl_pointer_axis;
  using pointer_error_t = enum wl_pointer_error;
  using pointer_t = struct wl_pointer;
  using region_t = struct wl_region;
  using registry_t = struct wl_registry;
  using resource_t = struct wl_resource;
  using seat_t = struct wl_seat;
  using shell_error_t = enum wl_shell_error;
  using shell_t = struct wl_shell;
  using shm_buffer_t = struct wl_shm_buffer;
  using shm_error_t = enum wl_shm_error;
  using shm_format_t = enum wl_shm_format;
  using shm_pool_t = struct wl_shm_pool;
  using signal_t = struct wl_signal;
  using subcompositor_error_t = enum wl_subcompositor_error;
  using subsurface_error_t = enum wl_subsurface_error;
  using subsurface_t = struct wl_subsurface;
  using surface_error_t = enum wl_surface_error;
  using surface_t = struct wl_surface;
  using touch_t = struct wl_touch;

  struct Signal {

    Signal() noexcept {
      wl_signal_init(&signal);
    }

    ~Signal() noexcept { }

    void add(listener_t& listener) {
      wl_signal_add(&signal, &listener);
    }

    void emit(void* data = nullptr) {
      wl_signal_emit(&signal, data);
    }

    operator signal_t&() {
      return signal;
    }

    operator signal_t*() {
      return &signal;
    }
  private:
    signal_t signal;
  };

  struct Listener {
    Listener(std::function<void(void*)> func) noexcept
      : _func(new std::function<void(void*)>(std::move(func)))
    {
      _listener.link = {nullptr, nullptr};
      _listener.notify = [](wl_listener* lst, void* data) {
        Listener& self = *wl_container_of(lst, &self, _listener);
        if (self._func && *self._func) (*self._func)(data);
      };
    }

    Listener(std::function<void()> func) noexcept
      : Listener([f = std::move(func)] (void*) { f(); })
    {}

    Listener(std::nullptr_t) noexcept
      : Listener(std::function<void(void*)>(nullptr))
    {}

    Listener() noexcept : Listener(nullptr) {}

    ~Listener() noexcept
    {
      if (_func) delete _func;
      if (_listener.link.next) wl_list_remove(&_listener.link);
    }

    Listener(const Listener& rhs) = delete;

    Listener(Listener&& rhs) noexcept
    {
      swap(*this, rhs);
    }

    Listener& operator=(Listener&& rhs) noexcept
    {
      swap(*this, rhs);
      return *this;
    }

    Listener& operator=(std::function<void(void*)> func) noexcept
    {
      return this->operator=(Listener(func));
    }

    Listener& operator=(std::function<void()> func) noexcept
    {
      return this->operator=(Listener(func));
    }

    Listener& operator=(std::nullptr_t func) noexcept
    {
      return this->operator=(Listener(func));
    }

    operator wl_listener&() noexcept
    {
      return _listener;
    }

    operator const wl_listener&() const noexcept
    {
      return _listener;
    }

    friend void swap(Listener& lhs, Listener& rhs) noexcept
    {
      std::swap(lhs._func, rhs._func);
      std::swap(lhs._listener, rhs._listener);
    }

    void add_to(wl_signal& sig) noexcept
    {
      wl_signal_add(&sig, &_listener);
    }

    void remove() noexcept {
      wl_list_remove(&_listener.link);
      _listener.link = {nullptr, nullptr};
    }

  private:
    std::function<void(void* data)>* _func;
    struct wl_listener _listener;
  };

} // namespace cloth::wl

namespace cloth::wlr {

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
  using device_t = struct wlr_device;
  using dmabuf_buffer_t = struct wlr_dmabuf_buffer;
  using drag_icon_t = struct wlr_drag_icon;
  using drag_motion_event_t = struct wlr_drag_motion_event;
  using drag_t = struct wlr_drag;
  using edges_t = enum wlr_edges;
  using egl_t = struct wlr_egl;
  using event_keyboard_key_t = struct wlr_event_keyboard_key;
  using event_pointer_axis_t = struct wlr_event_pointer_axis;
  using event_pointer_button_t = struct wlr_event_pointer_button;
  using event_pointer_motion_absolute_t = struct wlr_event_pointer_motion_absolute;
  using event_pointer_motion_t = struct wlr_event_pointer_motion;
  using event_tablet_tool_axis_t = struct wlr_event_tablet_tool_axis;
  using event_tablet_tool_tip_t = struct wlr_event_tablet_tool_tip;
  using event_touch_down_t = struct wlr_event_touch_down;
  using event_touch_motion_t = struct wlr_event_touch_motion;
  using event_touch_up_t = struct wlr_event_touch_up;
  using export_dmabuf_manager_v1_t = struct wlr_export_dmabuf_manager_v1;
  using gamma_control_manager_t = struct wlr_gamma_control_manager;
  using gamma_control_t = struct wlr_gamma_control;
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
  using layer_surface_t = struct wlr_layer_surface;
  using linux_dmabuf_t = struct wlr_linux_dmabuf;
  using list_t = struct wlr_list;
  using output_cursor_t = struct wlr_output_cursor;
  using output_damage_t = struct wlr_output_damage;
  using output_impl_t = struct wlr_output_impl;
  using output_layout_state_t = struct wlr_output_layout_state;
  using output_layout_output_t = struct wlr_output_layout_output;
  using output_layout_output_state_t = struct wlr_output_layout_output_state;
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
  using seat_t = struct wlr_seat;
  using server_decoration_manager_t = struct wlr_server_decoration_manager;
  using session_t = struct wlr_session;
  using subcompositor_t = struct wlr_subcompositor;
  using subsurface_state_t = struct wlr_subsurface_state;
  using subsurface_t = struct wlr_subsurface;
  using surface_role_t = struct wlr_surface_role;
  using surface_state_t = struct wlr_surface_state;
  using surface_t = struct wlr_surface;
  using tablet_pad_impl_t = struct wlr_tablet_pad_impl;
  using tablet_pad_t = struct wlr_tablet_pad;
  using tablet_tool_t = struct wlr_tablet_tool;
  using texture_impl_t = struct wlr_texture_impl;
  using texture_t = struct wlr_texture;
  using touch_impl_t = struct wlr_touch_impl;
  using touch_point_t = struct wlr_touch_point;
  using touch_t = struct wlr_touch;
  using virtual_keyboard_v1_t = struct wlr_virtual_keyboard_v1;
  using virtual_keyboard_manager_v1_t = struct wlr_virtual_keyboard_manager_v1;
  using wl_shell_surface_t = struct wlr_wl_shell_surface;
  using wl_shell_t = struct wlr_wl_shell;
  using xcursor_image_t = struct wlr_xcursor_image;
  using xcursor_manager_t = struct wlr_xcursor_manager;
  using xcursor_theme_t = struct wlr_xcursor_theme;
  using xcursor_t = struct wlr_xcursor;
  using xdg_client_t = struct wlr_xdg_client;
  using xdg_client_v6_t = struct wlr_xdg_client_v6;
  using xdg_popup_grab_t = struct wlr_xdg_popup_grab;
  using xdg_popup_grab_v6_t = struct wlr_xdg_popup_grab_v6;
  using xdg_popup_t = struct wlr_xdg_popup;
  using xdg_popup_v6_t = struct wlr_xdg_popup_v6;
  using xdg_positioner_t = struct wlr_xdg_positioner;
  using xdg_positioner_v6_t = struct wlr_xdg_positioner_v6;
  using xdg_shell_t = struct wlr_xdg_shell;
  using xdg_shell_v6_t = struct wlr_xdg_shell_v6;
  using xdg_surface_t = struct wlr_xdg_surface;
  using xdg_surface_v6_t = struct wlr_xdg_surface_v6;
  using xdg_toplevel_t = struct wlr_xdg_toplevel;
  using xdg_toplevel_v6_t = struct wlr_xdg_toplevel_v6;
  using xwm_t = struct wlr_xwm;

#ifdef WLR_HAS_XWAYLAND
  using xwayland_surface_t = struct wlr_xwayland_surface;
  using xwayland_t = struct wlr_xwayland;
#endif

  [[deprecated]]
  using Listener = wl::Listener;
} // namespace cloth::wlr
