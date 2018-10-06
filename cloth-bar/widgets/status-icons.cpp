#include "status-icons.hpp"

#include "util/exception.hpp"

namespace cloth::bar::widgets::sni {

  TrayWidget::TrayWidget()
    : connection_([] {
        auto con = DBus::Connection::SessionBus();
        if (!con.acquire_name(Watcher::server_name.c_str())) {
          throw util::exception("Couldnt aquire name {}", Watcher::server_name);
        }
        if (!con.acquire_name(Host::server_name.c_str())) {
          throw util::exception("Couldnt aquire name {}", Host::server_name);
        }
        return con;
      }()),
      watcher_(connection_),
      host_(connection_)
  {
  }

} // namespace cloth::bar::widgets::sni
