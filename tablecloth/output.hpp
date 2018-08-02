#pragma once

#include <pixman.h>

#include "util/chrono.hpp"
#include "util/ptr_vec.hpp"

#include "wlroots.hpp"
#include "layers.hpp"

namespace cloth {

  struct Desktop;
  struct View;
  struct DragIcon;
  struct Workspace;

  struct Output {
    Output(Desktop& desktop, Workspace& ws, wlr::output_t& wlr) noexcept;

    Output(const Output&) = delete;
    Output& operator=(const Output&) noexcept = delete;

    Output(Output&& rhs) noexcept = default;
    Output& operator=(Output&&) noexcept = default;

    void damage_whole();
    void damage_whole_view(View& view);
    void damage_from_view(View& view);
    void damage_whole_drag_icon(DragIcon& icon);
    void damage_from_local_surface(wlr::surface_t& surface,
                                          double ox,
                                          double oy,
                                          float rotation = 0);
    void damage_whole_local_surface(wlr::surface_t& surface,
                                           double ox,
                                           double oy,
                                           float rotation = 0);
    // Member variables

    Desktop& desktop;

    std::array<util::ptr_vec<LayerSurface>, 4> layers;

    util::non_null_ptr<Workspace> workspace;
    wlr::output_t& wlr_output;

    chrono::time_point last_frame;

    wlr::output_damage_t* damage;
    wlr::box_t usable_area;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_mode;
    wl::Listener on_transform;
    wl::Listener on_damage_frame;
    wl::Listener on_damage_destroy;

  private:
    void render();
  };



} // namespace cloth
