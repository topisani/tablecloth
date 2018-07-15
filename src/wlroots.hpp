#pragma once

#include <functional>

extern "C" {
#include <wlr/backend.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_data_device.h>
}

#include "util/bindings.hpp"

namespace cloth::wlr {

  using backend_impl_t = struct wlr_backend_impl;
  using backend_t = struct wlr_backend;
  using buffer_t = struct wlr_buffer;
  using cursor_t = struct wlr_cursor;
  using data_offer_t = struct wlr_data_offer;
  using data_source_impl_t = struct wlr_data_source_impl;
  using device_t = struct wlr_device;
  using dmabuf_buffer_t = struct wlr_dmabuf_buffer;
  using drag_motion_event_t = struct wlr_drag_motion_event;
  using drag_t = struct wlr_drag;
  using egl_t = struct wlr_egl;
  using event_keyboard_key_t = struct wlr_event_keyboard_key;
  using event_pointer_axis_t = struct wlr_event_pointer_axis;
  using event_touch_down_t = struct wlr_event_touch_down;
  using event_touch_up_t = struct wlr_event_touch_up;
  using gamma_control_t = struct wlr_gamma_control;
  using idle_timeout_t = struct wlr_idle_timeout;
  using idle_t = struct wlr_idle;
  using input_device_impl_t = struct wlr_input_device_impl;
  using input_device_t = struct wlr_input_device;
  using keyboard_impl_t = struct wlr_keyboard_impl;
  using keyboard_t = struct wlr_keyboard;
  using linux_dmabuf_t = struct wlr_linux_dmabuf;
  using output_cursor_t = struct wlr_output_cursor;
  using output_impl_t = struct wlr_output_impl;
  using output_layout_state_t = struct wlr_output_layout_state;
  using output_layout_t = struct wlr_output_layout;
  using output_mode_t = struct wlr_output_mode;
  using output_t = struct wlr_output;
  using pointer_impl_t = struct wlr_pointer_impl;
  using pointer_t = struct wlr_pointer;
  using renderer_impl_t = struct wlr_renderer_impl;
  using renderer_t = struct wlr_renderer;
  using screenshooter_t = struct wlr_screenshooter;
  using screenshot_t = struct wlr_screenshot;
  using seat_client_t = struct wlr_seat_client;
  using seat_keyboard_grab_t = struct wlr_seat_keyboard_grab;
  using seat_keyboard_state_t = struct wlr_seat_keyboard_state;
  using seat_pointer_state_t = struct wlr_seat_pointer_state;
  using seat_t = struct wlr_seat;
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
  using xcursor_image_t = struct wlr_xcursor_image;
  using xcursor_manager_t = struct wlr_xcursor_manager;
  using xcursor_theme_t = struct wlr_xcursor_theme;
  using xcursor_t = struct wlr_xcursor;
  using xdg_client_v6_t = struct wlr_xdg_client_v6;
  using xdg_popup_grab_v6_t = struct wlr_xdg_popup_grab_v6;
  using xdg_popup_v6_t = struct wlr_xdg_popup_v6;
  using xdg_positioner_v6_t = struct wlr_xdg_positioner_v6;
  using xdg_shell_v6_t = struct wlr_xdg_shell_v6;
  using xdg_surface_v6_t = struct wlr_xdg_surface_v6;
  using xdg_toplevel_v6_t = struct wlr_xdg_toplevel_v6;


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

  private:
    std::function<void(void* data)>* _func;
    struct wl_listener _listener;
  };

} // namespace cloth::wlr
