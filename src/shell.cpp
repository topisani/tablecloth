#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/config.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/wlr_renderer.h>
}

#include "server.hpp"

namespace cloth {
int main(int argc, char **argv) {
  Server server;
  wl_display_run(server.wl_display);
  wl_display_destroy(server.wl_display);
  return 0;
}
}

int main(int argc, char** argv) {
  return cloth::main(argc, argv);
}
