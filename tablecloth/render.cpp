#include "render.hpp"

#include "util/logging.hpp"

#include "output.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"

namespace cloth {

  struct ContextAndData {
    RenderContext& context;
    const RenderData& data;
  };

  RenderContext::RenderContext(Output& output)
    : output(output), damage(wlr_output_damage_create(&output.wlr_output))
  {}

  //////////////////////////////////////////
  // Utility functions
  //////////////////////////////////////////

  /**
   * Rotate a child's position relative to a parent. The parent size is (pw, ph),
   * the child position is (*sx, *sy) and its size is (sw, sh).
   */
  static void rotate_child_position(double& sx,
                                    double& sy,
                                    double sw,
                                    double sh,
                                    double pw,
                                    double ph,
                                    float rotation)
  {
    if (rotation != 0.0) {
      // Coordinates relative to the center of the subsurface
      double ox = sx - pw / 2 + sw / 2, oy = sy - ph / 2 + sh / 2;
      // Rotated coordinates
      double rx = std::cos(rotation) * ox - std::sin(rotation) * oy,
             ry = std::cos(rotation) * oy + std::sin(rotation) * ox;
      sx = rx + pw / 2 - sw / 2;
      sy = ry + ph / 2 - sh / 2;
    }
  }

  static void get_layout_position(const LayoutData& data,
                                  double& lx,
                                  double& ly,
                                  const wlr::surface_t& surface,
                                  int sx,
                                  int sy)
  {
    double _sx = sx, _sy = sy;
    rotate_child_position(_sx, _sy, surface.current.width, surface.current.height, data.width,
                          data.height, data.rotation);
    lx = data.x + _sx;
    ly = data.y + _sy;
  }

  static bool has_standalone_surface(View& view)
  {
    if (!wl_list_empty(&view.wlr_surface->subsurfaces)) {
      return false;
    }

    if (auto* xdg_surface_v6 = dynamic_cast<XdgSurfaceV6*>(&view); xdg_surface_v6) {
      return wl_list_empty(&xdg_surface_v6->xdg_surface->popups);
    } else if (auto* xdg_surface = dynamic_cast<XdgSurface*>(&view); xdg_surface) {
      return wl_list_empty(&xdg_surface->xdg_surface->popups);
    } else if (auto* wl_shell_surface = dynamic_cast<WlShellSurface*>(&view); wl_shell_surface) {
      return wl_list_empty(&wl_shell_surface->wl_shell_surface->popups);
    } else
#ifdef WLR_HAS_XWAYLAND
      if (auto* wlr_surface = dynamic_cast<XwaylandSurface*>(&view); wlr_surface) {
      return wl_list_empty(&wlr_surface->xwayland_surface->children);
    }
#endif
    return true;
  }

  /**
   * Checks whether a surface at (lx, ly) intersects an output. If `box` is not
   * nullptr, it populates it with the surface box in the output, in output-local
   * coordinates.
   */
  static bool surface_intersect_output(wlr::surface_t& surface,
                                       wlr::output_layout_t& output_layout,
                                       wlr::output_t& wlr_output,
                                       double lx,
                                       double ly,
                                       float rotation,
                                       wlr::box_t* box)
  {
    double ox = lx, oy = ly;
    wlr_output_layout_output_coords(&output_layout, &wlr_output, &ox, &oy);

    ox += surface.sx;
    oy += surface.sy;

    if (box != nullptr) {
      box->x = ox * wlr_output.scale;
      box->y = oy * wlr_output.scale;
      box->width = surface.current.width * wlr_output.scale;
      box->height = surface.current.height * wlr_output.scale;
    }

    wlr::box_t layout_box = {
      .x = (int) lx,
      .y = (int) ly,
      .width = surface.current.width,
      .height = surface.current.height,
    };
    wlr_box_rotated_bounds(&layout_box, rotation, &layout_box);
    return wlr_output_layout_intersects(&output_layout, &wlr_output, &layout_box);
  }

  static void surface_send_frame_done(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& cvd = *(ContextAndData*) _data;
    auto when = chrono::to_timespec(cvd.context.when);
    float rotation = cvd.data.layout.rotation;

    double lx, ly;
    get_layout_position(cvd.data.layout, lx, ly, *surface, sx, sy);

    if (!surface_intersect_output(*surface, *cvd.context.output.desktop.layout,
                                  cvd.context.output.wlr_output, lx, ly, rotation, nullptr)) {
      return;
    }

    wlr_surface_send_frame_done(surface, &when);
  }


  //////////////////////////////////////////
  // RenderContext render functions
  //////////////////////////////////////////


  auto RenderContext::layers_send_done() -> void
  {
    auto when_ts = chrono::to_timespec(when);
    for (auto& layer : output.layers) {
      for (auto& surface : layer) {
        wlr::layer_surface_t& layer = surface.layer_surface;
        wlr_surface_send_frame_done(layer.surface, &when_ts);
        wlr::xdg_popup_t* popup;
        wl_list_for_each(popup, &surface.layer_surface.popups, link)
        {
          wlr_surface_send_frame_done(popup->base->surface, &when_ts);
        }
      }
    }
  }

  static void scissor_output(Output& output, pixman_box32_t* rect)
  {
    wlr::renderer_t* renderer = wlr_backend_get_renderer(output.wlr_output.backend);
    assert(renderer);

    wlr::box_t box = {
      .x = rect->x1,
      .y = rect->y1,
      .width = rect->x2 - rect->x1,
      .height = rect->y2 - rect->y1,
    };

    int ow, oh;
    wlr_output_transformed_resolution(&output.wlr_output, &ow, &oh);

    auto transform = wlr_output_transform_invert(output.wlr_output.transform);
    wlr_box_transform(&box, transform, ow, oh, &box);

    wlr_renderer_scissor(renderer, &box);
  }

  static wlr::box_t get_decoration_box(View& view, Output& output)
  {
    wlr::output_t& wlr_output = output.wlr_output;

    wlr::box_t deco_box = view.get_deco_box();
    double sx = deco_box.x - view.x;
    double sy = deco_box.y - view.y;
    rotate_child_position(sx, sy, deco_box.width, deco_box.height, view.wlr_surface->current.width,
                          view.wlr_surface->current.height, view.rotation);
    double x = sx + view.x;
    double y = sy + view.y;

    wlr::box_t box;

    wlr_output_layout_output_coords(output.desktop.layout, &wlr_output, &x, &y);

    box.x = x * wlr_output.scale;
    box.y = y * wlr_output.scale;
    box.width = deco_box.width * wlr_output.scale;
    box.height = deco_box.height * wlr_output.scale;
    return box;
  }

  auto RenderContext::render_decorations(View& view, RenderData& data) -> void
  {
    if (!view.decorated || view.wlr_surface == nullptr) {
      return;
    }

    wlr::renderer_t* renderer = wlr_backend_get_renderer(output.wlr_output.backend);
    assert(renderer);

    wlr::box_t box = get_decoration_box(view, output);

    wlr::box_t rotated;
    wlr_box_rotated_bounds(&box, view.rotation, &rotated);

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_union_rect(&damage, &damage, rotated.x, rotated.y, rotated.width,
                               rotated.height);
    pixman_region32_intersect(&damage, &damage, &pixman_damage);
    bool damaged = pixman_region32_not_empty(&damage);
    if (damaged) {
      float matrix[9];
      wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, view.rotation,
                             output.wlr_output.transform_matrix);
      float color[] = {0.2, 0.2, 0.2, view.alpha};

      int nrects;
      pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
      for (int i = 0; i < nrects; ++i) {
        scissor_output(output, &rects[i]);
        wlr_render_quad_with_matrix(renderer, color, matrix);
      }
    }

    pixman_region32_fini(&damage);
  }

  auto RenderContext::render_surface(wlr::surface_t* surface, int sx, int sy, void* _data) -> void
  {
    auto& cvd = *(ContextAndData*) _data;

    Output& output = cvd.context.output;
    float rotation = cvd.data.layout.rotation;

    wlr::texture_t* texture = wlr_surface_get_texture(surface);
    if (texture == nullptr) {
      return;
    }

    wlr::renderer_t* renderer = wlr_backend_get_renderer(output.wlr_output.backend);
    assert(renderer);

    double lx, ly;
    get_layout_position(cvd.data.layout, lx, ly, *surface, sx, sy);

    wlr::box_t box;
    bool intersects = surface_intersect_output(*surface, *output.desktop.layout, output.wlr_output,
                                               lx, ly, rotation, &box);
    if (!intersects) {
      return;
    }

    wlr::box_t rotated;
    wlr_box_rotated_bounds(&box, rotation, &rotated);

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_union_rect(&damage, &damage, rotated.x, rotated.y, rotated.width,
                               rotated.height);
    pixman_region32_intersect(&damage, &damage, &cvd.context.pixman_damage);
    bool damaged = pixman_region32_not_empty(&damage);
    if (damaged) {
      float matrix[9];
      auto transform = wlr_output_transform_invert(surface->current.transform);
      wlr_matrix_project_box(matrix, &box, transform, rotation, output.wlr_output.transform_matrix);

      int nrects;
      pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
      for (int i = 0; i < nrects; ++i) {
        scissor_output(output, &rects[i]);
        wlr_render_texture_with_matrix(renderer, texture, matrix, cvd.data.alpha);
      }
    }

    pixman_region32_fini(&damage);
  };


  auto RenderContext::render(View& view, RenderData& data) -> void
  {
    // Do not render views fullscreened on other outputs
    if (view.fullscreen_output != nullptr && view.fullscreen_output != &output) {
      return;
    }

    render_decorations(view, data);
    for_each_surface(view, render_surface, data);
  }

  auto RenderContext::render(Layer& layer) -> void
  {
    for (auto& layer_surface : layer) {
      // TODO: alpha and rotation for layer surfaces.
      RenderData data = {.layout = {
                           .x = layer_surface.geo.x + (double) output_box->x,
                           .y = layer_surface.geo.y + (double) output_box->y,
                           .width = (double) layer_surface.layer_surface.surface->current.width,
                           .height = (double) layer_surface.layer_surface.surface->current.height,
                         }};
      for_each_surface(*layer_surface.layer_surface.surface, render_surface, data);

      wlr_layer_surface_for_each_surface(&layer_surface.layer_surface, render_surface, &data);
    }
  } // namespace cloth


  auto RenderContext::do_render() -> void
  {
    renderer = wlr_backend_get_renderer(output.wlr_output.backend);
    assert(renderer);

    when = chrono::clock::now();

    output_box = wlr_output_layout_get_box(output.desktop.layout, &output.wlr_output);

    // Check if we can delegate the fullscreen surface to the output
    if (fullscreen_view != nullptr && fullscreen_view->view.wlr_surface != nullptr) {
      View& view = fullscreen_view->view;

      // Make sure the view is centered on screen
      wlr::box_t view_box = view.get_box();
      double view_x = (double) (output_box->width - view_box.width) / 2 + output_box->x;
      double view_y = (double) (output_box->height - view_box.height) / 2 + output_box->y;
      view.move(view_x, view_y);

      if (has_standalone_surface(view)) {
        wlr_output_set_fullscreen_surface(&output.wlr_output, view.wlr_surface);
      } else {
        wlr_output_set_fullscreen_surface(&output.wlr_output, nullptr);
      }

      // Fullscreen views are rendered on a black background
      clear_color = {0.f, 0.f, 0.f, 1.f};
    } else {
      wlr_output_set_fullscreen_surface(&output.wlr_output, nullptr);
    }

    bool needs_swap;
    pixman_region32_init(&pixman_damage);
    if (!wlr_output_damage_make_current(this->damage, &needs_swap, &pixman_damage)) {
      return;
    }

    // otherwise Output doesn't need swap and isn't damaged, skip rendering completely
    if (needs_swap) {
      wlr_renderer_begin(renderer, output.wlr_output.width, output.wlr_output.height);

      // otherwise Output isn't damaged but needs buffer swap
      if (pixman_region32_not_empty(&pixman_damage)) {
        if (output.desktop.server.config.debug_damage_tracking) {
          wlr_renderer_clear(renderer, (float[]){1, 1, 0, 1});
        }

        int nrects;
        pixman_box32_t* rects = pixman_region32_rectangles(&pixman_damage, &nrects);
        for (int i = 0; i < nrects; ++i) {
          scissor_output(output, &rects[i]);
          wlr_renderer_clear(renderer, clear_color.data());
        }

        render(output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
        render(output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

        // If a view is fullscreen on this output, render it
        if (fullscreen_view != nullptr) {
          auto& [view, data] = *fullscreen_view;

          if (output.wlr_output.fullscreen_surface == view.wlr_surface) {
            // The output will render the fullscreen view
            goto renderer_end;
          }


          if (view.wlr_surface != nullptr) {
            for_each_surface(view, render_surface, data);
          }

          // During normal rendering the xwayland window tree isn't traversed
          // because all windows are rendered. Here we only want to render
          // the fullscreen window's children so we have to traverse the tree.
#ifdef WLR_HAS_XWAYLAND
          if (auto* xwayland_surface = dynamic_cast<XwaylandSurface*>(&view); xwayland_surface) {
            for_each_surface(*xwayland_surface->xwayland_surface, render_surface, data);
          }
#endif
        } else {
          // Render all views
          for (auto& vd : views) {
            render(vd.view, vd.data);
          }
          // Render top layer above shell views
          render(output.layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
        }

        // Render drag icons
        RenderData data{.alpha = 1.f};
        for_each_drag_icon(output.desktop.server.input, render_surface, data);

        render(output.layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
      }

    renderer_end:
      wlr_renderer_scissor(renderer, nullptr);
      wlr_renderer_end(renderer);

      if (output.desktop.server.config.debug_damage_tracking) {
        int width, height;
        wlr_output_transformed_resolution(&output.wlr_output, &width, &height);
        pixman_region32_union_rect(&pixman_damage, &pixman_damage, 0, 0, width, height);
      }

      // PREV: update now?
      struct timespec now_ts = chrono::to_timespec(when);
      if (wlr_output_damage_swap_buffers(this->damage, &now_ts, &pixman_damage)) {
        when = chrono::to_time_point(now_ts);
        output.last_frame = output.desktop.last_frame = when;
      }
    }

    damage_done();
  }

  auto RenderContext::damage_done() -> void
  {
    // Damage finish

    pixman_region32_fini(&pixman_damage);

    // Send frame done events to all surfaces
    if (fullscreen_view != nullptr) {
      auto& [view, data] = *fullscreen_view;
      if (output.wlr_output.fullscreen_surface == view.wlr_surface) {
        // The surface is managed by the output.wlr_output
        return;
      }

      for_each_surface(view, surface_send_frame_done, data);

#ifdef WLR_HAS_XWAYLAND
      if (auto* xwayland_surface = dynamic_cast<XwaylandSurface*>(&view); xwayland_surface) {
        for_each_surface(*xwayland_surface->xwayland_surface, surface_send_frame_done, data);
      }
#endif
    } else {
      for (auto& [view, data] : views) {
        for_each_surface(view, surface_send_frame_done, data);
      }

      RenderData data{.alpha = 1.f};
      for_each_drag_icon(output.desktop.server.input, surface_send_frame_done, data);
    }
    layers_send_done();
  }


  //////////////////////////////////////////
  // RenderContext damage functions
  //////////////////////////////////////////


  static bool view_accept_damage(Output& output, View& view)
  {
    if (view.wlr_surface == nullptr) {
      return false;
    }
    if (output.workspace->fullscreen_view == nullptr) {
      return true;
    }
    if (output.workspace->fullscreen_view == &view) {
      return true;
    }
#ifdef WLR_HAS_XWAYLAND
    {
      auto* xfv = dynamic_cast<XwaylandSurface*>(output.workspace->fullscreen_view);
      auto* xv = dynamic_cast<XwaylandSurface*>(&view);
      if (xfv && xv) {
        // Special case: accept damage from children
        auto* xsurface = xv->xwayland_surface;
        while (xsurface != nullptr) {
          if (xfv->xwayland_surface == xsurface) {
            return true;
          }
          xsurface = xsurface->parent;
        }
      }
    }
#endif
    return false;
  }

  auto RenderContext::damage_whole() -> void
  {
    wlr_output_damage_add_whole(damage);
  }

  static void damage_whole_surface(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& [context, data] = *(ContextAndData*) _data;
    float rotation = data.layout.rotation;

    double lx, ly;
    get_layout_position(data.layout, lx, ly, *surface, sx, sy);

    if (!wlr_surface_has_buffer(surface)) {
      return;
    }

    int ow, oh;
    wlr_output_transformed_resolution(&context.output.wlr_output, &ow, &oh);

    wlr::box_t box;
    bool intersects = surface_intersect_output(*surface, *context.output.desktop.layout,
                                               context.output.wlr_output, lx, ly, rotation, &box);
    if (!intersects) {
      return;
    }

    wlr_box_rotated_bounds(&box, rotation, &box);

    wlr_output_damage_add_box(context.damage, &box);
  }

  auto RenderContext::damage_whole_local_surface(wlr::surface_t& surface,
                                                 double ox,
                                                 double oy,
                                                 float rotation) -> void
  {
    wlr::output_layout_output_t* layout =
      wlr_output_layout_get(output.desktop.layout, &output.wlr_output);
    RenderData data = {.layout = {
                         .x = ox + layout->x,
                         .y = oy + layout->y,
                       }};
    for_each_surface(surface, damage_whole_surface, data);
  }

  auto RenderContext::damage_whole_decoration(View& view) -> void
  {
    if (!view.decorated || view.wlr_surface == nullptr) {
      return;
    }

    wlr::box_t box = get_decoration_box(view, output);

    wlr_box_rotated_bounds(&box, view.rotation, &box);

    wlr_output_damage_add_box(damage, &box);
  }

  auto RenderContext::damage_whole_view(View& view) -> void
  {
    if (!view_accept_damage(output, view)) {
      return;
    }

    damage_whole_decoration(view);

    for_each_surface(view, damage_whole_surface, {});
  }

  auto RenderContext::damage_whole_drag_icon(DragIcon& icon) -> void
  {
    RenderData data{.layout = {.x = icon.x, .y = icon.y}};
    for_each_surface(*icon.wlr_drag_icon.surface, damage_whole_surface, data);
  }

  static void damage_from_surface(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& [context, data] = *(ContextAndData*) _data;
    wlr::output_t& wlr_output = context.output.wlr_output;
    float rotation = data.layout.rotation;

    double lx, ly;
    get_layout_position(data.layout, lx, ly, *surface, sx, sy);

    if (!wlr_surface_has_buffer(surface)) {
      return;
    }

    int ow, oh;
    wlr_output_transformed_resolution(&wlr_output, &ow, &oh);

    wlr::box_t box;
    surface_intersect_output(*surface, *context.output.desktop.layout, wlr_output, lx, ly, rotation,
                             &box);

    int center_x = box.x + box.width / 2;
    int center_y = box.y + box.height / 2;

    enum wl_output_transform transform = wlr_output_transform_invert(surface->current.transform);

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_copy(&damage, &surface->buffer_damage);
    wlr_region_transform(&damage, &damage, transform, surface->current.buffer_width,
                         surface->current.buffer_height);
    wlr_region_scale(&damage, &damage, wlr_output.scale / (float) surface->current.scale);
    if (std::ceil(wlr_output.scale) > surface->current.scale) {
      // When scaling up a surface, it'll become blurry so we need to
      // expand the damage region
      wlr_region_expand(&damage, &damage, std::ceil(wlr_output.scale) - surface->current.scale);
    }
    pixman_region32_translate(&damage, box.x, box.y);
    wlr_region_rotated_bounds(&damage, &damage, rotation, center_x, center_y);
    wlr_output_damage_add(context.damage, &damage);
    pixman_region32_fini(&damage);
  }

  auto RenderContext::damage_from_local_surface(wlr::surface_t& surface,
                                                double ox,
                                                double oy,
                                                float rotation) -> void
  {
    wlr::output_layout_output_t* layout =
      wlr_output_layout_get(output.desktop.layout, &output.wlr_output);
    RenderData data = {.layout = {.x = ox + layout->x, .y = oy + layout->y}};
    for_each_surface(surface, damage_from_surface, data);
  }

  auto RenderContext::damage_from_view(View& view) -> void
  {
    if (!view_accept_damage(output, view)) {
      return;
    }

    for_each_surface(view, damage_from_surface, {});
  }



  //////////////////////////////////////////
  // RenderContext for_each wrappers
  //////////////////////////////////////////


  auto RenderContext::for_each_surface(wlr::surface_t& surface,
                                       wlr_surface_iterator_func_t iterator,
                                       const RenderData& data) -> void
  {
    ContextAndData cd = {*this, data};
    wlr_surface_for_each_surface(&surface, iterator, &cd);
  }

  auto RenderContext::for_each_surface(View& view,
                                       wlr_surface_iterator_func_t iterator,
                                       const RenderData& data) -> void
  {
    ContextAndData cd = {*this, data};
    if (auto* xdg_surface_v6 = dynamic_cast<XdgSurfaceV6*>(&view); xdg_surface_v6) {
      wlr_xdg_surface_v6_for_each_surface(xdg_surface_v6->xdg_surface, iterator, &cd);
    } else if (auto* xdg_surface = dynamic_cast<XdgSurface*>(&view); xdg_surface) {
      wlr_xdg_surface_for_each_surface(xdg_surface->xdg_surface, iterator, &cd);
    } else if (auto* wl_shell_surface = dynamic_cast<WlShellSurface*>(&view); wl_shell_surface) {
      wlr_wl_shell_surface_for_each_surface(wl_shell_surface->wl_shell_surface, iterator, &cd);
    } else
#ifdef WLR_HAS_XWAYLAND
      if (auto* wlr_surface = dynamic_cast<XwaylandSurface*>(&view); wlr_surface) {
      wlr_surface_for_each_surface(wlr_surface->xwayland_surface->surface, iterator, &cd);
    }
#endif
  }

#ifdef WLR_HAS_XWAYLAND

  auto RenderContext::for_each_surface(wlr::xwayland_surface_t& surface,
                                       wlr_surface_iterator_func_t iterator,
                                       RenderData data) -> void
  {
    wlr::xwayland_surface_t* child;
    wl_list_for_each(child, &surface.children, parent_link)
    {
      // TODO: make relative to parent size and position
      data.layout.x = child->x;
      data.layout.y = child->y;
      data.layout.width = child->surface->current.width;
      data.layout.height = child->surface->current.height;
      if (child->mapped) {
        for_each_surface(*child->surface, iterator, data);
      }
      for_each_surface(*child, iterator, data);
    }
  }
#endif

  auto RenderContext::for_each_drag_icon(Input& input,
                                         wlr_surface_iterator_func_t iterator,
                                         RenderData data) -> void
  {
    for (auto& seat : input.seats) {
      for (auto& drag_icon : seat.drag_icons) {
        if (drag_icon.wlr_drag_icon.mapped) {
          data.layout.x = drag_icon.x;
          data.layout.y = drag_icon.y;
          data.layout.width = drag_icon.wlr_drag_icon.surface->current.width;
          data.layout.height = drag_icon.wlr_drag_icon.surface->current.height;
          for_each_surface(*drag_icon.wlr_drag_icon.surface, iterator, data);
        }
      }
    }
  }


} // namespace cloth
