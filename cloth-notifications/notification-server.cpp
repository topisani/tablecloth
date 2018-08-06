#include "notification-server.hpp"

#include "client.hpp"

#include "gdkwayland.hpp"
#include "util/iterators.hpp"

namespace cloth::notifications {

  auto NotificationServer::GetCapabilities() -> std::vector<std::string>
  {
    return {"body", "actions", "icon-static"};
  }

  auto NotificationServer::Notify(const std::string& app_name,
                                  const uint32_t& not_id_in,
                                  const std::string& app_icon,
                                  const std::string& summary,
                                  const std::string& body,
                                  const std::vector<std::string>& actions,
                                  const std::map<std::string, ::DBus::Variant>& hints,
                                  const int32_t& expire_timeout) -> uint32_t
  {
    unsigned notification_id = not_id_in;
    if (notification_id == 0) notification_id = ++_id;

    LOGI("[{}]: {}", summary, body);
    std::ostringstream strm;
    strm << "hints = { ";
    for (auto& [k, v] : hints) {
      strm << k << " ";
    }
    strm << " }";
    LOGD("{}", strm.str());

    std::string image_path;
    try {
      auto [width, height, rowstride, has_alpha, bits_per_sample, channels, image_data, _] = DBus::Struct<int, int, int, bool, int, int, std::vector<uint8_t>>(hints.at("image-data"));
      // TODO
    } catch (std::out_of_range& e) {
    }

    try {
      image_path = std::string(hints.at("image-path"));
    } catch (std::out_of_range& e) {
    }

    if (image_path.empty()) image_path = app_icon;

    if (!image_path.empty()) LOGD("Image: {}", image_path);

    notifications.emplace_back(*this, notification_id, summary, body, actions, image_path);

    return notification_id;
  }

  auto NotificationServer::CloseNotification(const uint32_t&) -> void {}

  auto NotificationServer::GetServerInformation(std::string& name,
                                                std::string& vendor,
                                                std::string& version,
                                                std::string& spec_version) -> void
  {
    name = "cloth-notifications";
    vendor = "topisani";
    version = "0.0.1";
    spec_version = "1.2";
  }

  // Notification //

  Notification::Notification(NotificationServer& server,
                             unsigned id,
                             const std::string& title_str,
                             const std::string& body_str,
                             const std::vector<std::string>& actions,
                             const std::string& app_icon)
    : server(server), id(id)
  {
    window.set_title("Cloth Notification");
    window.set_decorated(false);

    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    server.client.style_context->add_provider_for_screen(screen, server.client.css_provider,
                                                         GTK_STYLE_PROVIDER_PRIORITY_USER);

    title.set_text(title_str);
    body.set_text(body_str);
    auto& actions_box = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    auto prev = actions.begin();
    auto cur = prev + 1; 
    for (;cur != actions.end() && prev != actions.end(); cur++, prev++) {
      auto& action = *prev;
      auto& label = *cur;
      auto& button = this->actions.emplace_back(label);
      button.signal_clicked().connect([this, action = action, label = label] {
        LOGD("Action: {} -> {}", label, action);
        this->server.ActionInvoked(this->id, action);
        util::erase_this(this->server.notifications, this);
      });
      actions_box.pack_start(button);
    }
    auto& box2 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    box2.pack_start(title);
    box2.pack_start(body);
    box2.pack_start(actions_box);

    auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    box1.pack_end(box2);
    if (!app_icon.empty()) {
      image.set(app_icon);
    }

    window.signal_button_press_event().connect([this](GdkEventButton* evt) {
      util::erase_this(this->server.notifications, this);
      return false;
    });

    window.add(box1);

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = server.client.layer_shell.get_layer_surface(
      surface, nullptr, wl::zwlr_layer_shell_v1_layer::top, "cloth.notification");
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      layer_surface.ack_configure(serial);
      if (width != window.get_width()) {
        layer_surface.set_size(window.get_width(), window.get_height());
        layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::top |
                                 wl::zwlr_layer_surface_v1_anchor::right);
        layer_surface.set_margin(20, 20, 20, 20);
        layer_surface.set_exclusive_zone(0);
        surface.commit();
      }
      window.show_all();
    };
    layer_surface.on_closed() = [&] { util::erase_this(this->server.notifications, this); };

    layer_surface.set_size(1, 1);

    surface.commit();
  } // namespace cloth::notifications

  Notification::~Notification()
  {
    server.NotificationClosed(id, 0);
  }

} // namespace cloth::notifications
