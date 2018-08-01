#include "unistd.h"
#include "util/logging.hpp"

#include "config.hpp"
#include "server.hpp"
#include "seat.hpp"
extern "C" {
#include <wlr/util/log.h>
}

using namespace cloth;

extern "C" int main(int argc, char **argv) {
	wlr_log_init(WLR_DEBUG, NULL);

  auto server = Server(argc, argv);
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		LOGE("Unable to open wayland socket: {}", strerror(errno));
		return 1;
	}

	LOGI("Running compositor on wayland display '{}'", socket);
	setenv("_WAYLAND_DISPLAY", socket, true);

	if (!wlr_backend_start(server.backend)) {
		LOGE("Failed to start backend");
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);
#ifdef WLR_HAS_XWAYLAND
	if (server.desktop.xwayland != nullptr) {
		Seat& xwayland_seat = server.input.get_seat(Config::default_seat_name);
		wlr_xwayland_set_seat(server.desktop.xwayland, xwayland_seat.wlr_seat);
	}
#endif

	if (!server.config.startup_cmd.empty()) {
		const char *cmd = server.config.startup_cmd.c_str();
		pid_t pid = fork();
		if (pid < 0) {
			LOGE("cannot execute binding command: fork() failed");
		} else if (pid == 0) {
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)nullptr);
		}
	}

	wl_display_run(server.wl_display);

	return 0;
}
