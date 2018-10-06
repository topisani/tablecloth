#pragma once

#include "wlroots.hpp"

namespace cloth {

  struct View;

  enum struct DecoPart {
    none = 0,
    top_border = (1 << 0),
    bottom_border = (1 << 1),
    left_border = (1 << 2),
    right_border = (1 << 3),
    titlebar = (1 << 4),
  };
  CLOTH_ENABLE_BITMASK_OPS(DecoPart);

  struct Decoration {
    Decoration(View& v);

    auto update() -> void;
    auto set_visible(bool visible) -> void;
    auto box() const noexcept -> wlr::box_t;
    auto part_at(double sx, double sy) const noexcept -> DecoPart;
    auto damage() -> void;
    auto is_visible()
    {
      return _visible;
    }
    auto border_width()
    {
      return _border_width;
    }
    auto titlebar_height()
    {
      return _titlebar_height;
    }
    auto shadow_offset() -> float;
    auto shadow_radius() -> float;

    View& view;

  private:
    int _border_width = 0;
    int _titlebar_height = 0;
    bool _visible = false;
    float _shadow_offset = 4;
    float _shadow_radius = 20;
  };

} // namespace cloth
