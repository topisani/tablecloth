#pragma once

#include "util/ptr_vec.hpp"

#include "cursor.hpp"
#include "keyboard.hpp"
#include "util/bindings.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct Input;
  struct View;

  struct DragIcon {
    DragIcon(Seat&, wlr::drag_icon_t&) noexcept;
    ~DragIcon() noexcept = default;
    void update_position();
    void damage_whole();

    Seat& seat;
    wlr::drag_icon_t& wlr_drag_icon;

    double x, y;

    wl::Listener on_surface_commit;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_destroy;
  };

  struct Pointer {
    Pointer(Seat&, wlr::input_device_t&) noexcept;
    ~Pointer() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;
  };

  struct Touch {
    Touch(Seat&, wlr::input_device_t&) noexcept;
    ~Touch() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;
  };

  struct TabletTool {
    TabletTool(Seat&, wlr::input_device_t&) noexcept;
    ~TabletTool() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;

    wl::Listener on_axis;
    wl::Listener on_proximity;
    wl::Listener on_tip;
    wl::Listener on_button;
  };

  struct SeatView {
    SeatView(Seat&, View&) noexcept;
    ~SeatView() noexcept;

    void deco_motion(double deco_sx, double deco_sy);
    void deco_leave();
    void deco_button(double sx, double sy, wlr::Button button, wlr::button_state_t state);
    Seat& seat;
    View& view;

    bool has_button_grab;
    double grab_sx;
    double grab_sy;

    wl::Listener on_view_unmap;
    wl::Listener on_view_destroy;
  };

  struct Seat {
    Seat(Input& input, const std::string& name);
    ~Seat();

    void add_device(wlr::input_device_t& device) noexcept;
    void update_capabilities() noexcept;
    void configure_cursor();
    void configure_xcursor();
    bool has_meta_pressed();
    View* get_focus();
    void set_focus(View* view);
    void set_focus_layer(wlr::layer_surface_t* layer);
    void cycle_focus();
    void begin_move(View& view);
    void begin_resize(View& view, wlr::edges_t edges);
    void begin_rotate(View& view);
    void end_compositor_grab();

    void set_exclusive_client(wl::client_t* client);
    bool allow_input(wl::resource_t& resource);

    SeatView& seat_view_from_view(View& view);

    wlr::seat_t* wlr_seat = nullptr;
    Input& input;
    Cursor cursor;

    // coordinates of the first touch point if it exists
    int touch_id;
    double touch_x, touch_y;

    // If the focused layer is set, views cannot receive keyboard focus
    wlr::layer_surface_t* focused_layer = nullptr;

    // If non-null, only this client can receive input events
    wl::client_t* exclusive_client = nullptr;

    util::ptr_vec<SeatView> views;
    bool has_focus;

    util::ptr_vec<DragIcon> drag_icons; // roots_drag_icon::link

    util::ptr_vec<Keyboard> keyboards;
    util::ptr_vec<Pointer> pointers;
    util::ptr_vec<Touch> touch;
    util::ptr_vec<TabletTool> tablet_tools;

    wl::Listener on_new_drag_icon;
    wl::Listener on_destroy;


    void reset_device_mappings(wlr::input_device_t& device) noexcept;
    void set_device_output_mappings(wlr::input_device_t& device, wlr::output_t* output) noexcept;

    void init_cursor();

    void handle_new_drag_icon(void* data);

    void add_keyboard(wlr::input_device_t& device);
    void add_pointer(wlr::input_device_t& device);
    void add_touch(wlr::input_device_t& device);
    void add_tablet_pad(wlr::input_device_t& device);
    void add_tablet_tool(wlr::input_device_t& device);
    SeatView& add_view(View& view);
  };

} // namespace cloth
