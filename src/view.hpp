#pragma once

#include <cassert>

#include <optional>
#include <variant>

#include "wlroots.hpp"
#include "util/ptr_vec.hpp"

namespace cloth {

  struct View;
  struct Output;
  struct Desktop;

  struct WlShellSurface {
    View& view;

    wlr::wl_shell_surface_t* wlr_surface;

    wlr::Listener on_destroy;
    wlr::Listener on_new_popup;
    wlr::Listener on_request_move;
    wlr::Listener on_request_resize;
    wlr::Listener on_request_maximize;
    wlr::Listener on_request_fullscreen;
    wlr::Listener on_set_state;

    wlr::Listener on_surface_commit;
  };

  struct XdgSurfaceV6 {
    View& view;

    wlr::xdg_surface_v6_t* wlr_surface;

    wlr::Listener on_destroy;
    wlr::Listener on_new_popup;
    wlr::Listener on_map;
    wlr::Listener on_unmap;
    wlr::Listener on_request_move;
    wlr::Listener on_request_resize;
    wlr::Listener on_request_maximize;
    wlr::Listener on_request_fullscreen;

    wlr::Listener on_surface_commit;

    uint32_t pending_move_resize_configure_serial;
  };

  struct XdgSurface {
    View& view;

    wlr::xdg_surface_t* wlr_surface;

    wlr::Listener on_destroy;
    wlr::Listener on_new_popup;
    wlr::Listener on_map;
    wlr::Listener on_unmap;
    wlr::Listener on_request_move;
    wlr::Listener on_request_resize;
    wlr::Listener on_request_maximize;
    wlr::Listener on_request_fullscreen;

    wlr::Listener on_surface_commit;

    uint32_t pending_move_resize_configure_serial;
  };

  struct XwaylandSurface {
    View& view;

    wlr::xwayland_surface_t* wlr_surface;

    wlr::Listener on_destroy;
    wlr::Listener on_request_configure;
    wlr::Listener on_request_move;
    wlr::Listener on_request_resize;
    wlr::Listener on_request_maximize;
    wlr::Listener on_request_fullscreen;
    wlr::Listener on_map;
    wlr::Listener on_unmap;

    wlr::Listener on_surface_commit;
  };

  struct ViewChild {
    virtual ~ViewChild() noexcept;
    void finish();

    void handle_commit(void* data);
    void handle_new_subsurface(void* data);

    View& view;

    wlr::surface_t* wlr_surface;

    wlr::Listener on_commit;
    wlr::Listener on_new_subsurface;

  protected:
    ViewChild(View& view, wlr::surface_t* wlr_surface);
    friend struct View;
  };

  struct Subsurface : ViewChild {

    wlr::subsurface_t* wlr_subsurface;

    wlr::Listener on_destroy;

  protected:
    Subsurface(View& view, wlr::subsurface_t* wlr_subsurface);
  };

  struct WlShellPopup {
    ViewChild view_child;

    wlr::wl_shell_surface_t* wlr_wl_shell_surface;

    wlr::Listener on_destroy;
    wlr::Listener on_set_state;
    wlr::Listener on_new_popup;
  };

  struct XdgPopupV6 {
    ViewChild view_child;

    wlr::xdg_popup_v6_t* wlr_popup;

    wlr::Listener on_destroy;
    wlr::Listener on_map;
    wlr::Listener on_unmap;
    wlr::Listener on_new_popup;
  };

  struct XdgPopup {
    ViewChild view_child;

    wlr::xdg_popup_t* wlr_popup;

    wlr::Listener on_destroy;
    wlr::Listener on_map;
    wlr::Listener on_unmap;
    wlr::Listener on_new_popup;
  };

  enum struct ViewType {
    wl_shell,
    xdg_shell_v6,
    xdg_shell,
#ifdef WLR_HAS_XWAYLAND
    xwayland,
#endif
  };

  enum struct DecoPart {
    none = 0,
    top_border = (1 << 0),
    bottom_border = (1 << 1),
    left_border = (1 << 2),
    right_border = (1 << 3),
    titlebar = (1 << 4),
  };

  CLOTH_ENABLE_BITMASK_OPS(DecoPart);

  struct View {
    View(Desktop& desktop);
    ~View() noexcept;

    wlr::box_t get_box() const;
    void activate(bool active);
    void move(double x, double y);
    void resize(int width, int height);
    void move_resize(double x, double y, int width, int height);
    void move_resize(wlr::box_t);
    void maximize(bool maximized);
    void set_fullscreen(bool fullscreen, wlr::output_t* output);
    void rotate(float rotation);
    void cycle_alpha();
    void close();
    bool center();
    void setup();
    void teardown();

    void apply_damage();
    void damage_whole();
    void update_position(double x, double y);
    void update_size(uint32_t width, uint32_t height);
    void initial_focus();
    void map(wlr::surface_t& surface);
    void unmap();
    void arrange_maximized();

    wlr::box_t get_deco_box() const;
    DecoPart get_deco_part(double sx, double sy);

    ViewChild& create_child(wlr::surface_t*);
    Subsurface& create_subsurface(wlr::subsurface_t& wlr_subsurface);
    bool at(double lx, double ly, wlr::surface_t*& surface, double& sx, double& sy);

    ViewType type() noexcept;

    template<ViewType _VT>
    auto& get_surface(); 

  private:
    void update_output(std::optional<wlr::box_t> before = std::nullopt) const;
    wlr::output_t* get_output();
    void child_handle_commit(void* data);
    void child_handle_new_subsurface(void* data);
    void handle_new_subsurface(void* data);

  public:
    Desktop& desktop;

    double x, y;
    uint32_t width, height;
    float rotation;
    float alpha = 1;

    bool decorated;
    int border_width;
    int titlebar_height;

    bool maximized;

    Output* fullscreen_output;

    struct : wlr::box_t {
      float rotation;
    } saved;

    struct {
      bool update_x, update_y;
      double x, y;
      uint32_t width, height;
    } pending_move_resize;

    // TODO: Something for roots-enforced width/height
    std::variant<WlShellSurface,
                 XdgSurfaceV6,
                 XdgSurface
#ifdef WLR_HAS_XWAYLAND
                 ,
                 XwaylandSurface
#endif
                 >
      surface;

    wlr::surface_t* wlr_surface;
    util::ptr_vec<ViewChild> children;

    wlr::Listener on_new_subsurface;

    struct {
      wl::Signal unmap;
      wl::Signal destroy;
    } events;

    // TODO: this should follow the typical type/impl pattern we use elsewhere
    struct {
      void (*activate)(View& view, bool active);
      void (*move)(View& view, double x, double y);
      void (*resize)(View& view, uint32_t width, uint32_t height);
      void (*move_resize)(View& view, double x, double y, uint32_t width, uint32_t height);
      void (*maximize)(View& view, bool maximized);
      void (*set_fullscreen)(View& view, bool fullscreen);
      void (*close)(View& view);
      void (*destroy)(View& view);
    } vtbl;
  };

  template<ViewType _VT>
  auto& View::get_surface()
  {
    if constexpr (_VT == ViewType::wl_shell) {
      assert(std::holds_alternative<WlShellSurface>(surface));
      return std::get<WlShellSurface>(surface);
    }
    if constexpr (_VT == ViewType::xdg_shell) {
      assert(std::holds_alternative<XdgSurface>(surface));
      return std::get<XdgSurface>(surface);
    }
    if constexpr (_VT == ViewType::xdg_shell_v6) {
      assert(std::holds_alternative<XdgSurfaceV6>(surface));
      return std::get<XdgSurfaceV6>(surface);
    }
    #ifdef WLR_HAS_XWAYLAND
    if constexpr (_VT == ViewType::xwayland) {
      assert(std::holds_alternative<XwaylandSurface>(surface));
      return std::get<XwaylandSurface>(surface);
    }
    #endif
  }
} // namespace cloth
