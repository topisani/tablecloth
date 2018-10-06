#pragma once

#include "util/chrono.hpp"
#include "util/macros.hpp"
#include "util/ptr_vec.hpp"

#include "layers.hpp"
#include "render.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Output {
    Output(Desktop& desktop, Workspace& ws, wlr::output_t& wlr) noexcept;

    Output(const Output&) = delete;
    Output& operator=(const Output&) noexcept = delete;

    Output(Output&& rhs) noexcept = default;
    Output& operator=(Output&&) = default;

    auto damage_whole() -> void;
    auto damage_whole_view(View& view) -> void;
    auto damage_whole_decoration(View& view) -> void;
    auto damage_from_view(View& view) -> void;
    auto damage_whole_drag_icon(DragIcon& icon) -> void;
    auto damage_from_local_surface(wlr::surface_t& surface,
                                   double ox,
                                   double oy,
                                   float rotation = 0) -> void;
    auto damage_whole_local_surface(wlr::surface_t& surface,
                                    double ox,
                                    double oy,
                                    float rotation = 0) -> void;
    // Member variables

    Desktop& desktop;

    std::array<Layer, 4> layers;

    util::non_null_ptr<Workspace> workspace;
    wlr::output_t& wlr_output;

    chrono::time_point last_frame;

    wlr::box_t usable_area;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_mode;
    wl::Listener on_transform;
    wl::Listener on_damage_frame;
    wl::Listener on_damage_destroy;

  private:
    auto render() -> void;

    render::Context context = {*this};

    Workspace* prev_workspace = nullptr;

    float ws_alpha = 0.f;
  };

} // namespace cloth
