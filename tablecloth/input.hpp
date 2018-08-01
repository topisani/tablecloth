#pragma once

#include "util/ptr_vec.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Server;
  struct Seat;
  struct Config;
  struct View;

  struct Input {
    Input(Server& server, Config& config) noexcept;
    ~Input() noexcept;

    Seat* seat_from_wlr_seat(wlr::seat_t& seat);
    bool view_has_focus(View& view);
    Seat& get_seat(const std::string&);
    Seat* last_active_seat();
    Seat& create_seat(const std::string& name);

    Server& server;
    Config& config;

    wl::Listener on_new_input;

    util::ptr_vec<Seat> seats;
  };

} // namespace cloth
