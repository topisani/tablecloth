#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

#include "client.hpp"

#include <csignal>

namespace cloth::bar {

  using namespace clara;
  namespace wl = wayland;

  static Client* client;

} // namespace cloth::bar


int main(int argc, char* argv[])
{
  try {
    cloth::bar::Client c(argc, argv);
    cloth::bar::client = &c;
    std::signal(SIGUSR1, [] (int signal) {
      for (auto& bar : cloth::bar::client->bars) {
        bar.toggle();
      }
    });

    return c.main(argc, argv);
  } catch (const std::exception& e) {
    LOGE(e.what());
    return 1;
  } catch (const Glib::Exception& e) {
    LOGE(e.what().c_str());
    return 1;
  }
}
