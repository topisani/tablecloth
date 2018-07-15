#pragma once

#include "wlroots.hpp"

#include "server.hpp"
#include "seat.hpp"

namespace cloth {

  struct Input {

    Input(Server& server) noexcept;
    ~Input() noexcept;

    Seat& get_seat(std::string_view name) noexcept;

    Server& server;
    wlr::Listener new_input;
    std::vector<Seat> seats;
  };

}
