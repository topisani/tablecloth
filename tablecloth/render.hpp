#pragma once

#include <pixman.h>

#include "util/chrono.hpp"
#include "util/macros.hpp"
#include "util/ptr_vec.hpp"

#include "layers.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Desktop;
  struct View;
  struct DragIcon;
  struct Workspace;
  struct Output;
  struct Input;

  using Layer = util::ptr_vec<LayerSurface>;

  namespace render {


    struct LayoutData {
      double x = 0;
      double y = 0;
      double width = 0;
      double height = 0;
      float rotation = 0;

      DEFAULT_EQUALITY(LayoutData, x, y, width, height, rotation);
    };

    struct RenderData {
      LayoutData layout;
      float alpha = 1;

      DEFAULT_EQUALITY(RenderData, layout, alpha);
    };

    struct ViewAndData {
      constexpr ViewAndData(View& view, RenderData data = {}) noexcept : view(view), data(data){};
      ;
      View& view;
      RenderData data;
    };

    struct Context {
      Context(Output& output);

      auto do_render() -> void;

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

      auto reset() -> void;

      // DATA //

      Output& output;

      wlr::renderer_t* renderer = nullptr;
      chrono::time_point when = chrono::clock::now();
      std::vector<ViewAndData> views;
      std::array<float, 4> clear_color = {0.25f, 0.25f, 0.25f, 1.0f};
      View* fullscreen_view = nullptr;
      wlr::output_damage_t* damage;
      wlr::box_t* output_box;

    private:
      auto draw_shadow(wlr::box_t box, float rotation, float alpha, float radius, float offset)
        -> void;

      auto render_decorations(View&, RenderData&) -> void;
      auto render(View&, RenderData&) -> void;
      auto render(Layer&) -> void;

      auto damage_done() -> void;
      auto layers_send_done() -> void;

      auto for_each_surface(wlr::surface_t& surface,
                            wlr_surface_iterator_func_t iterator,
                            const RenderData& data) -> void;
      auto for_each_surface(View&, wlr_surface_iterator_func_t, const RenderData&) -> void;

      auto for_each_drag_icon(Input& input, wlr_surface_iterator_func_t iterator, RenderData data)
        -> void;

#ifdef WLR_HAS_XWAYLAND
      auto for_each_surface(wlr::xwayland_surface_t& surface,
                            wlr_surface_iterator_func_t,
                            RenderData) -> void;
#endif

      /// Callback for wlr for_each functions
      ///
      /// \param data is ContextAndData
      static auto render_surface(wlr::surface_t* surface, int sx, int sy, void* data) -> void;

      pixman_region32 pixman_damage;
    };

  } // namespace render
} // namespace cloth
