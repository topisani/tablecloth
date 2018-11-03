#pragma once

#include "util/ptr_vec.hpp"
#include "wlroots.hpp"

namespace cloth {
  struct Seat;
  struct LayerPopup;
  struct Output;

  struct LayerSurface {
    LayerSurface(Output& output, wlr::layer_surface_v1_t& layer_surface);

    LayerPopup& create_popup(wlr::xdg_popup_v6_t& wlr_popup);
    void update_cursors(const util::ptr_vec<Seat>& seats);

    Output& output;
    wlr::layer_surface_v1_t& layer_surface;
    bool has_shadow = false;
    float shadow_radius = 10;
    float shadow_offset = 0;

    bool configured;
    wlr::box_t geo;

    util::ptr_vec<LayerPopup> children;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_surface_commit;
    wl::Listener on_output_destroy;
    wl::Listener on_new_popup;
  };

  struct LayerPopup {
    LayerPopup(LayerSurface& parent, wlr::xdg_popup_v6_t& layer_surface);

    LayerSurface& parent;
    wlr::xdg_popup_v6_t& wlr_popup;

  protected:
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_destroy;
    wl::Listener on_commit;
    wl::Listener on_new_popup;
  };

  struct Output;
  void arrange_layers(Output& output);

} // namespace cloth

// kak: other_file=layer_shell.cpp
