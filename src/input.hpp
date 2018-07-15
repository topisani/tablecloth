#pragma once

#include "wlroots.hpp"

#include "config.hpp"
#include "cursor.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"

namespace cloth {
  struct Input {
    Input(Server& server, Config& config) noexcept;
    ~Input() noexcept;

    Seat* seat_from_wlr_seat(wlr::seat_t& seat);
    bool view_has_focus(View& view);
    Seat& get_seat(std::string_view);
    Seat* last_active_seat();

    Config& config;
    Server& server;

    wl::Listener on_new_input;

    std::vector<Seat> seats;
  };

} // namespace cloth
