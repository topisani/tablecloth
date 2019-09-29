#include "lock.hpp"

#include <condition_variable>
#include <thread>

#include "client.hpp"

#include "util/logging.hpp"

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "gdkwayland.hpp"

namespace cloth::lock {

  LockScreen::LockScreen(Client& client, std::unique_ptr<wl::output_t>&& p_output, bool show_widget)
    : client(client),
      window{Gtk::WindowType::WINDOW_TOPLEVEL},
      output(std::move(p_output)),
      show_login_widgets(show_widget)
  {
    output->on_mode() = [this](wl::output_mode, int32_t w, int32_t h, int32_t refresh) {
      cloth_info("LockScreen width configured: {}", w);
      set_size(w, h);
    };
    window.set_title("tablecloth panel");
    window.set_decorated(false);

    setup_css();
    setup_widgets();

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = client.layer_shell.get_layer_surface(
      surface, *output, wl::zwlr_layer_shell_v1_layer::overlay, "cloth.panel");
    layer_surface.set_anchor(
      wl::zwlr_layer_surface_v1_anchor::left | wl::zwlr_layer_surface_v1_anchor::top |
      wl::zwlr_layer_surface_v1_anchor::right | wl::zwlr_layer_surface_v1_anchor::bottom);
    layer_surface.set_size(width, height);
    if (show_widget) layer_surface.set_keyboard_interactivity(1);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      window.show_all();
      layer_surface.ack_configure(serial);
      if (this->height != height || this->width != width) {
        width = this->width;
        height = this->height;
        cloth_debug("New size: {}, {}", width, height);
        layer_surface.set_size(width, height);
        layer_surface.set_exclusive_zone(-1);
        surface.commit();
      }
    };
    layer_surface.on_closed() = [&] { window.close(); };

    surface.commit();
  }

  auto LockScreen::setup_css() -> void
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

  auto LockScreen::set_size(int width, int height) -> void
  {
    this->width = width;
    this->height = height;
    window.set_size_request(width, height);
    window.resize(width, height);
    layer_surface.set_size(width, height);
    surface.commit();
  }

  // function used to get user input
  static auto check_login(const std::string& user, const std::string& password) -> bool
  {
    static pam_response* reply;
    const struct pam_conv local_conversation = {[](int num_msg, const struct pam_message** msg,
                                                   struct pam_response** resp, void* appdata_ptr) {
                                                  *resp = reply;
                                                  return PAM_SUCCESS;
                                                },
                                                NULL}; // namespace cloth::lock
    pam_handle_t* local_auth_handle = NULL;            // this gets set by pam_start


    reply = (struct pam_response*) malloc(sizeof(struct pam_response));

    int retval;
    retval = pam_start("sudo", user.c_str(), &local_conversation, &local_auth_handle);

    if (retval != PAM_SUCCESS) {
      throw util::exception("pam_start returned: {}", retval);
    }

    reply->resp = strdup(password.c_str());
    reply->resp_retcode = 0;
    retval = pam_authenticate(local_auth_handle, 0);

    if (retval != PAM_SUCCESS) {
      if (retval == PAM_AUTH_ERR) {
        return false;
      } else {
        throw util::exception("pam_end returned {}", retval);
      }
    }

    cloth_debug("Authenticated.\n");
    retval = pam_end(local_auth_handle, retval);

    if (retval != PAM_SUCCESS) {
      throw util::exception("pam_end returned {}", retval);
    }

    return true;
  }

  auto LockScreen::submit() -> void
  {
    try {
      if (check_login(user_prompt.get_text(), password_prompt.get_text())) {
        this->client.gtk_main.quit();
      } else {
        password_prompt.set_text("");
      }
    } catch (std::exception& e) {
      cloth_error("Error while checking password: {}", e.what());
      password_prompt.set_text("");
    }
  }

  auto LockScreen::setup_widgets() -> void
  {
    box = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    box.add(clock_widget);

    if (show_login_widgets) {
      login_box = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
      user_prompt.set_text(getenv("USER"));
      login_box.add(user_prompt);
      login_box.add(password_prompt);
      login_box.set_focus_child(password_prompt);
      login_button = Gtk::Button("Log in");
      login_box.add(login_button);
      login_button.signal_clicked().connect([this] { submit(); });
      password_prompt.signal_activate().connect([this] { submit(); });
      password_prompt.set_visibility(false);
      box.add(login_box);
    }
    vbox = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    hbox = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL);
    vbox.pack_start(hbox, true, false);
    hbox.pack_start(box, true, false);
    window.add(vbox);
  }

} // namespace cloth::lock
