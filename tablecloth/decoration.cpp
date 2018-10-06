#include "decoration.hpp"

#include "view.hpp"

namespace cloth {

  Decoration::Decoration(View& v) : view(v) {}

  auto Decoration::update() -> void {}

  auto Decoration::set_visible(bool visible) -> void
  {
    view.damage_whole();
    _visible = visible;
    if (visible) {
      _border_width = 4;
      _titlebar_height = 0;
    } else {
      _border_width = 0;
      _titlebar_height = 0;
    }
    view.damage_whole();
  }

  auto Decoration::box() const noexcept -> wlr::box_t
  {
    auto box = view.get_box();
    if (_visible) {
      box.x -= _border_width;
      box.y -= (_border_width + _titlebar_height);
      box.width += _border_width * 2;
      box.height += (_border_width * 2 + _titlebar_height);
    }
    return box;
  }


  auto Decoration::part_at(double sx, double sy) const noexcept -> DecoPart
  {
    if (!_visible) {
      return DecoPart::none;
    }

    int sw = view.wlr_surface->current.width;
    int sh = view.wlr_surface->current.height;
    int bw = _border_width;
    int titlebar_h = _titlebar_height;

    if (sx > 0 && sx < sw && sy < 0 && sy > -_titlebar_height) {
      return DecoPart::titlebar;
    }

    DecoPart parts = DecoPart::none;
    if (sy >= -(titlebar_h + bw) && sy <= sh + bw) {
      if (sx < 0 && sx > -bw) {
        parts |= DecoPart::left_border;
      } else if (sx > sw && sx < sw + bw) {
        parts |= DecoPart::right_border;
      }
    }

    if (sx >= -bw && sx <= sw + bw) {
      if (sy > sh && sy <= sh + bw) {
        parts |= DecoPart::bottom_border;
      } else if (sy >= -(titlebar_h + bw) && sy < 0) {
        parts |= DecoPart::top_border;
      }
    }

    // TODO corners
    return parts;
  }

  auto Decoration::damage() -> void
  {
    view.damage_whole();
  }


} // namespace cloth
