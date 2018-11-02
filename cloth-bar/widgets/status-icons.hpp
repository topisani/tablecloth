#pragma once

#include "util/logging.hpp"

#include "client.hpp"

#include <dbus-status-notifier-item-adaptor.hpp>
#include <dbus-status-notifier-watcher-adaptor.hpp>

namespace cloth::bar::widgets::sni {

  struct Item : org::kde::StatusNotifierItem_adaptor,
                DBus::IntrospectableAdaptor,
                DBus::ObjectAdaptor {
    static inline const std::string server_path = "/org/kde/StatusNotifierItem";
    static inline const std::string server_name = "org.kde.StatusNotifierItem";

    Item(DBus::Connection& connection) : DBus::ObjectAdaptor(connection, server_path)
    {
      cloth_debug("Registered StatusNotifierItem");
    }

    auto Activate(const int32_t& x, const int32_t& y, ::DBus::Error& error) -> void
    {
      cloth_debug("Activate {}x{}", x, y);
    }

    auto SecondaryActivate(const int32_t& x, const int32_t& y, ::DBus::Error& error) -> void
    {
      cloth_debug("SecondaryActivate {}x{}", x, y);
    }

    auto Scroll(const int32_t& delta, const std::string& dir, ::DBus::Error& error) -> void
    {
      cloth_debug("Scroll {} {}", delta, dir);
    }
  };

  struct Watcher : org::kde::StatusNotifierWatcher_adaptor,
                   DBus::IntrospectableAdaptor,
                   DBus::ObjectAdaptor {
    static inline const std::string server_path = "/org/kde/StatusNotifierWatcher";
    static inline const std::string server_name = "org.kde.StatusNotifierWatcher";

    Watcher(DBus::Connection& connection) : DBus::ObjectAdaptor(connection, server_path)
    {
      cloth_debug("Registered StatusNotifierWatcher");
    }

    auto RegisterStatusNotifierItem(const std::string& service, ::DBus::Error& error) -> void
    {
      cloth_debug("Registered item {}", service);
    }

    auto RegisterNotificationHost(const std::string& service, ::DBus::Error& error) -> void
    {
      cloth_debug("Registered host {}", service);
    }

    auto ProtocolVersion(::DBus::Error& error) -> std::string
    {
      return "";
    }

    auto IsNotificationHostRegistered(::DBus::Error& error) -> bool
    {
      return true;
    }

  };

  struct Host : DBus::InterfaceAdaptor, DBus::ObjectAdaptor {
    static inline const std::string server_path = "/org/kde/StatusNotifierHost";
    static inline const std::string server_name = "org.kde.StatusNotifierHost";

    Host(DBus::Connection& connection)
      : DBus::InterfaceAdaptor(server_name), DBus::ObjectAdaptor(connection, server_path)
    {}
  };

  struct TrayWidget {

    TrayWidget();

    operator Gtk::Widget&();

  private:
    DBus::Connection connection_;
    Watcher watcher_;
    Host host_;
  };

} // namespace cloth::bar::widgets::sni
