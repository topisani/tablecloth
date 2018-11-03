#include "seat.hpp"

#include "cursor.hpp"
#include "input.hpp"
#include "keyboard.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "xcursor.hpp"

#include "util/algorithm.hpp"
#include "util/exception.hpp"
#include "util/logging.hpp"

namespace cloth {

  TabletTool::TabletTool(Seat& seat, wlr::tablet_v2_tablet_tool_t& tool) noexcept
    : seat(seat), tablet_v2_tool(tool)
  {
    cloth_debug("Create tablet tool");
    tablet_v2_tool.wlr_tool->data = this;

    on_set_cursor.add_to(tablet_v2_tool.events.set_cursor);
    on_set_cursor = [this](void* data) {
      auto& evt = *(wlr::tablet_v2_event_cursor_t*) data;
      wlr::seat_pointer_request_set_cursor_event_t event = {
        .seat_client = evt.seat_client,
        .surface = evt.surface,
        .serial = evt.serial,
        .hotspot_x = evt.hotspot_x,
        .hotspot_y = evt.hotspot_y,
      };
      this->seat.cursor.on_request_set_cursor((void*) &event);
    };

    on_tool_destroy.add_to(tablet_v2_tool.wlr_tool->events.destroy);
    on_tool_destroy = [] {  };
  }

  TabletTool::~TabletTool() noexcept {}

  // TABLET PAD //


  static void attach_tablet_pad(TabletPad& pad, Tablet& tablet)
  {
    cloth_debug("Attaching tablet pad \"{}\" to tablet \"{}\" ", pad.wlr_device.name,
         tablet.wlr_device.name);
    pad.tablet = &tablet;
    pad.on_tablet_destroy.add_to(tablet.wlr_device.events.destroy);
  }

  TabletPad::TabletPad(Seat& p_seat, wlr::tablet_v2_tablet_pad_t& p_pad) noexcept
    : Device(p_seat, *p_pad.wlr_device), tablet_v2_pad(p_pad)
  {
    wlr_device.data = this;
    on_tablet_destroy = [this] {
      tablet = nullptr;
      on_tablet_destroy.remove();
    };

    on_device_destroy.add_to(wlr_device.events.destroy);
    on_device_destroy = [this] { util::erase_this(seat.tablet_pads, this); };

    on_attach.add_to(wlr_device.tablet_pad->events.attach_tablet);
    on_attach = [this](void* data) {
      auto* wlr_tool = (wlr::tablet_tool_t*) data;
      attach_tablet_pad(*this, *(Tablet*) wlr_tool->data);
    };

    on_ring.add_to(wlr_device.tablet_pad->events.ring);
    on_ring = [this](void* data) {
      auto* event = (wlr::event_tablet_pad_ring_t*) data;
      wlr_tablet_v2_tablet_pad_notify_ring(&tablet_v2_pad, event->ring, event->position,
                                         event->source == WLR_TABLET_PAD_RING_SOURCE_FINGER,
                                         event->time_msec);
    };

    on_strip.add_to(wlr_device.tablet_pad->events.strip);
    on_strip = [this](void* data) {
      auto* event = (wlr::event_tablet_pad_strip_t*) data;
      wlr_tablet_v2_tablet_pad_notify_strip(&tablet_v2_pad, event->strip, event->position,
                                          event->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
                                          event->time_msec);
    };

    on_button.add_to(wlr_device.tablet_pad->events.button);
    on_button = [this](void* data) {
      auto* event = (wlr::event_tablet_pad_button_t*) data;
      wlr_tablet_v2_tablet_pad_notify_mode(&tablet_v2_pad, event->group, event->mode,
                                         event->time_msec);
      wlr_tablet_v2_tablet_pad_notify_button(&tablet_v2_pad, event->button, event->time_msec,
                                           (enum zwp_tablet_pad_v2_button_state) event->state);
    };

    /* Search for a sibling tablet */
    if (!wlr_input_device_is_libinput(&wlr_device)) {
      /* We can only do this on libinput devices */
      return;
    }
    struct libinput_device_group* group =
      libinput_device_get_device_group(wlr_libinput_get_device_handle(&wlr_device));
    for (auto& tablet : seat.tablets) {
      if (!wlr_input_device_is_libinput(&tablet.wlr_device)) {
        continue;
      }
      struct libinput_device* li_dev = wlr_libinput_get_device_handle(&tablet.wlr_device);
      if (libinput_device_get_device_group(li_dev) == group) {
        attach_tablet_pad(*this, tablet);
        break;
      }
    }
  }

  TabletPad::~TabletPad() noexcept
  {
    seat.update_capabilities();
  }

  // TABLET //

  Tablet::Tablet(Seat& p_seat, wlr::input_device_t& p_device) noexcept
    : Device(p_seat, p_device),
      tablet_v2(*wlr_tablet_create(seat.input.server.desktop.tablet_v2, seat.wlr_seat, &wlr_device))
  {
    wlr_device.data = this;

    on_device_destroy.add_to(wlr_device.events.destroy);
    on_device_destroy = [this] { util::erase_this(seat.tablets, this); };

    wlr_cursor_attach_input_device(seat.cursor.wlr_cursor, &wlr_device);

    seat.configure_cursor();

    struct libinput_device_group* group =
      libinput_device_get_device_group(wlr_libinput_get_device_handle(&wlr_device));
    for (auto& pad : seat.tablet_pads) {
      if (!wlr_input_device_is_libinput(&pad.wlr_device)) {
        continue;
      }

      struct libinput_device* li_dev = wlr_libinput_get_device_handle(&pad.wlr_device);
      if (libinput_device_get_device_group(li_dev) == group) {
        attach_tablet_pad(pad, *this);
      }
    }
  }

  Tablet::~Tablet() noexcept
  {
    wlr_cursor_detach_input_device(seat.cursor.wlr_cursor, &wlr_device);
    seat.update_capabilities();
  }


} // namespace cloth
