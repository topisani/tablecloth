#pragma once

#include <pixman.h>

#include "util/chrono.hpp"

#include "wlroots.hpp"
#include "layers.hpp"

namespace cloth {

  struct Desktop;
  struct View;
  struct DragIcon;

  struct Output {
    Output(struct wlr_output* wlr, Desktop& desktop) noexcept : desktop(desktop), wlr_output(wlr) {}

    Output(const Output&) = delete;
    Output& operator=(const Output&) noexcept = delete;

    Output(Output&& rhs) noexcept = default;
    Output& operator=(Output&&) noexcept = default;

    void frame_notify(void*);

    void handle_new_output(void* data);

    void damage_whole();
    void damage_whole_view(View& view);
    void damage_from_view(View& view);
    void damage_whole_drag_icon(DragIcon& icon);
    void damage_from_local_surface(wlr::surface_t& surface,
                                          double ox,
                                          double oy,
                                          float rotation);
    void damage_whole_local_surface(wlr::surface_t& surface,
                                           double ox,
                                           double oy,
                                           float rotation);
    // Member variables

    Desktop& desktop;
    struct wlr_output* wlr_output;

    View* fullscreen_view;

    std::array<std::vector<LayerSurface>, 4> layers;

    chrono::time_point last_frame;

    wlr::output_damage_t damage;
    wlr::box_t usable_area;

    wlr::Listener destroy;
    wlr::Listener mode;
    wlr::Listener transform;
    wlr::Listener damage_frame;
    wlr::Listener damage_destroy;
    wlr::Listener frame;
  };



} // namespace cloth
