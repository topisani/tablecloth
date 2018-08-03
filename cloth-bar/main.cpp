#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

#include "client.hpp"

namespace cloth::bar {

  using namespace clara;
  namespace wl = wayland;

} // namespace cloth::bar

int main(int argc, char* argv[])
{
  try {
    cloth::bar::Client c(argc, argv);
    return c.main(argc, argv);
  } catch (std::runtime_error& e) {
    LOGE(e.what());
    return 1;
  }
}
