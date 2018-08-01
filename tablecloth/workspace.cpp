#include "workspace.hpp"

#include "util/chrono.hpp"
#include "util/ptr_vec.hpp"

#include "desktop.hpp"
#include "layers.hpp"
#include "output.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"
#include "wlroots.hpp"

#include "util/iterators.hpp"

namespace cloth {

  util::ref_vec<View> Workspace::visible_views()
  {
    return util::view::filter(_views, [](auto& v) { return v.mapped; });
  }

  auto Workspace::focused_view() -> View*
  {
    auto visviews = visible_views();
    return visviews.begin() == visviews.end() ? nullptr : &visviews.back();
  }

  auto Workspace::outputs() -> util::ref_vec<Output>
  {
    return util::view::filter(desktop.outputs, [this](Output& o) { return o.workspace == this; });
  }

  auto Workspace::is_current() -> bool
  {
    return &desktop.current_workspace() == this;
  }

  auto Workspace::is_visible() -> bool
  {
    return util::any_of(desktop.outputs, [this](Output& o) { return o.workspace == this; });
  }

  auto Workspace::set_focused_view(View* view) -> View*
  {
    if (view == nullptr) {
      return nullptr;
    }
    View* prev_focus = focused_view();

    _views.rotate_to_back(*view);

    if (is_current()) {
      for (auto&& seat : desktop.server.input.seats) {
        seat.set_focus(view);
      };
    }

    bool unfullscreen = true;

#ifdef WLR_HAS_XWAYLAND
    if (auto* xwl_view = dynamic_cast<XwaylandSurface*>(view);
        xwl_view && xwl_view->xwayland_surface->override_redirect) {
      unfullscreen = false;
    }
#endif

    if (view && unfullscreen) {
      Desktop& desktop = view->desktop;
      wlr::box_t box = view->get_box();
      if (this->fullscreen_view && this->fullscreen_view != view &&
          util::any_of(outputs(), [&](Output& output) {
            return wlr_output_layout_intersects(desktop.layout, &output.wlr_output, &box);
          })) {
        this->fullscreen_view->set_fullscreen(false, nullptr);
      }
    }

    desktop.server.window_manager.send_focused_window_name(*this);

    if (view == prev_focus) {
      return view;
    }

#ifdef WLR_HAS_XWAYLAND
    if (auto* xwl_view = dynamic_cast<XwaylandSurface*>(view);
        xwl_view && wlr_xwayland_surface_is_unmanaged(xwl_view->xwayland_surface)) {
      return view;
    }
#endif

    return view;
  }

  auto Workspace::cycle_focus() -> View*
  {
    auto views = visible_views();
    auto rviews = util::view::reverse(views);
    if (views.empty()) {
      return focused_view();
    }

    auto first_view = &*rviews.begin();
    if (!first_view->is_focused()) {
      return set_focused_view(first_view);
    }
    auto next_view = ++rviews.begin();
    if (next_view != rviews.end()) {
      // Focus the next view. Invalidates the iterator next_view;
      auto nvp = set_focused_view(&*next_view);
      // Move the first view to the front of the list
      _views.rotate_to_front(*first_view);
      return nvp;
    }
    return nullptr;
  }

  auto Workspace::add_view(std::unique_ptr<View>&& view_ptr) -> View&
  {
    view_ptr->workspace = this;
    return _views.push_back(std::move(view_ptr));
  }

  auto Workspace::erase_view(View& v) -> std::unique_ptr<View>
  {
    return _views.erase(v);
  }


} // namespace cloth
