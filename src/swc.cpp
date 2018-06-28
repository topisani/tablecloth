#include <iostream>
#include <vector>

#include "swc.hpp"

static void log(const char* str)
{
  std::cerr << str << std::endl;
};

namespace swc {

  Screen::Screen(Base* base) noexcept : _base(base)
  {
    log("Screen::Screen(Base* base) noexcept : _base(base)");

    if (!base) return;
    _shandl.destroy = [](void* data) {
      printf("Data: %p\n", data);
      auto& screen = *static_cast<Screen*>(data);
      screen.destroyed();
    };
    _shandl.geometry_changed = [](void* data) {
      printf("Data: %p\n", data);
      auto& screen = *static_cast<Screen*>(data);
      screen.geometry_changed();
    };
    _shandl.usable_geometry_changed = [](void* data) {
      printf("Data: %p\n", data);
      auto& screen = *static_cast<Screen*>(data);
      screen.usable_geometry_changed();
    };
    _shandl.entered = [](void* data) {
      printf("Data: %p\n", data);
      auto& screen = *static_cast<Screen*>(data);
      screen.entered();
    };

    ::swc_screen_set_handler(_base, &_shandl, this);
  }

  Window::Window(Base* base) noexcept : _base(base)
  {
    log("Window::Window(Base* base) noexcept : _base(base)");
    if (!base) return;
    _shandl.destroy = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.destroyed();
    };
    _shandl.title_changed = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.title_changed();
    };
    _shandl.app_id_changed = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.app_id_changed();
    };
    _shandl.parent_changed = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.parent_changed();
    };
    _shandl.entered = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.entered();
    };
    _shandl.move = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.move();
    };
    _shandl.resize = [](void* data) {
      printf("Data: %p\n", data);
      auto& window = *static_cast<Window*>(data);
      window.resize();
    };

    ::swc_window_set_handler(_base, &_shandl, this);
  }

  Window::Window(Window&& rhs) noexcept
    : Window(rhs._base)
  {
    rhs._base = nullptr;
  }

  void Window::close() noexcept
  {
    log(" void Window::close() noexcept ");
    ::swc_window_close(_base);
  }

  void Window::show() noexcept
  {
    log(" void Window::show() noexcept ");
    ::swc_window_show(_base);
  }

  void Window::hide() noexcept
  {
    log(" void Window::hide() noexcept ");
    ::swc_window_hide(_base);
  }

  void Window::focus() noexcept
  {
    log(" void Window::focus() noexcept ");
    ::swc_window_focus(_base);
  }

  void Window::set_stacked() noexcept
  {
    log(" void Window::set_stacked() noexcept ");
    ::swc_window_set_stacked(_base);
  }

  void Window::set_tiled() noexcept
  {
    log(" void Window::set_tiled() noexcept ");
    ::swc_window_set_tiled(_base);
  }

  void Window::set_fullscreen(Screen& screen) noexcept
  {
    log(" void Window::set_fullscreen(Screen& screen) noexcept ");
    ::swc_window_set_fullscreen(_base, screen.base());
  }

  void Window::set_position(int32_t x, int32_t y) noexcept
  {
    log(" void Window::set_position(int32_t x, int32_t y) noexcept ");
    ::swc_window_set_position(_base, x, y);
  }

  void Window::set_size(uint32_t width, uint32_t height) noexcept
  {
    log(" void Window::set_size(uint32_t width, uint32_t height) noexcept ");
    ::swc_window_set_size(_base, width, height);
  }

  void Window::set_geometry(const Rectangle& geometry) noexcept
  {
    log(" void Window::set_geometry(const Rectangle& geometry) noexcept ");
    ::swc_window_set_geometry(_base, &geometry);
  }

  void Window::set_border(uint32_t color, uint32_t width) noexcept
  {
    log(" void Window::set_border(uint32_t color, uint32_t width) noexcept ");
    ::swc_window_set_border(_base, color, width);
  }

  void Window::begin_move() noexcept
  {
    log(" void Window::begin_move() noexcept ");
    ::swc_window_begin_move(_base);
  }

  void Window::end_move() noexcept
  {
    log(" void Window::end_move() noexcept ");
    ::swc_window_end_move(_base);
  }

  void Window::begin_resize(Edge edges) noexcept
  {
    log(" void Window::begin_resize(Edge edges) noexcept ");
    ::swc_window_begin_resize(_base, edges);
  }

  void Window::end_resize() noexcept
  {
    log(" void Window::end_resize() noexcept ");
    ::swc_window_end_resize(_base);
  }

  static std::vector<BindingHandler> handler_storage;

  int add_binding(BindingType type, uint32_t modifiers, uint32_t value, BindingHandler handler)
  {

  log("int add_binding(BindingType type, uint32_t modifiers, uint32_t value, BindingHandler handler)");
    auto& h = handler_storage.emplace_back(std::move(handler));
    return ::swc_add_binding(static_cast<enum swc_binding_type>(type), modifiers, value,
                             [](void* data, uint32_t type, uint32_t mods, uint32_t val) {
                               auto& handler = *static_cast<BindingHandler*>(data);
                               handler(static_cast<BindingType>(type), static_cast<Modifier>(mods),
                                       val);
                             },
                             &h);
  }

  int Manager::init(struct wl_display* display, struct wl_event_loop* event_loop) const noexcept
  {
    log(" void Manager::init(struct wl_display* display, struct wl_event_loop* event_loop) "
        "noexcept ");
    return ::swc_initialize(display, event_loop, this);
  }

  Manager::~Manager() noexcept

  {
    log("  Manager::~Manager() noexcept");
    ::swc_finalize();
  }

} // namespace swc
