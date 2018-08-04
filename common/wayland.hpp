#pragma once

#include <functional>

#include <wayland-client.h>
#include <wayland-server.h>

namespace cloth::wl {

  using argument_t = union wl_argument;
  using array_t = struct wl_array;
  // using buffer_t = struct wl_buffer;
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
  using subcompositor_error_t = enum wl_subcompositor_error;
  using subsurface_error_t = enum wl_subsurface_error;
  using subsurface_t = struct wl_subsurface;
  using surface_error_t = enum wl_surface_error;
  using surface_t = struct wl_surface;
  using touch_t = struct wl_touch;


  using listener_t = struct wl_listener;
  using signal_t = struct wl_signal;
  struct Signal {
    Signal() noexcept
    {
      wl_signal_init(&signal);
    }

    ~Signal() noexcept {}

    void add(listener_t& listener)
    {
      wl_signal_add(&signal, &listener);
    }

    void emit(void* data = nullptr)
    {
      wl_signal_emit(&signal, data);
    }

    operator signal_t&()
    {
      return signal;
    }

    operator signal_t*()
    {
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

    Listener(std::function<void()> func) noexcept : Listener([f = std::move(func)](void*) { f(); })
    {}

    Listener(std::nullptr_t) noexcept : Listener(std::function<void(void*)>(nullptr)) {}

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
      *_func = func;
      return *this;
    }

    Listener& operator=(std::function<void()> func) noexcept
    {
      *_func = [func](void*) { func(); };
      return *this;
    }

    Listener& operator=(std::nullptr_t func) noexcept
    {
      *_func = func;
      return *this;
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

    void remove() noexcept
    {
      if (_listener.link.next != nullptr) wl_list_remove(&_listener.link);
      _listener.link = {nullptr, nullptr};
    }

  private:
    std::function<void(void* data)>* _func;
    struct wl_listener _listener;
  };

} // namespace cloth::wl
