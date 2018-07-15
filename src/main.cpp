#include "unistd.h"
#include "util/logging.hpp"

#include "config.hpp"
#include "server.hpp"

using namespace cloth;

int main(int argc, char **argv) {

  auto& server = Server::get();
	server.config = cloth::Config(argc, argv);
	server.wl_display = wl_display_create();
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
	assert(server.config && server.wl_display && server.wl_event_loop);

	server.backend = wlr_backend_autocreate(server.wl_display, nullptr);
	if (server.backend == nullptr) {
		LOGE("could not start backend");
		return 1;
	}

	server.renderer = wlr_backend_get_renderer(server.backend);
	assert(server.renderer);
	server.data_device_manager =
		wlr_data_device_manager_create(server.wl_display);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);
	//server.desktop = (&server, server.config);
	//server.input = input_create(&server, server.config);

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
	if (server.desktop->xwayland != nullptr) {
		Seat& xwayland_seat = server.input.get_seat(ROOTS_CONFIG_DEFAULT_SEAT_NAME);
		wlr_xwayland_set_seat(server.desktop->xwayland, xwayland_seat.wlr_seat);
	}
#endif

	if (server.config->startup_cmd != nullptr) {
		const char *cmd = server.config->startup_cmd;
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
