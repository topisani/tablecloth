#include "notification-server.hpp"

#include "client.hpp"

#include "gdkwayland.hpp"
#include "util/iterators.hpp"

namespace cloth::notifications {

  auto get_image(const std::map<std::string, ::DBus::Variant>& hints, const std::string& app_icon)
    -> std::pair<Glib::RefPtr<Gdk::Pixbuf>, bool>
  {
    try {
      auto [key, is_path, is_icon] = [&]() -> std::tuple<std::string, bool, bool> {
        if (hints.count("image-data")) return {"image-data", false, false};
        if (hints.count("image_data")) return {"image_data", false, false}; // deprecated
        if (hints.count("image-path")) return {hints.at("image-path"), true, false};
        if (hints.count("image_path")) return {hints.at("image_path"), true, false}; // deprecated
        if (!app_icon.empty()) return {app_icon, true, true};
        if (hints.count("icon_data")) return {"icon_data", false, true};
        return {"", true, false};
      }();

      if (is_path && !key.empty()) {
        if (util::starts_with("file://", key)) key = key.substr(7);
        return std::pair(Gdk::Pixbuf::create_from_file(key), is_icon);
      } else if (!key.empty()) {
        // This looks terrible, I know. But I don't know how to do it better, please fix it :)
        auto [width, height, rowstride, has_alpha, bits_per_sample, channels, image_data,
            ign1, ign2, ign3, ign4, ign5, ign6, ign7, ign8, ign9] =
          DBus::Struct<int, int, int, bool, int, int, std::vector<uint8_t>>(hints.at("image-data"));
        cloth_debug("Image data: {}, {}, {}, {}, {}, {}", width, height, rowstride, has_alpha,
                    bits_per_sample, channels);
        return std::pair(
          Gdk::Pixbuf::create_from_data(image_data.data(), Gdk::Colorspace::COLORSPACE_RGB,
                                        has_alpha, bits_per_sample, width, height, rowstride),
          is_icon);
      }

    } catch (Glib::FileError& e) {
      cloth_error("FileError: {}", e.what());
    }
    return std::pair(Glib::RefPtr<Gdk::Pixbuf>{}, false);
  }

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
                                  const int32_t& expire_timeout_in) -> uint32_t
  {
    try {
      unsigned notification_id = not_id_in;
      int expire_timeout = expire_timeout_in;

      if (notification_id == 0) notification_id = ++_id;

      cloth_info("[{}]: {}", summary, body);
      std::ostringstream strm;
      strm << "hints = { ";
      for (auto& [k, v] : hints) {
        strm << k << "<" << char(v.reader().type()) << "> ";
      }
      strm << " }";
      cloth_debug("{}", strm.str());

      Urgency urgency = Urgency::Normal;
      if (hints.count("urgency"))
        urgency = [&] {
          auto& urg = hints.at("urgency");
          switch (urg.reader().type()) {
          case 'y': return Urgency{uint8_t(urg)};
          case 'u': return Urgency{uint8_t(uint32_t(urg))};
          case 'i': return Urgency{uint8_t(int32_t(urg))};
          default:
            cloth_error("Urgency hint has wrong type {}", urg.reader().type());
            return Urgency::Normal;
          }
        }();

      if (expire_timeout < 0) {
        switch (urgency) {
        case Urgency::Low: expire_timeout = 5; break;
        case Urgency::Normal: expire_timeout = 10; break;
        case Urgency::Critical: expire_timeout = 0; break;
        }
      }

      cloth_debug("Timeout: {}", expire_timeout);

      auto image = get_image(hints, app_icon);

      Glib::signal_idle().connect_once([=] {
        notifications.underlying().erase(
          util::remove_if(notifications.underlying(),
                          [&](auto& n_ptr) { return n_ptr->id == notification_id; }),
          notifications.underlying().end());
        notifications.emplace_back(*this, notification_id, summary, body, actions, urgency,
                                   expire_timeout, image);
        for (auto& n : notifications) {
          n.update_padding();
        }
      });

      return notification_id;
    } catch (std::exception& e) {
      cloth_error("NotificationServer::Notify: {}", e.what());
      return not_id_in;
    }
  }

  auto NotificationServer::CloseNotification(const uint32_t& id) -> void
  {
    Glib::signal_idle().connect_once([this, id = id] {
      auto found = util::find_if(notifications, [id](Notification& n) { return n.id == id; });
      if (found != notifications.end()) notifications.underlying().erase(found.data());
      for (auto& n : notifications) {
        n.update_padding();
      }
    });
  }

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

  auto NotificationServer::get_padding(Notification& elem) -> int
  {
    int res = 0;
    for (auto& n : notifications) {
      if (&n == &elem) break;
      res += n.height;
      res += 10;
    }
    return res;
  }

  // Notification //

  Notification::Notification(NotificationServer& server,
                             unsigned id,
                             const std::string& title_str,
                             const std::string& body_str,
                             const std::vector<std::string>& actions,
                             Urgency urgency,
                             int expire_timeout,
                             std::pair<Glib::RefPtr<Gdk::Pixbuf>, bool> image_data)
    : server(server), id(id), pixbuf(image_data.first)
  {
    window.set_title("Cloth Notification");
    window.set_decorated(false);

    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    server.client.style_context->add_provider_for_screen(screen, server.client.css_provider,
                                                         GTK_STYLE_PROVIDER_PRIORITY_USER);

    title.set_markup(fmt::format("<b>{}</b>", title_str));

    // Add CSS classes for easier styling
    title.get_style_context()->add_class("title");
    body.get_style_context()->add_class("body");

    // Set alignment (maybe we should export it as a CSS property)
    title.set_justify(Gtk::Justification::JUSTIFY_LEFT);
    body.set_justify(Gtk::Justification::JUSTIFY_LEFT);
    title.set_halign(Gtk::Align::ALIGN_START);
    body.set_halign(Gtk::Align::ALIGN_START);
    
    body.set_markup(body_str);

    body.set_line_wrap(true);
    body.set_max_width_chars(80);
    auto& actions_box = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    auto prev = actions.begin();
    auto cur = prev + 1;
    for (; cur != actions.end() && prev != actions.end();
         std::advance(cur, 2), std::advance(prev, 2)) {
      auto& action = *prev;
      auto& label = *cur;
      auto& button = this->actions.emplace_back(label);
      button.signal_clicked().connect([this, action = action, label = label] {
        cloth_debug("Action: {} -> {}", label, action);
        this->server.ActionInvoked(this->id, action);
        auto keep_around = util::erase_this(this->server.notifications, this);
        for (auto& n : this->server.notifications) {
          n.update_padding();
        }
      });
      actions_box.pack_start(button);
    }
    auto& box2 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    box2.pack_start(title);
    if (!body_str.empty()) box2.pack_start(body);
    if (!actions.empty()) box2.pack_start(actions_box);

    auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    box1.pack_end(box2);

    if (image_data.first) {
      auto w = image_data.first->get_width();
      auto h = image_data.first->get_height();
      cloth_debug("Image data now: {}, {}", w, h);
      if (w > (int) max_image_width || h > (int) max_image_height) {
        auto scale = std::min(max_image_width / float(w), max_image_height / float(h));
        this->pixbuf =
          image_data.first->scale_simple(w * scale, h * scale, Gdk::InterpType::INTERP_BILINEAR);
      }
      image.set(this->pixbuf);
      if (image_data.second) image.get_style_context()->add_class("icon");
      box1.pack_start(image);
    }

    window.signal_button_press_event().connect([this](GdkEventButton* evt) {
      auto keep_around = util::erase_this(this->server.notifications, this);
      for (auto& n : this->server.notifications) {
        n.update_padding();
      }
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
      surface, nullptr, wl::zwlr_layer_shell_v1_layer::top, "cloth.notification");
    layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::top |
                             wl::zwlr_layer_surface_v1_anchor::right);
    layer_surface.set_size(1, 1);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      cloth_debug("Configured {}x{}", width, height);
      layer_surface.ack_configure(serial);
      window.show_all();
      if ((int) width != window.get_width()) {
        this->height = window.get_height();
        layer_surface.set_size(window.get_width(), window.get_height());
        layer_surface.set_exclusive_zone(0);
        update_padding();
      }
    };
    layer_surface.on_closed() = [&] {
      auto keep_around = util::erase_this(this->server.notifications, this);
      for (auto& n : server.notifications) {
        n.update_padding();
      }
    };

    window.resize(1, 1);

    surface.commit();

    sleeper_thread = [this, expire_timeout] {
      sleeper_thread.sleep_for(chrono::seconds(expire_timeout));
      if (expire_timeout > 0 && sleeper_thread.running()) {
        Glib::signal_idle().connect_once([this] {
          auto keep_around = util::erase_this(this->server.notifications, this);
          for (auto& n : this->server.notifications) {
            n.update_padding();
          }
        });
      }
      sleeper_thread.stop();
    };
  } // namespace cloth::notifications

  Notification::~Notification()
  {
    server.NotificationClosed(id, 0);
  }

  void Notification::update_padding()
  {
    layer_surface.set_margin(20 + server.get_padding(*this), 20, 20, 20);
    surface.commit();
  }

} // namespace cloth::notifications
