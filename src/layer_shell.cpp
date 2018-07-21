#include "layers.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "util/exception.hpp"
#include "wlroots.hpp"

#include "desktop.hpp"
#include "output.hpp"
#include "server.hpp"
#include "seat.hpp"

namespace cloth {

  LayerPopup::LayerPopup(LayerSurface& p_parent, wlr::xdg_popup_v6_t& p_wlr_popup)
    : parent(p_parent), wlr_popup(p_wlr_popup)
  {
    on_destroy.add_to(wlr_popup.base->events.destroy);
    on_destroy = [this] { util::erase_this(parent.children, this); };
    on_new_popup.add_to(wlr_popup.base->events.new_popup);
    on_new_popup = [this](void* data) { parent.create_popup(*((wlr::xdg_popup_v6_t*) data)); };

    auto damage_whole = [this] {
      int ox = wlr_popup.geometry.x + parent.geo.x;
      int oy = wlr_popup.geometry.y + parent.geo.y;
      parent.output.damage_whole_local_surface(*wlr_popup.base->surface, ox, oy, 0);
    };

    on_unmap.add_to(wlr_popup.base->events.unmap);
    on_unmap = damage_whole;
    on_map.add_to(wlr_popup.base->events.map);
    on_map = damage_whole;
    on_commit.add_to(wlr_popup.base->surface->events.commit);
    on_commit = damage_whole;

    // TODO: Desired behaviour?
  }


  static void apply_exclusive(wlr::box_t& usable_area,
                              uint32_t anchor,
                              int32_t exclusive,
                              int32_t margin_top,
                              int32_t margin_right,
                              int32_t margin_bottom,
                              int32_t margin_left)
  {
    if (exclusive <= 0) {
      return;
    }
    struct {
      uint32_t anchors;
      int* positive_axis;
      int* negative_axis;
      int margin;
    } edges[] = {
      {
        .anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
        .positive_axis = &usable_area.y,
        .negative_axis = &usable_area.height,
        .margin = margin_top,
      },
      {
        .anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
        .positive_axis = nullptr,
        .negative_axis = &usable_area.height,
        .margin = margin_bottom,
      },
      {
        .anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
        .positive_axis = &usable_area.x,
        .negative_axis = &usable_area.width,
        .margin = margin_left,
      },
      {
        .anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
        .positive_axis = nullptr,
        .negative_axis = &usable_area.width,
        .margin = margin_right,
      },
    };
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
      if ((anchor & edges[i].anchors) == edges[i].anchors) {
        if (edges[i].positive_axis) {
          *edges[i].positive_axis += exclusive + edges[i].margin;
        }
        if (edges[i].negative_axis) {
          *edges[i].negative_axis -= exclusive + edges[i].margin;
        }
      }
    }
  }

  static void arrange_layer(wlr::output_t& output,
                            util::ptr_vec<LayerSurface>& list,
                            wlr::box_t& usable_area,
                            bool exclusive)
  {
    wlr::box_t full_area = {0};
    wlr_output_effective_resolution(&output, &full_area.width, &full_area.height);
    for (auto& surface : list) {
      wlr::layer_surface_t& layer = surface.layer_surface;
      wlr::layer_surface_state_t& state = layer.current;
      if (exclusive != (state.exclusive_zone > 0)) {
        continue;
      }
      wlr::box_t bounds;
      if (state.exclusive_zone == -1) {
        bounds = full_area;
      } else {
        bounds = usable_area;
      }
      wlr::box_t box = {.width = (int) state.desired_width, .height = (int) state.desired_height};
      // Horizontal axis
      const uint32_t both_horiz =
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
      if ((state.anchor & both_horiz) && box.width == 0) {
        box.x = bounds.x;
        box.width = bounds.width;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
        box.x = bounds.x;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
        box.x = bounds.x + (bounds.width - box.width);
      } else {
        box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));
      }
      // Vertical axis
      const uint32_t both_vert =
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
      if ((state.anchor & both_vert) && box.height == 0) {
        box.y = bounds.y;
        box.height = bounds.height;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
        box.y = bounds.y;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
        box.y = bounds.y + (bounds.height - box.height);
      } else {
        box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));
      }
      // Margin
      if ((state.anchor & both_horiz) == both_horiz) {
        box.x += state.margin.left;
        box.width -= state.margin.left + state.margin.right;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
        box.x += state.margin.left;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
        box.x -= state.margin.right;
      }
      if ((state.anchor & both_vert) == both_vert) {
        box.y += state.margin.top;
        box.height -= state.margin.top + state.margin.bottom;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
        box.y += state.margin.top;
      } else if ((state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
        box.y -= state.margin.bottom;
      }
      if (box.width < 0 || box.height < 0) {
        // TODO: Bubble up a protocol error?
        wlr_layer_surface_close(&layer);
        continue;
      }
      // Apply
      surface.geo = box;
      apply_exclusive(usable_area, state.anchor, state.exclusive_zone, state.margin.top,
                      state.margin.right, state.margin.bottom, state.margin.left);
      wlr_layer_surface_configure(&layer, box.width, box.height);
    }
  }

  void arrange_layers(Output& output)
  {
    wlr::box_t usable_area = {0};
    wlr_output_effective_resolution(&output.wlr_output, &usable_area.width, &usable_area.height);

    // Arrange exclusive surfaces from top->bottom
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], usable_area,
                  true);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], usable_area,
                  true);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], usable_area,
                  true);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
                  usable_area, true);
    memcpy(&output.usable_area, &usable_area, sizeof(wlr::box_t));

    for (auto& view : output.desktop.views) {
      if (view.maximized) view.arrange_maximized();
    }

    // Arrange non-exlusive surfaces from top->bottom
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], usable_area,
                  false);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], usable_area,
                  false);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], usable_area,
                  false);
    arrange_layer(output.wlr_output, output.layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
                  usable_area, false);

    // Find topmost keyboard interactive layer, if such a layer exists
    uint32_t layers_above_shell[] = {
      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
      ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };
    size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
    LayerSurface* topmost = nullptr;
    for (size_t i = 0; i < nlayers; ++i) {
      for (auto& layer : util::view::reverse(output.layers[layers_above_shell[i]])) {
        if (layer.layer_surface.current.keyboard_interactive) {
          topmost = &layer;
          break;
        }
      }
      if (topmost != nullptr) {
        break;
      }
    }

    for (auto& seat : output.desktop.server.input.seats) {
      seat.set_focus_layer(topmost ? &topmost->layer_surface : nullptr);
    }
  }

  void Desktop::handle_layer_shell_surface(void* data)
  {
    auto& layer_surface = *(wlr::layer_surface_t*) data;

    LOGD("new layer surface: namespace {} layer {} anchor {} size {}x{} margin {},{},{},{}",
         layer_surface.namespace_, layer_surface.layer, layer_surface.layer,
         layer_surface.client_pending.desired_width, layer_surface.client_pending.desired_height,
         layer_surface.client_pending.margin.top, layer_surface.client_pending.margin.right,
         layer_surface.client_pending.margin.bottom, layer_surface.client_pending.margin.left);

    if (!layer_surface.output) {
      Seat* seat = server.input.last_active_seat();
      assert(seat); // TODO: Technically speaking we should handle this case
      wlr::output_t* output = wlr_output_layout_output_at(layout, seat->cursor.wlr_cursor->x,
                                                          seat->cursor.wlr_cursor->y);
      if (!output) {
        LOGE("Couldn't find output at (%.0f,%.0f)", seat->cursor.wlr_cursor->x,
                seat->cursor.wlr_cursor->y);
        output = wlr_output_layout_get_center_output(layout);
      }
      if (output) {
        layer_surface.output = output;
      } else {
        wlr_layer_surface_close(&layer_surface);
        return;
      }
    }

    auto* output = (Output*) layer_surface.data;
    if (!output)  throw util::exception("layer_surface.data not set");

    [[maybe_unused]]
    auto& layer = output->layers[layer_surface.layer].emplace_back(*output, layer_surface);

    // Temporarily set the layer's current state to client_pending
    // So that we can easily arrange it
    wlr::layer_surface_state_t old_state = layer_surface.current;
    layer_surface.current = layer_surface.client_pending;

    arrange_layers(*output);

    layer_surface.current = old_state;
  }

  LayerPopup& LayerSurface::create_popup(wlr::xdg_popup_v6_t& wlr_popup) {
    auto popup = std::make_unique<LayerPopup>(*this, wlr_popup);
    return children.push_back(std::move(popup));
  }

  LayerSurface::LayerSurface(Output& p_output, wlr::layer_surface_t& p_layer_surface)
    : output(p_output), layer_surface(p_layer_surface)
  {
    layer_surface.data = this;

    on_surface_commit.add_to(layer_surface.surface->events.commit);
    on_surface_commit = [this] (void* data) {
      wlr::box_t old_geo = geo;
      arrange_layers(output);
      if (old_geo == geo) {
        output.damage_whole_local_surface(*layer_surface.surface, old_geo.x, old_geo.y);
        output.damage_whole_local_surface(*layer_surface.surface, geo.x, geo.y);
      } else {
        output.damage_from_local_surface(*layer_surface.surface, geo.x, geo.y);
      }
    };

    on_output_destroy.add_to(layer_surface.output->events.destroy);
    on_output_destroy = [this] (void* data) {
      layer_surface.output = nullptr;
      on_output_destroy.remove();
      wlr_layer_surface_close(&layer_surface);
    };

    on_destroy.add_to(layer_surface.events.destroy);
    on_destroy = [this] (void* data) {
      if (layer_surface.mapped) {
        output.damage_whole_local_surface(*layer_surface.surface, geo.x, geo.y);
      }
      util::erase_this(output.layers[layer_surface.layer], this);
      arrange_layers(output);
    };
    on_map.add_to(layer_surface.events.map);
    on_map = [this] (void* data) {
      output.damage_whole_local_surface(*layer_surface.surface, geo.x, geo.y);
      wlr_surface_send_enter(layer_surface.surface, &output.wlr_output);
    };
    on_unmap.add_to(layer_surface.events.unmap);
    on_unmap = [this] (void* data) {
       output.damage_whole_local_surface(*layer_surface.surface, geo.x, geo.y);
    };
    on_new_popup.add_to(layer_surface.events.new_popup);
    on_new_popup = [this] (void* data) {
      auto& wlr_popup = *(wlr::xdg_popup_v6_t*) data;
      create_popup(wlr_popup);
    };
    // TODO: Listen for subsurfaces

  }

} // namespace cloth

// kak: other_file=layers.hpp
