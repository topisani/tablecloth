#pragma once

#include <dbus-status-notifier-watcher-adaptor.hpp>
#include <dbus-status-notifier-item-adaptor.hpp>

#include "util/logging.hpp"

namespace cloth::bar::widgets {

  struct StatusNotifierWatcher : org::kde::StatusNotifierWatcher_adaptor,
                                 DBus::IntrospectableAdaptor,
                                 DBus::ObjectAdaptor {
    static inline const std::string server_path = "/org/kde/StatusNotifierWatcher";
    static inline const std::string server_name = "org.kde.StatusNotifierWatcher";

    StatusNotifierWatcher(DBus::Connection& connection)
      : DBus::ObjectAdaptor(connection, server_path)
    {
      LOGD("Registered StatusNotifierWatcher");
    }

    auto RegisterStatusNotifierItem(const std::string& service, ::DBus::Error &error) -> void
    {
      LOGD("Registered item {}", service);
    }

    auto RegisterNotificationHost(const std::string& service, ::DBus::Error &error) -> void
    {
      LOGD("Registered host {}", service);
    }

    auto ProtocolVersion(::DBus::Error &error) -> std::string
    {
    }

    auto IsNotificationHostRegistered(::DBus::Error &error) -> bool
    {
      return true;
    }

  };

  struct StatusIcons {};

} // namespace cloth::bar::widgets
