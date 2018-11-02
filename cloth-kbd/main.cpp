#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

#include <csignal>
#include "client.hpp"

static cloth::kbd::Client* client;

auto quit_handler(int signal) -> void {
  Glib::signal_idle().connect_once([signal] {
    cloth_error("Exiting from signal {}", signal);
    client->gtk_main.quit();
  });
}

int main(int argc, char* argv[])
{
  try {
    cloth::kbd::Client c(argc, argv);
    client = &c;

    std::signal(SIGINT, quit_handler);
    std::signal(SIGTERM, quit_handler);
    std::signal(SIGUSR1, quit_handler);

    return c.main(argc, argv);
  } catch (const std::exception& e) {
    cloth_error(e.what());
    return 1;
  } catch (const Glib::Exception& e) {
    cloth_error(e.what().c_str());
    return 1;
  }

  return 0;
}
