#include "keyboard.hpp"

#include <condition_variable>
#include <thread>

#include "client.hpp"

#include "util/chrono.hpp"
#include "util/exception.hpp"
#include "util/logging.hpp"

#include "gdkwayland.hpp"
#include "keymap_data.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>

namespace cloth::kbd {

  int os_fd_set_cloexec(int fd)
  {
    long flags;

    if (fd == -1) return -1;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1) return -1;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) return -1;

    return 0;
  }

  static int set_cloexec_or_close(int fd)
  {
    if (os_fd_set_cloexec(fd) != 0) {
      close(fd);
      return -1;
    }
    return fd;
  }


  static int create_tmpfile_cloexec(char* tmpname)
  {
    int fd;

#ifdef HAVE_MKOSTEMP
    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0) unlink(tmpname);
#else
    fd = mkstemp(tmpname);
    if (fd >= 0) {
      fd = set_cloexec_or_close(fd);
      unlink(tmpname);
    }
#endif

    return fd;
  }


  int os_create_anonymous_file(off_t size)
  {
    static const char tmplt[] = "/cloth-kbd-shared-XXXXXX";
    const char* path;
    char* name;
    int fd;
    int ret;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path) {
      errno = ENOENT;
      return -1;
    }

    name = (char*) malloc(strlen(path) + sizeof(tmplt));
    if (!name) return -1;

    strcpy(name, path);
    strcat(name, tmplt);

    fd = create_tmpfile_cloexec(name);

    free(name);

    if (fd < 0) return -1;

#ifdef HAVE_POSIX_FALLOCATE
    do {
      ret = posix_fallocate(fd, 0, size);
    } while (ret == EINTR);
    if (ret != 0) {
      close(fd);
      errno = ret;
      return -1;
    }
#else
    do {
      ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
      close(fd);
      return -1;
    }
#endif

    return fd;
  }

  auto VirtualKeyboard::setup_kbd_protocol() -> void
  {
    wp_keyboard = client.virtual_keyboard_manager.create_virtual_keyboard(client.seat);
    const char* keymap_str = keymap_data;
    size_t keymap_size = strlen(keymap_str) + 1;
    int keymap_fd = os_create_anonymous_file(keymap_size);
    if (keymap_fd >= 0) {
      void* ptr = mmap(NULL, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, keymap_fd, 0);
      if (ptr != (void*) -1) {
        std::strcpy((char*) ptr, keymap_str);
        wp_keyboard.keymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_size);
        return;
      }
      close(keymap_fd);
    } else {
      throw util::exception("Couldnt create anonymous file");
    }
    return;
  }

  VirtualKeyboard::VirtualKeyboard(Client& client)
    : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL}, state(*this)
  {
    window.set_title("tablecloth panel");
    window.set_decorated(false);

    setup_kbd_protocol();

    setup_css();
    setup_widgets();

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = client.layer_shell.get_layer_surface(
      surface, nullptr, wl::zwlr_layer_shell_v1_layer::overlay, "cloth.panel");
    layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::bottom);
    layer_surface.set_size(width, height);
    layer_surface.set_keyboard_interactivity(0);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      window.show_all();
      layer_surface.ack_configure(serial);
      if (window.get_height() != height) {
        width = window.get_width();
        height = window.get_height();
        LOGD("New size: {}, {}", width, height);
        layer_surface.set_size(width, height);
        layer_surface.set_exclusive_zone(0);
        surface.commit();
      }
    };
    layer_surface.on_closed() = [&] { window.close(); };

    surface.commit();
  }

  auto VirtualKeyboard::setup_css() -> void
  {
    css_provider = Gtk::CssProvider::create();
    style_context = Gtk::StyleContext::create();

    // load our css file, wherever that may be hiding
    if (css_provider->load_from_path(client.css_file)) {
      Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
      style_context->add_provider_for_screen(screen, css_provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
  }

  auto VirtualKeyboard::setup_widgets() -> void
  {
    window.add(state.layers[LayerType::Letters]);
  }

  auto VirtualKeyboard::set_size(int width, int height) -> void
  {
    this->width = width;
    this->height = height;
    window.set_size_request(width, height);
    window.resize(width, height);
    layer_surface.set_size(width, height);
    surface.commit();
  }

  auto KeyboardState::send_key_press(xkb_keysym_t keysym) -> void
  {
    char buf[256] = {'\0'};
    xkb_keysym_get_name(keysym, buf, 256);
    LOGD("Pressed {}", buf);
    vkbd.wp_keyboard.key(0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED);
    if (modifiers != Modifiers::None) {
      vkbd.wp_keyboard.modifiers(0, 0, 0, 0);
      modifiers = Modifiers::None;
    }
  }

  auto KeyboardState::send_key_release(xkb_keysym_t keysym) -> void
  {
    char buf[256] = {'\0'};
    xkb_keysym_get_name(keysym, buf, 256);
    LOGD("Released {}", buf);
    vkbd.wp_keyboard.key(0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED);
  }

  auto KeyboardState::send_modifier_press(Modifiers m) -> void
  {
    LOGD("Pressed modifier {}", util::enum_cast(m));
    modifiers |= m;
    unsigned mods = (modifiers & Modifiers::Ctrl) ? 0b100 : 0;
    unsigned locked = (modifiers & Modifiers::Shift) ? 0b01 : 0;
    vkbd.wp_keyboard.modifiers(mods, 0, locked, 0);
  }

  auto KeyboardState::set_layer(LayerType lt) -> void
  {
    vkbd.window.remove();
    vkbd.window.add(layers[lt]);
    vkbd.window.show_all();
  }

  auto do_quit(KeyboardState& state) -> void {
    state.vkbd.client.gtk_main.quit();
  }
} // namespace cloth::kbd
