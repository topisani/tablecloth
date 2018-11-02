#include <clara.hpp>

#include <gtkmm.h>
#include <wayland-client.hpp>

#include <protocols.hpp>

#include "gdkwayland.hpp"
#include "util/logging.hpp"

#include "client.hpp"

int main(int argc, char* argv[])
{
  try {
    cloth::lock::Client c(argc, argv);

    return c.main(argc, argv);
  } catch (const std::exception& e) {
    cloth_error(e.what());
    return 1;
  } catch (const Glib::Exception& e) {
    cloth_error(e.what().c_str());
    return 1;
  }
}
