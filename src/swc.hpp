#pragma once

#include <swc.h>
#include <cstdint>
#include <functional>

namespace swc {

  /* Rectangles */

  using Rectangle = struct ::swc_rectangle;

  /*  */

  /* Screens */

  struct Screen {
    using Base = ::swc_screen;

    /**
     * Calls swc_screen_set_handler
     */
    Screen(Base* base) noexcept;

    virtual ~Screen() noexcept = default;

    /**
     * Called when the screen is about to be destroyed.
     *
     * After this is called, the screen is no longer valid.
     */
    virtual void destroyed() noexcept {};

    /**
     * Called when the total area of the screen has changed.
     */
    virtual void geometry_changed() noexcept {};

    /**
     * Called when the geometry of the screen available for laying out windows has
     * changed.
     *
     * A window manager should respond by making sure all visible windows are
     * within this area.
     */
    virtual void usable_geometry_changed() noexcept {};

    /**
     * Called when the pointer enters the screen.
     */
    virtual void entered() noexcept {};

    Base* base() noexcept
    {
      return _base;
    }

    bool operator==(const Screen& rhs) const noexcept
    {
      return _base == rhs._base;
    }

  private:
    Base* _base;
    struct ::swc_screen_handler _shandl;
  };

  /* */

  /* Windows {{{ */

  struct Window {
    using Base = ::swc_window;

    /**
     * Calls swc_window_set_handler
     */
    Window(Base* window) noexcept;

    Window(const Window&) = delete;

    Window(Window&&) noexcept;

    Window& operator=(Window&&) noexcept = default;

    virtual ~Window() noexcept = default;

    /**
     * Called when the window is about to be destroyed.
     *
     * After this is called, the window is no longer valid.
     */
    virtual void destroyed() noexcept {};

    /**
     * Called when the window's title changes.
     */
    virtual void title_changed() noexcept {};

    /**
     * Called when the window's application identifier changes.
     */
    virtual void app_id_changed() noexcept {};

    /**
     * Called when the window's parent changes.
     *
     * This can occur when the window becomes a transient for another window, or
     * becomes a toplevel window.
     */
    virtual void parent_changed() noexcept {};

    /**
     * Called when the pointer enters the window.
     */
    virtual void entered() noexcept {};

    /**
     * Called when the window wants to initiate an interactive move, but the
     * window is not in stacked mode.
     *
     * The window manager may respond by changing the window's mode, after which
     * the interactive move will be honored.
     */
    virtual void move() noexcept {};

    /**
     * Called when the window wants to initiate an interactive resize, but the
     * window is not in stacked mode.
     *
     * The window manager may respond by changing the window's mode, after which
     * the interactive resize will be honored.
     */
    virtual void resize() noexcept {};

    /**
     * Request that the specified window close.
     */
    void close() noexcept;

    /**
     * Make the specified window visible.
     */
    void show() noexcept;

    /**
     * Make the specified window hidden.
     */
    void hide() noexcept;

    /**
     * Set the keyboard focus to the specified window.
     *
     * If window is NULL, the keyboard will have no focus.
     */
    void focus() noexcept;

    /**
     * Sets the window to stacked mode.
     *
     * A window in this mode has its size specified by the client. The window's
     * viewport will be adjusted to the size of the buffer attached by the
     * client.
     *
     * Use of this mode is required to allow interactive moving and resizing.
     */
    void set_stacked() noexcept;

    /**
     * Sets the window to tiled mode.
     *
     * A window in this mode has its size specified by the window manager.
     * Additionally, swc will configure the window to operate in a tiled or
     * maximized state in order to prevent the window from drawing shadows.
     *
     * It is invalid to interactively move or resize a window in tiled mode.
     */
    void set_tiled() noexcept;

    /**
     * Sets the window to fullscreen mode.
     */
    void set_fullscreen(Screen& screen) noexcept;

    /**
     * Set the window's position.
     *
     * The x and y coordinates refer to the top-left corner of the actual contents
     * of the window and should be adjusted for the border size.
     */
    void set_position(int32_t x, int32_t y) noexcept;

    /**
     * Set the window's size.
     *
     * The width and height refer to the dimension of the actual contents of the
     * window and should be adjusted for the border size.
     */
    void set_size(uint32_t width, uint32_t height) noexcept;

    /**
     * Set the window's size and position.
     *
     * This is a convenience function that is equivalent to calling
     * set_size and then set_position.
     */
    void set_geometry(const Rectangle& geometry) noexcept;

    /**
     * Set the window's border color and width.
     *
     * NOTE: The window's geometry remains unchanged, and should be updated if a
     *       fixed top-left corner of the border is desired.
     */
    void set_border(uint32_t color, uint32_t width) noexcept;

    /**
     * Begin an interactive move of the specified window.
     */
    void begin_move() noexcept;

    /**
     * End an interactive move of the specified window.
     */
    void end_move() noexcept;

    enum Edge {
      Auto = SWC_WINDOW_EDGE_AUTO,
      Top = SWC_WINDOW_EDGE_TOP,
      Bottom = SWC_WINDOW_EDGE_BOTTOM,
      Left = SWC_WINDOW_EDGE_LEFT,
      Right = SWC_WINDOW_EDGE_RIGHT,
    };

    /**
     * Begin an interactive resize of the specified window.
     */
    void begin_resize(Edge edges) noexcept;

    /**
     * End an interactive resize of the specified window.
     */
    void end_resize() noexcept;

    Base* base() noexcept
    {
      return _base;
    }

    bool operator==(const Window& rhs) const noexcept
    {
      return _base == rhs._base;
    }

  private:
    Base* _base;
    struct swc_window_handler _shandl;
  };

  /* }}} */

  /* Bindings {{{ */

  enum struct Modifier {
    Ctrl = SWC_MOD_CTRL,
    Alt = SWC_MOD_ALT,
    Logo = SWC_MOD_LOGO,
    Shift = SWC_MOD_SHIFT,
    Any = SWC_MOD_ANY,
  };

  enum struct BindingType {
    Key = SWC_BINDING_KEY,
    Button = SWC_BINDING_BUTTON,
  };

  using BindingHandler = std::function<void(BindingType, Modifier, uint32_t)>;

  /**
   * Register a new input binding.
   *
   * Returns 0 on success, negative error code otherwise.
   */
  int add_binding(BindingType type, uint32_t modifiers, uint32_t value, BindingHandler handler);

  /* }}} */

  /**
   * This is a user-provided structure that swc will use to notify the display
   * server of new windows, screens and input devices.
   */
  struct Manager : ::swc_manager {
    Manager(void (*new_screen)(swc_screen* screen) = nullptr,
            void (*new_window)(swc_window* window) = nullptr,
            void (*new_device)(struct libinput_device* device) = nullptr,
            void (*activate)() = nullptr,
            void (*deactivate)() = nullptr) noexcept
      : ::swc_manager{new_screen, new_window, new_device, activate, deactivate}
    {}

    /**
     * Initializes the compositor using the specified display, event_loop, and
     * manager.
     */
    int init(struct wl_display* display, struct wl_event_loop* event_loop) const noexcept;

    /**
     * Stops the compositor, releasing any used resources.
     */
    ~Manager() noexcept;

    /**
     * Called when a new screen is created.
     */
    // void (*new_screen)(Screen *screen) noexcept = nullptr;

    /**
     * Called when a new window is created.
     */
    // void (*new_window)(Window *window) noexcept = nullptr;

    /**
     * Called when a new input device is detected.
     */
    // void (*new_device)(struct libinput_device *device) noexcept = nullptr;

    /**
     * Called when the session gets activated (for example, startup or VT switch).
     */
    // void (*activate)() noexcept = nullptr;

    /**
     * Called when the session gets deactivated.
     */
    // void (*deactivate)() noexcept = nullptr;
  };

} // namespace swc

/* vim: set fdm=marker : */
