#pragma once

#include <vector>
#include <chrono>
#include <functional>
#include <memory>

#include <wayland-server.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
}


namespace cloth {

  namespace chrono {
    using namespace std::chrono;

    using clock = std::chrono::system_clock;
    using duration = clock::duration;
    using time_point = std::chrono::time_point<clock, duration>;

    inline struct timespec to_timespec(time_point t) noexcept {
      long secs = duration_cast<seconds>(t.time_since_epoch()).count();
      long nsc = nanoseconds(t.time_since_epoch()).count() % 1000000000;
      return {secs, nsc};
    }
  }

  struct Listener {


    Listener(std::function<void(void*)> func) noexcept
      : _func (new std::function<void(void*)>(std::move(func)))
    {
      _listener.link = {nullptr, nullptr};
      _listener.notify = [] (wl_listener* lst, void* data) {
        Listener& self = *wl_container_of(lst, &self, _listener);
        if (self._func && *self._func) (*self._func)(data);
      };
    }

    Listener() noexcept
    : Listener(nullptr)
    {}

    ~Listener() noexcept {
      if (_func) delete _func;
      if (_listener.link.next) wl_list_remove(&_listener.link);
    }

    Listener(const Listener& rhs) = delete;

    Listener(Listener&& rhs) noexcept
    {
      swap(*this, rhs);
    }

    Listener& operator=(Listener&& rhs) noexcept {
      swap(*this, rhs);
      return *this;
    }

    Listener& operator=(std::function<void(void*)> func) noexcept {
      return this->operator=(Listener(func));
    }

    operator wl_listener& () noexcept {
      return _listener;
    }

    operator const wl_listener& () const noexcept {
      return _listener;
    }

    friend void swap(Listener& lhs, Listener& rhs) noexcept {
      std::swap(lhs._func, rhs._func);
      std::swap(lhs._listener, rhs._listener);
    }

    void add_to(wl_signal& sig) noexcept {
      wl_signal_add(&sig, &_listener);
    }

  private:
    std::function<void(void* data)>* _func;
    struct wl_listener _listener;
  };

  struct Server;

  struct Output {

    Output(struct wlr_output* wlr, Server& server) noexcept
      : server(server), wlr_output(wlr)
    {}

    Output(const Output&) = delete;
    Output& operator=(const Output&) noexcept = delete;

    Output(Output&& rhs) noexcept = default;
    Output& operator=(Output&&) noexcept = default;

    void frame_notify(void*);

    Server& server;
    struct wlr_output* wlr_output;

    chrono::time_point last_frame;

    Listener destroy;
    Listener frame;
  };

  struct Server {

    Server() noexcept;

    wl_display* wl_display;
    wl_event_loop* wl_event_loop;

    wlr_backend* backend;
    wlr_compositor* compositor;

    Listener new_output;

    std::vector<std::unique_ptr<Output>> outputs;
  };

} // namespace cloth
