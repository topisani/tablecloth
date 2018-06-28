#include <csignal>
#include <cstdlib>

#include <unistd.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

#include "swc.hpp"

/* Obtain a backtrace and print it to stdout. */
void print_trace(void)
{
  void* array[10];
  size_t size;
  char** strings;
  size_t i;

  size = backtrace(array, 10);
  strings = backtrace_symbols(array, size);

  printf("Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++) printf("%s\n", strings[i]);

  free(strings);
}

static void log(const char* str)
{
  std::cerr << str << '\n';
};

namespace cloth {
  struct Screen;

  struct Window : swc::Window {
    Window(Base*, Screen& screen) noexcept;

    Window(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept = default;

    void destroyed() noexcept override;

    void entered() noexcept override;

    Screen* screen;
  };


  struct Screen : swc::Screen {
    Screen(Base*) noexcept;

    Screen(const Screen&) = delete;
    Screen(Screen&&) noexcept = default;
    Screen& operator=(Screen&&) = default;

    virtual ~Screen() noexcept
    {
      log("destruct screen");
    }

    void usable_geometry_changed() noexcept override
    {
      /* If the usable geometry of the screen changes, for example when a panel is
       * docked to the edge of the screen, we need to rearrange the windows to
       * ensure they are all within the new usable geometry. */
      arrange();
    }

    void entered() noexcept override;

    /**
     * This is a basic grid arrange function that tries to give each window an
     * equal space.
     */
    void arrange() noexcept;


    Window& add_window(Window&& window) noexcept;

    void remove_window(Window& window) noexcept;

    Window* focus(Window* window) noexcept;

    std::vector<Window> windows;
    std::vector<Window>::iterator focused_window = windows.end();
  };

  static const char* terminal_command[] = {"wterm", NULL};
  static const char* dmenu_command[] = {"dmenu", NULL};
  static const uint32_t border_width = 1;
  static const uint32_t border_color_active = 0xff333388;
  static const uint32_t border_color_normal = 0xff888888;

  static struct wl_display* display;
  static struct wl_event_loop* event_loop;
  static std::vector<Screen> screens;
  static Screen* active_screen;

  Window::Window(Base* base, Screen& screen) noexcept : ::swc::Window{base}, screen(&screen)
  {
    log("Window constructor");
    set_tiled();
    screen.focus(this);
  }

  Window::Window(Window&& rhs) noexcept
    : swc::Window(std::move(rhs))
  {
    screen = rhs.screen;
    rhs.screen = nullptr;
  }

  void Window::destroyed() noexcept
  {
    if (base() != nullptr) {
      log("Window destroyed");
      screen->remove_window(*this);
    }
  }

  void Window::entered() noexcept
  {
    log("Window entered");
    screen->focus(this);
  }

  Screen::Screen(Base* base) noexcept : swc::Screen(base)
  {
    log("Screen constructed");
    active_screen = this;
  }

  void Screen::entered() noexcept
  {
    log("Entered screen");
    active_screen = this;
  }

  void Screen::arrange() noexcept
  {
    log("Arranging");
    unsigned num_columns, num_rows, column_index, row_index;
    swc::Rectangle geometry;
    swc::Rectangle& screen_geometry = base()->usable_geometry;

    if (windows.empty()) return;

    num_columns = std::ceil(std::sqrt(windows.size()));
    num_rows = windows.size() / num_columns + 1;
    auto iter = windows.begin();
    for (column_index = 0; iter != windows.end(); ++column_index) {
      geometry.x =
        screen_geometry.x + border_width + screen_geometry.width * column_index / num_columns;
      geometry.width = screen_geometry.width / num_columns - 2 * border_width;

      if (column_index == windows.size() % num_columns) --num_rows;

      for (row_index = 0; row_index < num_rows && iter != windows.end(); ++row_index) {
        geometry.y =
          screen_geometry.y + border_width + screen_geometry.height * row_index / num_rows;
        geometry.height = screen_geometry.height / num_rows - 2 * border_width;

        iter->set_geometry(geometry);
        ++iter;
      }
    }
  }

  Window& Screen::add_window(Window&& window) noexcept
  {
    log("Adding window");
    window.screen = this;
    auto& ret = windows.emplace_back(std::move(window));
    ret.show();
    arrange();
    return ret;
  }

  void Screen::remove_window(Window& window) noexcept
  {
    log("removing window");
    window.screen = NULL;
    auto iter = std::find(windows.begin(), windows.end(), window);
    if (iter != windows.end()) {
      if (iter == focused_window) {
        focus(nullptr);
      }
      iter->hide();
      windows.erase(iter);
    }
    arrange();
  }

  Window* Screen::focus(Window* window) noexcept
  {
    if (focused_window < windows.end() && focused_window >= windows.begin()) {
      focused_window->set_border(border_color_normal, border_width);
    }

    if (window != nullptr) {
      window->set_border(border_color_active, border_width);
      window->focus();
      focused_window = std::find(windows.begin(), windows.end(), *window);
    } else {
      swc_window_focus(nullptr);
      focused_window = windows.end();
    }

    return &*focused_window;
  }

  static void spawn(void* data, uint32_t time, uint32_t value, uint32_t state)
  {
    char* const* command = (char* const*) data;

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;

    if (fork() == 0) {
      execvp(command[0], command);
      exit(EXIT_FAILURE);
    }
  }

  static void quit(void* data, uint32_t time, uint32_t value, uint32_t state)
  {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;

    wl_display_terminate(display);
  }

  static const swc::Manager manager = {
    [](swc::Screen::Base* screen) {
      log("New screen");
      active_screen = &screens.emplace_back(screen);
    },
    [](swc::Window::Base* base) {
      log("New window");
      active_screen->add_window({base, *active_screen});
    }
  };

  int main(int argc, char* argv[])
  {
    auto sig_handler = [](int sig) {
      printf("Recieved signal %d\n", sig);
      print_trace();
      std::abort();
    };

    std::signal(SIGSEGV, sig_handler);

    display = wl_display_create();
    if (!display) return EXIT_FAILURE;

    if (wl_display_add_socket(display, NULL) != 0) return EXIT_FAILURE;

    if (!manager.init(display, nullptr)) return EXIT_FAILURE;

    swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_Return, &spawn, terminal_command);
    swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_r, &spawn, dmenu_command);
    swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_q, &quit, NULL);

    event_loop = wl_display_get_event_loop(display);
    wl_display_run(display);
    wl_display_destroy(display);

    return EXIT_SUCCESS;
  }

} // namespace cloth

int main(int argc, char* argv[])
{
  return cloth::main(argc, argv);
}
