#pragma once

#include "util/chrono.hpp"
#include "util/ptr_vec.hpp"

#include "layers.hpp"
#include "view.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Output;

  struct Workspace {
    Workspace(Desktop& desktop, int index) : index(index), desktop(desktop){};

    const int index;
    Desktop& desktop;
    View* fullscreen_view = nullptr;

    std::array<util::ptr_vec<LayerSurface>, 4> layers;

    auto views() const noexcept -> const util::ptr_vec<View>&;
    auto visible_views() -> util::ref_vec<View>;

    /// Get all the outputs that this workspace is currently visible on
    auto outputs() -> util::ref_vec<Output>;

    /// Is this the workspace displayed on the currently active output?
    auto is_current() -> bool;
    /// Is this workspace displayed on any output?
    auto is_visible() -> bool;

    auto focused_view() -> View*;
    auto set_focused_view(View* view) -> View*;
    auto cycle_focus() -> View*;

    auto add_view(std::unique_ptr<View>&& v) -> View&;
    auto erase_view(View& v) -> std::unique_ptr<View>;

  private:
    util::ptr_vec<View> _views;
  };


  inline auto Workspace::views() const noexcept -> const util::ptr_vec<View>&
  {
    return _views;
  } 

} // namespace cloth
