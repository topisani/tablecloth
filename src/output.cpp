#include "output.hpp"

#include "wlroots.hpp"

#include "util/logging.hpp"

#include "config.hpp"
#include "layers.hpp"
#include "output.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "view.hpp"

namespace cloth {

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

  struct LayoutData {
    double x, y;
    int width, height;
    float rotation;
  };

  static void get_layout_position(LayoutData& data,
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

  static void surface_for_each_surface(wlr::surface_t& surface,
                                       double lx,
                                       double ly,
                                       float rotation,
                                       LayoutData& layout_data,
                                       wlr_surface_iterator_func_t iterator,
                                       void* user_data)
  {
    layout_data.x = lx;
    layout_data.y = ly;
    layout_data.width = surface.current.width;
    layout_data.height = surface.current.height;
    layout_data.rotation = rotation;

    wlr_surface_for_each_surface(&surface, iterator, user_data);
  }

  static void view_for_each_surface(View& view,
                                    LayoutData& layout_data,
                                    wlr_surface_iterator_func_t iterator,
                                    void* user_data)
  {
    layout_data.x = view.x;
    layout_data.y = view.y;
    layout_data.width = view.wlr_surface->current.width;
    layout_data.height = view.wlr_surface->current.height;
    layout_data.rotation = view.rotation;

    if (auto* xdg_surface_v6 = dynamic_cast<XdgSurfaceV6*>(&view); xdg_surface_v6) {
      wlr_xdg_surface_v6_for_each_surface(xdg_surface_v6->xdg_surface, iterator, user_data);
    } else if (auto* xdg_surface = dynamic_cast<XdgSurface*>(&view); xdg_surface) {
      wlr_xdg_surface_for_each_surface(xdg_surface->xdg_surface, iterator, user_data);
    } else if (auto* wl_shell_surface = dynamic_cast<WlShellSurface*>(&view); wl_shell_surface) {
      wlr_wl_shell_surface_for_each_surface(wl_shell_surface->wl_shell_surface, iterator, user_data);
    } else
#ifdef WLR_HAS_XWAYLAND
      if (auto* wlr_surface = dynamic_cast<XwaylandSurface*>(&view); wlr_surface) {
      wlr_surface_for_each_surface(wlr_surface->xwayland_surface->surface, iterator, user_data);
    }
#endif
  }

#ifdef WLR_HAS_XWAYLAND
  static void xwayland_children_for_each_surface(wlr::xwayland_surface_t& surface,
                                                 wlr_surface_iterator_func_t iterator,
                                                 LayoutData& layout_data,
                                                 void* user_data)
  {
    wlr::xwayland_surface_t* child;
    wl_list_for_each(child, &surface.children, parent_link)
    {
      if (child->mapped) {
        surface_for_each_surface(*child->surface, child->x, child->y, 0, layout_data, iterator,
                                 user_data);
      }
      xwayland_children_for_each_surface(*child, iterator, layout_data, user_data);
    }
  }
#endif

  static void drag_icons_for_each_surface(Input& input,
                                          wlr_surface_iterator_func_t iterator,
                                          LayoutData& layout_data,
                                          void* user_data)
  {
    for (auto& seat : input.seats) {
      for (auto& drag_icon : seat.drag_icons) {
        if (!drag_icon.wlr_drag_icon.mapped) {
          continue;
        }
        surface_for_each_surface(*drag_icon.wlr_drag_icon.surface, drag_icon.x, drag_icon.y, 0,
                                 layout_data, iterator, user_data);
      }
    }
  }

  struct RenderData {
    LayoutData layout;
    Output* output;
    chrono::time_point when;
    pixman_region32_t* damage;
    float alpha;
  };

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

  static void render_surface(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto* data = (RenderData*) _data;
    Output& output = *data->output;
    float rotation = data->layout.rotation;

    wlr::texture_t* texture = wlr_surface_get_texture(surface);
    if (texture == nullptr) {
      return;
    }

    wlr::renderer_t* renderer = wlr_backend_get_renderer(output.wlr_output.backend);
    assert(renderer);

    double lx, ly;
    get_layout_position(data->layout, lx, ly, *surface, sx, sy);

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
    pixman_region32_intersect(&damage, &damage, data->damage);
    bool damaged = pixman_region32_not_empty(&damage);
    if (damaged) {
      float matrix[9];
      auto transform = wlr_output_transform_invert(surface->current.transform);
      wlr_matrix_project_box(matrix, &box, transform, rotation, output.wlr_output.transform_matrix);

      int nrects;
      pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
      for (int i = 0; i < nrects; ++i) {
        scissor_output(output, &rects[i]);
        wlr_render_texture_with_matrix(renderer, texture, matrix, data->alpha);
      }
    }

    pixman_region32_fini(&damage);
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

  static void render_decorations(View& view, RenderData& data)
  {
    if (!view.decorated || view.wlr_surface == nullptr) {
      return;
    }

    Output* output = data.output;
    wlr::renderer_t* renderer = wlr_backend_get_renderer(output->wlr_output.backend);
    assert(renderer);

    wlr::box_t box = get_decoration_box(view, *output);

    wlr::box_t rotated;
    wlr_box_rotated_bounds(&box, view.rotation, &rotated);

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_union_rect(&damage, &damage, rotated.x, rotated.y, rotated.width,
                               rotated.height);
    pixman_region32_intersect(&damage, &damage, data.damage);
    bool damaged = pixman_region32_not_empty(&damage);
    if (damaged) {
      float matrix[9];
      wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, view.rotation,
                             output->wlr_output.transform_matrix);
      float color[] = {0.2, 0.2, 0.2, view.alpha};

      int nrects;
      pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
      for (int i = 0; i < nrects; ++i) {
        scissor_output(*output, &rects[i]);
        wlr_render_quad_with_matrix(renderer, color, matrix);
      }
    }

    pixman_region32_fini(&damage);
  }

  static void render_view(View& view, RenderData& data)
  {
    // Do not render views fullscreened on other outputs
    if (view.fullscreen_output != nullptr && view.fullscreen_output != data.output) {
      return;
    }

    data.alpha = view.alpha;
    render_decorations(view, data);
    view_for_each_surface(view, data.layout, render_surface, &data);
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

  static void surface_send_frame_done(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& data = *(RenderData*) _data;
    Output* output = data.output;
    auto when = chrono::to_timespec(data.when);
    float rotation = data.layout.rotation;

    double lx, ly;
    get_layout_position(data.layout, lx, ly, *surface, sx, sy);

    if (!surface_intersect_output(*surface, *output->desktop.layout, output->wlr_output, lx, ly,
                                  rotation, nullptr)) {
      return;
    }

    wlr_surface_send_frame_done(surface, &when);
  }

  static void render_layer(Output* output,
                           const wlr::box_t* output_layout_box,
                           RenderData& data,
                           util::ptr_vec<LayerSurface>& layer)
  {
    for (auto& layer_surface : layer) {
      surface_for_each_surface(
        *layer_surface.layer_surface.surface, layer_surface.geo.x + output_layout_box->x,
        layer_surface.geo.y + output_layout_box->y, 0, data.layout, render_surface, &data);

      wlr_layer_surface_for_each_surface(&layer_surface.layer_surface, render_surface, &data);
    }
  }

  static void layers_send_done(Output& output, chrono::time_point when)
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

  void Output::render()
  {
    wlr::renderer_t* renderer = wlr_backend_get_renderer(wlr_output.backend);
    assert(renderer);

    if (!wlr_output.enabled) {
      return;
    }

    auto now = chrono::clock::now();

    float clear_color[] = {0.25f, 0.25f, 0.25f, 1.0f};

    wlr::box_t* output_box = wlr_output_layout_get_box(desktop.layout, &wlr_output);

    // Check if we can delegate the fullscreen surface to the output
    if (fullscreen_view != nullptr && fullscreen_view->wlr_surface != nullptr) {
      View& view = *fullscreen_view;

      // Make sure the view is centered on screen
      wlr::box_t view_box = view.get_box();
      double view_x = (double) (output_box->width - view_box.width) / 2 + output_box->x;
      double view_y = (double) (output_box->height - view_box.height) / 2 + output_box->y;
      view.move(view_x, view_y);

      if (has_standalone_surface(view)) {
        wlr_output_set_fullscreen_surface(&wlr_output, view.wlr_surface);
      } else {
        wlr_output_set_fullscreen_surface(&wlr_output, nullptr);
      }

      // Fullscreen views are rendered on a black background
      clear_color[0] = clear_color[1] = clear_color[2] = 0;
    } else {
      wlr_output_set_fullscreen_surface(&wlr_output, nullptr);
    }

    bool needs_swap;
    pixman_region32_t damage;
    pixman_region32_init(&damage);
    if (!wlr_output_damage_make_current(this->damage, &needs_swap, &damage)) {
      return;
    }

    struct RenderData data = {
      .output = this,
      .when = now,
      .damage = &damage,
      .alpha = 1.0,
    };

    // otherwise Output doesn't need swap and isn't damaged, skip rendering completely
    if (needs_swap) {
      wlr_renderer_begin(renderer, wlr_output.width, wlr_output.height);

      // otherwise Output isn't damaged but needs buffer swap
      if (pixman_region32_not_empty(&damage)) {
        if (desktop.server.config.debug_damage_tracking) {
          wlr_renderer_clear(renderer, (float[]){1, 1, 0, 1});
        }

        int nrects;
        pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
        for (int i = 0; i < nrects; ++i) {
          scissor_output(*this, &rects[i]);
          wlr_renderer_clear(renderer, clear_color);
        }

        render_layer(this, output_box, data, layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
        render_layer(this, output_box, data, layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

        // If a view is fullscreen on this output, render it
        if (fullscreen_view != nullptr) {
          View& view = *fullscreen_view;

          if (wlr_output.fullscreen_surface == view.wlr_surface) {
            // The output will render the fullscreen view
            goto renderer_end;
          }

          if (view.wlr_surface != nullptr) {
            view_for_each_surface(view, data.layout, render_surface, &data);
          }

          // During normal rendering the xwayland window tree isn't traversed
          // because all windows are rendered. Here we only want to render
          // the fullscreen window's children so we have to traverse the tree.
#ifdef WLR_HAS_XWAYLAND
          if (auto* xwayland_surface = dynamic_cast<XwaylandSurface*>(&view); xwayland_surface) {
            xwayland_children_for_each_surface(*xwayland_surface->xwayland_surface, render_surface,
                                               data.layout, &data);
          }
#endif
        } else {
          // Render all views
          for (auto& view : desktop.views) {
            render_view(view, data);
          }
          // Render top layer above shell views
          render_layer(this, output_box, data, layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
        }

        // Render drag icons
        data.alpha = 1.0;
        drag_icons_for_each_surface(desktop.server.input, render_surface, data.layout, &data);

        render_layer(this, output_box, data, layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
      }

    renderer_end:
      wlr_renderer_scissor(renderer, nullptr);
      wlr_renderer_end(renderer);

      if (desktop.server.config.debug_damage_tracking) {
        int width, height;
        wlr_output_transformed_resolution(&wlr_output, &width, &height);
        pixman_region32_union_rect(&damage, &damage, 0, 0, width, height);
      }

      // PREV: update now?
      struct timespec now_ts = chrono::to_timespec(now);
      if (wlr_output_damage_swap_buffers(this->damage, &now_ts, &damage)) {
        last_frame = desktop.last_frame = now;
      }
    }

    pixman_region32_fini(&damage);

    // Send frame done events to all surfaces
    if (fullscreen_view != nullptr) {
      View& view = *fullscreen_view;
      if (wlr_output.fullscreen_surface == view.wlr_surface) {
        // The surface is managed by the wlr_output
        return;
      }

      view_for_each_surface(view, data.layout, surface_send_frame_done, &data);

#ifdef WLR_HAS_XWAYLAND
      if (auto* xwayland_surface = dynamic_cast<XwaylandSurface*>(&view); xwayland_surface) {
        xwayland_children_for_each_surface(*xwayland_surface->xwayland_surface, surface_send_frame_done,
                                           data.layout, &data);
      }
#endif
    } else {
      for (auto& view : desktop.views) {
        view_for_each_surface(view, data.layout, surface_send_frame_done, &data);
      }

      drag_icons_for_each_surface(desktop.server.input, surface_send_frame_done, data.layout,
                                  &data);
    }
    layers_send_done(*this, data.when);
  }

  void Output::damage_whole()
  {
    wlr_output_damage_add_whole(damage);
  }

  static bool view_accept_damage(Output& output, View& view)
  {
    if (view.wlr_surface == nullptr) {
      return false;
    }
    if (output.fullscreen_view == nullptr) {
      return true;
    }
    if (output.fullscreen_view == &view) {
      return true;
    }
#ifdef WLR_HAS_XWAYLAND
    {
      auto* xfv = dynamic_cast<XwaylandSurface*>(output.fullscreen_view);
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

  struct DamageData {
    LayoutData layout;
    Output* output;
  };

  static void damage_whole_surface(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& data = *(DamageData*) _data;
    Output* output = data.output;
    float rotation = data.layout.rotation;

    double lx, ly;
    get_layout_position(data.layout, lx, ly, *surface, sx, sy);

    if (!wlr_surface_has_buffer(surface)) {
      return;
    }

    int ow, oh;
    wlr_output_transformed_resolution(&output->wlr_output, &ow, &oh);

    wlr::box_t box;
    bool intersects = surface_intersect_output(*surface, *output->desktop.layout,
                                               output->wlr_output, lx, ly, rotation, &box);
    if (!intersects) {
      return;
    }

    wlr_box_rotated_bounds(&box, rotation, &box);

    wlr_output_damage_add_box(output->damage, &box);
  }

  void Output::damage_whole_local_surface(wlr::surface_t& surface,
                                          double ox,
                                          double oy,
                                          float rotation)
  {
    wlr::output_layout_output_t* layout = wlr_output_layout_get(desktop.layout, &wlr_output);
    DamageData data = {.output = this};
    surface_for_each_surface(surface, ox + layout->x, oy + layout->y, 0, data.layout,
                             damage_whole_surface, &data);
  }

  static void damage_whole_decoration(View& view, Output& output)
  {
    if (!view.decorated || view.wlr_surface == nullptr) {
      return;
    }

    wlr::box_t box = get_decoration_box(view, output);

    wlr_box_rotated_bounds(&box, view.rotation, &box);

    wlr_output_damage_add_box(output.damage, &box);
  }

  void Output::damage_whole_view(View& view)
  {
    if (!view_accept_damage(*this, view)) {
      return;
    }

    damage_whole_decoration(view, *this);

    DamageData data = {.output = this};
    view_for_each_surface(view, data.layout, damage_whole_surface, &data);
  }

  void Output::damage_whole_drag_icon(DragIcon& icon)
  {
    DamageData data = {.output = this};
    surface_for_each_surface(*icon.wlr_drag_icon.surface, icon.x, icon.y, 0, data.layout,
                             damage_whole_surface, &data);
  }

  static void damage_from_surface(wlr::surface_t* surface, int sx, int sy, void* _data)
  {
    auto& data = *(DamageData*) _data;
    Output* output = data.output;
    wlr::output_t& wlr_output = output->wlr_output;
    float rotation = data.layout.rotation;

    double lx, ly;
    get_layout_position(data.layout, lx, ly, *surface, sx, sy);

    if (!wlr_surface_has_buffer(surface)) {
      return;
    }

    int ow, oh;
    wlr_output_transformed_resolution(&wlr_output, &ow, &oh);

    wlr::box_t box;
    surface_intersect_output(*surface, *output->desktop.layout, wlr_output, lx, ly, rotation, &box);

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
    wlr_output_damage_add(output->damage, &damage);
    pixman_region32_fini(&damage);
  }

  void Output::damage_from_local_surface(wlr::surface_t& surface,
                                         double ox,
                                         double oy,
                                         float rotation)
  {
    wlr::output_layout_output_t* layout = wlr_output_layout_get(desktop.layout, &wlr_output);
    DamageData data = {.output = this};
    surface_for_each_surface(surface, ox + layout->x, oy + layout->y, 0, data.layout,
                             damage_from_surface, &data);
  }

  void Output::damage_from_view(View& view)
  {
    if (!view_accept_damage(*this, view)) {
      return;
    }

    DamageData data = {.output = this};
    view_for_each_surface(view, data.layout, damage_from_surface, &data);
  }

  static void set_mode(wlr::output_t& output, Config::Output& oc)
  {
    int mhz = (int) (oc.mode.refresh_rate * 1000);

    if (wl_list_empty(&output.modes)) {
      // Output has no mode, try setting a custom one
      wlr_output_set_custom_mode(&output, oc.mode.width, oc.mode.height, mhz);
      return;
    }

    wlr::output_mode_t *mode, *best = nullptr;
    wl_list_for_each(mode, &output.modes, link)
    {
      if (mode->width == oc.mode.width && mode->height == oc.mode.height) {
        if (mode->refresh == mhz) {
          best = mode;
          break;
        }
        best = mode;
      }
    }
    if (!best) {
      LOGE("Configured mode for {} not available", output.name);
    } else {
      LOGD("Assigning configured mode to {}", output.name);
      wlr_output_set_mode(&output, best);
    }
  }

  Output::Output(Desktop& desktop, wlr::output_t& wlr) noexcept
    : desktop(desktop),
      wlr_output(wlr),
      last_frame(chrono::clock::now()),
      damage(wlr_output_damage_create(&wlr_output))
  {
    wlr_output.data = this;

    LOGD("Output '{}' added", wlr_output.name);
    LOGD("'{} {} {}' {}mm x {}mm", wlr_output.make, wlr_output.model, wlr_output.serial,
         wlr_output.phys_width, wlr_output.phys_height);

    if (!wl_list_empty(&wlr_output.modes)) {
      wlr::output_mode_t* mode = wl_container_of((&wlr_output.modes)->prev, mode, link);
      wlr_output_set_mode(&wlr_output, mode);
    }

    on_destroy.add_to(wlr_output.events.destroy);
    on_destroy = [&] { util::erase_this(desktop.outputs, this); };

    on_mode.add_to(wlr_output.events.mode);
    on_mode = [&] { arrange_layers(*this); };

    on_transform.add_to(wlr_output.events.transform);
    on_transform = [&] { arrange_layers(*this); };

    on_damage_frame.add_to(damage->events.frame);
    on_damage_frame = [&] { render(); };

    on_damage_destroy.add_to(damage->events.destroy);
    on_damage_destroy = [&] { util::erase_this(desktop.outputs, this); };

    Config::Output* output_config = desktop.config.get_output(wlr_output);
    if (output_config) {
      if (output_config->enable) {
        if (wlr_output_is_drm(&wlr_output)) {
          for (auto& mode : output_config->modes) {
            wlr_drm_connector_add_mode(&wlr_output, &mode.info);
          }
        } else {
          if (output_config->modes.empty()) {
            LOGE("Can only add modes for DRM backend");
          }
        }
        if (output_config->mode.width) {
          set_mode(wlr_output, *output_config);
        }
        wlr_output_set_scale(&wlr_output, output_config->scale);
        wlr_output_set_transform(&wlr_output, output_config->transform);
        wlr_output_layout_add(desktop.layout, &wlr_output, output_config->x, output_config->y);
      } else {
        wlr_output_enable(&wlr_output, false);
      }
    } else {
      wlr_output_layout_add_auto(desktop.layout, &wlr_output);
    }

    for (auto& seat : desktop.server.input.seats)
    {
      seat.configure_cursor();
      seat.configure_xcursor();
    }

    arrange_layers(*this);
    damage_whole();
  }

} // namespace cloth
