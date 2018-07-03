#pragma once

#include <functional>

#include <wayland-client.hpp>

#include "util/bindings.hpp"
#include "weston-desktop-shell-client-protocol.h"

#define implicit

namespace weston {

  namespace util = cloth::util;



  struct DesktopShell {
    CLOTH_BIND_BASE(DesktopShell, weston_desktop_shell);

    enum struct Cursor : uint32_t {
      None = WESTON_DESKTOP_SHELL_CURSOR_NONE,
      ResizeTop = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP,
      ResizeBottom = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM,
      Arrow = WESTON_DESKTOP_SHELL_CURSOR_ARROW,
      ResizeLeft = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_LEFT,
      ResizeTopLeft = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP_LEFT,
      ResizeBottomLeft = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_LEFT,
      Move = WESTON_DESKTOP_SHELL_CURSOR_MOVE,
      ResizeRight = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_RIGHT,
      ResizeTopRight = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_TOP_RIGHT,
      ResizeBottomRight = WESTON_DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_RIGHT,
      Busy = WESTON_DESKTOP_SHELL_CURSOR_BUSY,
    };

    enum struct PanelPosition : uint32_t {
      Top = WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP,
      Bottom = WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM,
      Left = WESTON_DESKTOP_SHELL_PANEL_POSITION_LEFT,
      Right = WESTON_DESKTOP_SHELL_PANEL_POSITION_RIGHT,
    };

    struct Listener {
      CLOTH_BIND_BASE(Listener, weston_desktop_shell_listener);

      using configure_t = std::function<void(DesktopShell&,
                                             uint32_t edges,
                                             wayland::surface_t& surface,
                                             int32_t width,
                                             int32_t height)>;
      using prepare_lock_surface_t = std::function<void(DesktopShell&)>;
      using grab_cursor_t = std::function<void(DesktopShell&, Cursor cursor)>;

      Listener(configure_t configure,
               prepare_lock_surface_t prepare_lock_surface,
               grab_cursor_t grab_cursor)
        : _base(std::make_unique<Base>()),
          _configure(configure),
          _prepare_lock_surface(prepare_lock_surface),
          _grab_cursor(grab_cursor)
      {
        base()->configure = [](void* data, struct weston_desktop_shell* wshell, uint32_t edges,
                               struct wl_surface* surface, int32_t width, int32_t height) {
          auto& self = Listener::from_voidptr(data);
          auto shell = DesktopShell(wshell);
          auto surf = wayland::surface_t(surface);

          self._configure(shell, edges, surf, width, height);
        };
        base()->prepare_lock_surface = [](void* data, struct weston_desktop_shell* wshell) {
          auto& self = Listener::from_voidptr(data);
          auto shell = DesktopShell(wshell);

          self._prepare_lock_surface(shell);
        };
        base()->grab_cursor = [](void* data, struct weston_desktop_shell* wshell, uint32_t cursor) {
          auto& self = Listener::from_voidptr(data);
          auto shell = DesktopShell(wshell);

          self._grab_cursor(shell, Cursor{cursor});
        };
      }


    private:
      configure_t _configure;
      prepare_lock_surface_t _prepare_lock_surface;
      grab_cursor_t _grab_cursor;
    };

    inline static const std::string interface_name = "weston_desktop_shell";

    // Methods //

    int add_listener(DesktopShell::Listener& listener) noexcept
    {
      return ::weston_desktop_shell_add_listener(base(), listener, &listener);
    }

    void set_user_data(void* user_data) noexcept
    {
      ::weston_desktop_shell_set_user_data(base(), user_data);
    }

    void* get_user_data() noexcept
    {
      return ::weston_desktop_shell_get_user_data(base());
    }

    uint32_t get_version() noexcept
    {
      return ::weston_desktop_shell_get_version(base());
    }

    void destroy() noexcept
    {
      ::weston_desktop_shell_destroy(base());
    }


    void set_background(wayland::output_t& output, wayland::surface_t& surface) noexcept
    {
      ::weston_desktop_shell_set_background(base(), output, surface);
    }


    void set_panel(wayland::output_t& output, wayland::surface_t& surface) noexcept
    {
      ::weston_desktop_shell_set_panel(base(), output, surface);
    }

    void set_lock_surface(wayland::surface_t& surface) noexcept
    {
      ::weston_desktop_shell_set_lock_surface(base(), surface);
    }

    void unlock() noexcept
    {
      ::weston_desktop_shell_unlock(base());
    }

    /**
     * The surface set by this request will receive a fake
     * pointer.enter event during grabs at position 0, 0 and is
     * expected to set an appropriate cursor image as described by
     * the grab_cursor event sent just before the enter event.
     */
    void set_grab_surface(wayland::surface_t& surface) noexcept
    {
      ::weston_desktop_shell_set_grab_surface(base(), surface);
    }

    /**
     * Tell the server, that enough desktop elements have been drawn
     * to make the desktop look ready for use. During start-up, the
     * server can wait for this request with a black screen before
     * starting to fade in the desktop, for instance. If the client
     * parts of a desktop take a long time to initialize, we avoid
     * showing temporary garbage.
     */
    void desktop_ready() noexcept
    {
      ::weston_desktop_shell_desktop_ready(base());
    }

    /**
     * Tell the shell which side of the screen the panel is
     * located. This is so that new windows do not overlap the panel
     * and maximized windows maximize properly.
     */
    void set_panel_position(PanelPosition position) noexcept
    {
      ::weston_desktop_shell_set_panel_position(base(), static_cast<uint32_t>(position));
    }
  };

  struct Screensaver {
    CLOTH_BIND_BASE(Screensaver, weston_screensaver);

    void set_user_data(void* user_data) noexcept
    {
      weston_screensaver_set_user_data(base(), user_data);
    }

    void* get_user_data() noexcept
    {
      return weston_screensaver_get_user_data(base());
    }

    uint32_t get_version() noexcept
    {
      return weston_screensaver_get_version(base());
    }

    void destroy() noexcept
    {
      weston_screensaver_destroy(base());
    }

    /**
     * A screensaver surface is normally hidden, and only visible after an
     * idle timeout.
     */
    void set_surface(wayland::surface_t& surface, wayland::output_t& output) noexcept
    {
      weston_screensaver_set_surface(base(), surface, output);
    }
  };

} // namespace weston
