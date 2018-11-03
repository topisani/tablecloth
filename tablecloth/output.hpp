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

    // Member variables

    Desktop& desktop;

    std::array<Layer, 4> layers;

    util::non_null_ptr<Workspace> workspace;
    wlr::output_t& wlr_output;

    chrono::time_point last_frame;

    wlr::box_t usable_area;

    render::Context context = {*this};

  protected:
    wl::Listener on_destroy;
    wl::Listener on_mode;
    wl::Listener on_transform;
    wl::Listener on_present;
    wl::Listener on_damage_frame;
    wl::Listener on_damage_destroy;

  private:
    auto render() -> void;


    Workspace* prev_workspace = nullptr;

    float ws_alpha = 0.f;
  };

} // namespace cloth
