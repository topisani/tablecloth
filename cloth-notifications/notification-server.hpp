#pragma once

#include "util/logging.hpp"

#include <gtkmm.h>

#include <protocols.hpp>
#include <util/ptr_vec.hpp>

#include <dbus-notifications-adaptor.hpp>

// These macro names are a bit too generic, dbus++
#undef bind_property
#undef register_method
#undef connect_signal

namespace cloth::notifications {

  namespace wl = wayland;

  struct Client;
  struct NotificationServer;

  struct Notification {
    Notification(NotificationServer& server,
                 unsigned id,
                 const std::string& title,
                 const std::string& body,
                 const std::vector<std::string>& actions,
                 const std::string& app_icon);

    ~Notification();

    NotificationServer& server;
    const unsigned id;

    Gtk::Window window;
    Gtk::Image image;
    Gtk::Label title;
    Gtk::Label body;
    std::vector<Gtk::Button> actions;

    wl::surface_t surface;
    wl::zwlr_layer_surface_v1_t layer_surface;
  };

  struct NotificationServer : org::freedesktop::Notifications_adaptor,
                              DBus::IntrospectableAdaptor,
                              DBus::ObjectAdaptor {
    static inline const std::string server_path = "/org/freedesktop/Notifications";
    static inline const std::string server_name = "org.freedesktop.Notifications";

    NotificationServer(Client& client, DBus::Connection& connection)
      : DBus::ObjectAdaptor(connection, server_path), client(client)
    {}

    auto GetCapabilities() -> std::vector<std::string> override;
    auto Notify(const std::string&,
                const uint32_t&,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::vector<std::string>&,
                const std::map<std::string, ::DBus::Variant>&,
                const int32_t&) -> uint32_t override;
    auto CloseNotification(const uint32_t&) -> void override;
    auto GetServerInformation(std::string&, std::string&, std::string&, std::string&)
      -> void override;

    Client& client;

    util::ptr_vec<Notification> notifications;

  private:
    unsigned _id;
  };


} // namespace cloth::notifications
