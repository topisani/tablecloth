#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <wayland-server.h>

#include "wlroots.hpp"

namespace cloth {

  namespace chrono {
    using namespace std::chrono;

    using clock = std::chrono::system_clock;
    using duration = clock::duration;
    using time_point = std::chrono::time_point<clock, duration>;

    inline struct timespec to_timespec(time_point t) noexcept
    {
      long secs = duration_cast<seconds>(t.time_since_epoch()).count();
      long nsc = nanoseconds(t.time_since_epoch()).count() % 1000000000;
      return {secs, nsc};
    }
  } // namespace chrono

  struct Server;

  struct Output {
    Output(struct wlr_output* wlr, Server& server) noexcept : server(server), wlr_output(wlr) {}

    Output(const Output&) = delete;
    Output& operator=(const Output&) noexcept = delete;

    Output(Output&& rhs) noexcept = default;
    Output& operator=(Output&&) noexcept = default;

    void frame_notify(void*);

    Server& server;
    struct wlr_output* wlr_output;

    chrono::time_point last_frame;

    wlr::Listener destroy;
    wlr::Listener frame;
  };

  struct Server {
    Server() noexcept;

    wl_display* wl_display;
    wl_event_loop* wl_event_loop;

    wlr_backend* backend;
    wlr_compositor* compositor;

    wlr::Listener new_output;

    std::vector<std::unique_ptr<Output>> outputs;
  };

} // namespace cloth
