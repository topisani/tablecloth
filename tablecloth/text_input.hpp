#pragma once

#include "util/ptr_vec.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct InputMethodRelay;

  struct TextInput {
    TextInput(InputMethodRelay& relay, wlr::text_input_v3_t* input);

    InputMethodRelay& relay;
    wlr::text_input_v3_t* input;

    // The surface getting seat's focus. Stored for when text-input cannot
    // be sent an enter event immediately after getting focus, e.g. when
    // there's no input method available. Cleared once text-input is entered.
    wlr::surface_t* pending_focused_surface;
    wl::Listener pending_focused_surface_destroy;
  };

  /**
   * The relay structure manages the relationship between text-input and
   * input_method interfaces on a given seat. Multiple text-input interfaces may
   * be bound to a relay, but at most one will be focused (reveiving events) at
   * a time. At most one input-method interface may be bound to the seat. The
   * relay manages life cycle of both sides. When both sides are present and
   * focused, the relay passes messages between them.
   *
   * Text input focus is a subset of keyboard focus - if the text-input is
   * in the focused state, wl_keyboard sent an enter as well. However, having
   * wl_keyboard focused doesn't mean that text-input will be focused.
   */
  struct InputMethodRelay {
    InputMethodRelay(Seat& seat) : seat(seat){};

    /// Updates currently focused surface. Surface must belong to the same seat.
    void set_focus(wlr::surface_t* surface) {};

    Seat& seat;
    util::ptr_vec<TextInput> text_inputs;
    wlr::input_method_v2_t* input_method; // doesn't have to be present

    wl::Listener text_input_new;
    wl::Listener text_input_enable;
    wl::Listener text_input_commit;
    wl::Listener text_input_disable;
    wl::Listener text_input_destroy;
    wl::Listener input_method_new;
    wl::Listener input_method_commit;
    wl::Listener input_method_destroy;
  };
} // namespace cloth
