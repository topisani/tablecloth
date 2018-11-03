#pragma once

#include <cmath>
#include <optional>
#include <vector>

#include "util/chrono.hpp"
#include "util/logging.hpp"

#include "wlroots.hpp"

namespace cloth {

  enum struct Side { none = 0, top, right, bottom, left };

  struct Point {
    double x;
    double y;

    double dist(Point p2) const
    {
      return std::sqrt(std::pow(p2.x - x, 2) + std::pow(p2.y - y, 2));
    }

    double distx(double x2) const
    {
      return std::abs(x2 - x);
    }

    double disty(double y2) const
    {
      return std::abs(y2 - y);
    }
  };

  struct TouchGesture {
    static constexpr int radius = 10;
    static constexpr int min_length = 100;
    static constexpr chrono::duration max_time = chrono::seconds(1);

    bool on_touch_up(Point p)
    {
      if (chrono::clock::now() > start + max_time) {
        cloth_debug("Gesture cancelled: Too long");
        return false;
      }
      switch (side) {
      case Side::top:
        if (start_point.distx(p.x) >= start_point.disty(p.y)) return false;
        if (p.y - start_point.y >= min_length) return true;
      case Side::left:
        if (start_point.disty(p.y) >= start_point.distx(p.x)) return false;
        if (p.x - start_point.x >= min_length) return true;
      case Side::bottom:
        if (start_point.distx(p.x) >= start_point.disty(p.y)) return false;
        if (start_point.y - p.y >= min_length) return true;
      case Side::right:
        if (start_point.disty(p.y) >= start_point.distx(p.x)) return false;
        if (start_point.x - p.y >= min_length) return true;
      default: break;
      }
      return false;
    }

    static auto create(int touch_id, Point start_point, wlr::box_t output_box)
      -> std::optional<TouchGesture>
    {
      TouchGesture res;
      if (std::abs(start_point.distx(output_box.x)) <= radius) {
        res.side = Side::left;
      } else if (start_point.disty(output_box.y) <= radius) {
        res.side = Side::top;
      } else if (start_point.distx(output_box.x + output_box.width) <= radius) {
        res.side = Side::right;
      } else if (start_point.disty(output_box.y + output_box.height) <= radius) {
        res.side = Side::bottom;
      }
      if (res.side == Side::none) {
        return std::nullopt;
      }
      res.start_point = start_point;
      res.touch_id = touch_id;
      return res;
    }

    chrono::time_point start = chrono::clock::now();
    Point start_point;
    int touch_id;
    Side side = Side::none;

  private:
    TouchGesture() {}
  };

} // namespace cloth
