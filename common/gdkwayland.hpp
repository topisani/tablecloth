#pragma once

#include <string_view>

#include <gdk/gdkwayland.h>
#include <gtkmm.h>

#include <wayland-client.hpp>

namespace Gdk::wayland::window {

  inline GType get_type() noexcept
  {
    return gdk_wayland_window_get_type();
  }

  inline ::wayland::surface_t get_wl_surface(Gdk::Window& window) noexcept
  {
    return {gdk_wayland_window_get_wl_surface(window.gobj()),
            ::wayland::proxy_t::wrapper_type::foreign};
  }

  inline ::wayland::surface_t get_wl_surface(Gtk::Window& window) noexcept
  {
    return {gdk_wayland_window_get_wl_surface(gtk_widget_get_window(GTK_WIDGET(window.gobj()))),
            ::wayland::proxy_t::wrapper_type::foreign};
  }

  inline void set_use_custom_surface(Gdk::Window& window) noexcept
  {
    gdk_wayland_window_set_use_custom_surface(window.gobj());
  }

  inline void set_use_custom_surface(Gtk::Window& window) noexcept
  {
    gdk_wayland_window_set_use_custom_surface(gtk_widget_get_window(GTK_WIDGET(window.gobj())));
  }

  inline void set_dbus_properties_libgtk_only(Gdk::Window& window,
                                       const char* application_id,
                                       const char* app_menu_path,
                                       const char* menubar_path,
                                       const char* window_object_path,
                                       const char* application_object_path,
                                       const char* unique_bus_name) noexcept
  {
    gdk_wayland_window_set_dbus_properties_libgtk_only(window.gobj(), application_id, app_menu_path,
                                                       menubar_path, window_object_path,
                                                       application_object_path, unique_bus_name);
  }

  using WindowExported = std::function<void(Gdk::Window& window, std::string_view handle)>;

  inline gboolean export_handle(Gdk::Window& window,
                         WindowExported callback,
                         GDestroyNotify destroy_func = nullptr) noexcept
  {
    struct Data {
      WindowExported callback;
      GDestroyNotify destroy_func;
    };
    auto cb = [](GdkWindow* win, const char* handle, gpointer data) {
      auto& self = *static_cast<Data*>(data);
      auto window = Glib::wrap(win);
      self.callback(*(window.get()), handle);
    };
    auto destroy = [](gpointer data) {
      auto& self = *static_cast<Data*>(data);
      if (self.destroy_func) self.destroy_func(&self.callback);
      delete (&self);
    };
    auto data = new Data{std::move(callback), std::move(destroy_func)};
    return gdk_wayland_window_export_handle(window.gobj(), cb, data, destroy);
  }

  inline void unexport_handle(Gdk::Window& window) noexcept
  {
    gdk_wayland_window_unexport_handle(window.gobj());
  }

  inline gboolean set_transient_for_exported(Gdk::Window& window, char* parent_handle_str) noexcept
  {
    return gdk_wayland_window_set_transient_for_exported(window.gobj(), parent_handle_str);
  }

  inline void announce_csd(Gdk::Window& window) noexcept
  {
    gdk_wayland_window_announce_csd(window.gobj());
  }

} // namespace Gdk::wayland::window
