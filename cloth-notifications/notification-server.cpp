#include "notification-server.hpp"

#include "client.hpp"

#include "gdkwayland.hpp"
#include "util/iterators.hpp"

namespace cloth::notifications {

  auto get_image(const std::map<std::string, ::DBus::Variant>& hints, const std::string& app_icon)
  {
    try {
      auto [key, is_path] = [&]() -> std::pair<std::string, bool> {
        if (hints.count("image-data")) return {"image-data", false};
        if (hints.count("image_data")) return {"image_data", false}; // deprecated
        if (hints.count("image-path")) return {hints.at("image-path"), true};
        if (hints.count("image_path")) return {hints.at("image_path"), true}; // deprecated
        if (!app_icon.empty()) return {app_icon, true};
        if (hints.count("icon_data")) return {hints.at("icon_data"), false};
        return {"", true};
      }();

      if (is_path && !key.empty()) {
        return Gdk::Pixbuf::create_from_file(key);
      } else if (!key.empty()) {
        auto [width, height, rowstride, has_alpha, bits_per_sample, channels, image_data, _] =
          DBus::Struct<int, int, int, bool, int, int, std::vector<uint8_t>>(hints.at("image-data"));
        return Gdk::Pixbuf::create_from_data(image_data.data(), Gdk::Colorspace::COLORSPACE_RGB,
                                             has_alpha, bits_per_sample, width, height, rowstride);
      }

    } catch (Glib::FileError& e) {
      LOGE("FileError: {}", e.what());
    }
    return Glib::RefPtr<Gdk::Pixbuf>{};
  }

  auto NotificationServer::GetCapabilities(DBus::Error& e) -> std::vector<std::string>
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
                                  const int32_t& expire_timeout, DBus::Error& e) -> uint32_t
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

    Urgency urgency = Urgency::Normal;
    if (hints.count("urgency")) urgency = Urgency{uint8_t(hints.at("urgency"))};

    auto image = get_image(hints, app_icon);
    notifications.emplace_back(*this, notification_id, summary, body, actions, urgency, image);

    return notification_id;
  }

  auto NotificationServer::CloseNotification(const uint32_t& id, DBus::Error& e) -> void {
    auto found = util::find_if(notifications, [id] (Notification& n) { return n.id == id; });
    if (found != notifications.end()) notifications.underlying().erase(found.data());
  }

  auto NotificationServer::GetServerInformation(std::string& name,
                                                std::string& vendor,
                                                std::string& version,
                                                std::string& spec_version, DBus::Error& e) -> void
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
                             Urgency urgency,
                             Glib::RefPtr<Gdk::Pixbuf> pixbuf)
    : server(server), id(id), pixbuf(pixbuf)
  {
    window.set_title("Cloth Notification");
    window.set_decorated(false);

    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    server.client.style_context->add_provider_for_screen(screen, server.client.css_provider,
                                                         GTK_STYLE_PROVIDER_PRIORITY_USER);

    title.set_markup(fmt::format("<b>{}</b>", title_str));
    body.set_text(body_str);
    auto& actions_box = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    auto prev = actions.begin();
    auto cur = prev + 1;
    for (; cur != actions.end() && prev != actions.end();
         std::advance(cur, 2), std::advance(prev, 2)) {
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
    if (!body_str.empty()) box2.pack_start(body);
    if (!actions.empty()) box2.pack_start(actions_box);

    auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    box1.pack_end(box2);

    if (pixbuf) {
      auto w = pixbuf->get_width();
      auto h = pixbuf->get_height();
      if (w > max_image_width || h > max_image_height) {
        auto scale = std::min(max_image_width / float(w), max_image_height / float(h));
        this->pixbuf = pixbuf->scale_simple(w * scale, h * scale, Gdk::InterpType::INTERP_BILINEAR);
      }
      image.set(this->pixbuf);
      box1.pack_start(image);
    }

    window.signal_button_press_event().connect([this](GdkEventButton* evt) {
      util::erase_this(this->server.notifications, this);
      return false;
    });

    window.add(box1);
    switch (urgency) {
    case Urgency::Low: window.get_style_context()->add_class("urgency-low"); break;
    case Urgency::Normal: window.get_style_context()->add_class("urgency-normal"); break;
    case Urgency::Critical: window.get_style_context()->add_class("urgency-critical"); break;
    }

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = server.client.layer_shell.get_layer_surface(
      surface, server.client.output, wl::zwlr_layer_shell_v1_layer::overlay, "cloth.notification");
    layer_surface.set_size(1, 1);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      LOGD("Configured");
      layer_surface.ack_configure(serial);
      window.show_all();
      if (width != window.get_width()) {
        layer_surface.set_size(window.get_width(), window.get_height());
        layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::top |
                                 wl::zwlr_layer_surface_v1_anchor::right);
        layer_surface.set_margin(20, 20, 20, 20);
        layer_surface.set_exclusive_zone(0);
        surface.commit();
      }
    };
    layer_surface.on_closed() = [&] { util::erase_this(this->server.notifications, this); };

    window.resize(1, 1);

    surface.commit();
    LOGD("Constructed");
  } // namespace cloth::notifications

  Notification::~Notification()
  {
    server.NotificationClosed(id, 0);
    LOGD("Destructed");
  }

} // namespace cloth::notifications
