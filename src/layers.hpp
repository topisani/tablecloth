#pragma once

#include "wlroots.hpp"

namespace cloth {

  struct LayerSurface {
    wlr::layer_surface_t* layer_surface;

    wlr::Listener destroy;
    wlr::Listener map;
    wlr::Listener unmap;
    wlr::Listener surface_commit;
    wlr::Listener output_destroy;
    wlr::Listener new_popup;

    bool configured;
    wlr::box_t geo;
  };

  struct LayerPopup {
    wlr::xdg_popup_v6_t* wlr_popup;
    LayerSurface& parent;

    wlr::Listener map;
    wlr::Listener unmap;
    wlr::Listener destroy;
    wlr::Listener commit;
  };

  struct Output;
  void arrange_layers(Output& output);

} // namespace cloth
