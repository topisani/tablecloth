#pragma once

#include "util/ptr_vec.hpp"

#include "cursor.hpp"
#include "keyboard.hpp"
#include "util/bindings.hpp"
#include "view.hpp"
#include "text_input.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct Input;

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

  struct Pointer : Device {
    Pointer(Seat&, wlr::input_device_t&) noexcept;
    ~Pointer() noexcept;

    wl::Listener on_output_transform;
  };

  struct Touch : Device {
    Touch(Seat&, wlr::input_device_t&) noexcept;
    ~Touch() noexcept;

    wl::Listener on_output_transform;
  };

  /// The device that connects to the tools
  struct Tablet : Device {
    Tablet(Seat&, wlr::input_device_t&) noexcept;
    ~Tablet() noexcept;

    wlr::tablet_v2_tablet_t& tablet_v2;

    wl::Listener on_axis;
    wl::Listener on_proximity;
    wl::Listener on_tip;
    wl::Listener on_button;
  };

  /// Unrelated to the pen stuff, just handles different input on the pad
  struct TabletPad : Device {
    TabletPad(Seat& seat, wlr::tablet_v2_tablet_pad_t&) noexcept;
    ~TabletPad() noexcept;

    Tablet* tablet;

    wlr::tablet_v2_tablet_pad_t& tablet_v2_pad;

    wl::Listener on_attach;
    wl::Listener on_button;
    wl::Listener on_ring;
    wl::Listener on_strip;
    wl::Listener on_tablet_destroy;
  };

  /// Not a separate device, just added on the tablet proximity event.
  struct TabletTool {
    TabletTool(Seat& seat, wlr::tablet_v2_tablet_tool_t&) noexcept;
    ~TabletTool() noexcept;

    Seat& seat;
    double tilt_x = 0;
    double tilt_y = 0;

    wlr::tablet_v2_tablet_tool_t& tablet_v2_tool;
    Tablet* current_tablet;

    wl::Listener on_set_cursor;
    wl::Listener on_tool_destroy;
    wl::Listener on_tablet_destroy;

    bool in_fallback_mode = false;
  };

  struct PointerConstraint {
    PointerConstraint(wlr::pointer_constraint_v1_t* wlr_constraint);
    wlr::pointer_constraint_v1_t* wlr_constraint;
    wl::Listener on_destroy;
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

    Device& add_device(wlr::input_device_t& device) noexcept;
    void update_capabilities() noexcept;
    void configure_cursor();
    void configure_xcursor();
    bool has_meta_pressed();
    View* get_focus();
    void set_focus(View* view);
    void set_focus_layer(wlr::layer_surface_v1_t* layer);
    void begin_move(View& view);
    void begin_resize(View& view, wlr::edges_t edges);
    void begin_rotate(View& view);
    void end_compositor_grab();

    void set_exclusive_client(wl::client_t* client);
    bool allow_input(wl::resource_t& resource);

    SeatView& seat_view_from_view(View& view);

    Output* current_output();

    wlr::seat_t* wlr_seat = nullptr;
    Input& input;
    Cursor cursor;

    // coordinates of the first touch point if it exists
    int touch_id;
    double touch_x, touch_y;

    // If the focused layer is set, views cannot receive keyboard focus
    wlr::layer_surface_v1_t* focused_layer = nullptr;
    InputMethodRelay im_relay = {*this};

    // If non-null, only this client can receive input events
    wl::client_t* exclusive_client = nullptr;

    util::ptr_vec<SeatView> views;

    bool has_focus;

    util::ptr_vec<DragIcon> drag_icons;

    util::ptr_vec<Keyboard> keyboards;
    util::ptr_vec<Pointer> pointers;
    util::ptr_vec<Touch> touch;
    util::ptr_vec<Tablet> tablets;
    util::ptr_vec<TabletPad> tablet_pads;

    wl::Listener on_new_drag_icon;
    wl::Listener on_destroy;

    void reset_device_mappings(Device& device) noexcept;
    void set_device_output_mappings(Device& device, wlr::output_t* output) noexcept;

    void init_cursor();

    void handle_new_drag_icon(void* data);

    auto add_keyboard(wlr::input_device_t& device) -> Keyboard&;
    auto add_pointer(wlr::input_device_t& device) -> Pointer&;
    auto add_touch(wlr::input_device_t& device) -> Touch&;
    auto add_tablet_pad(wlr::input_device_t& device) -> TabletPad&;
    auto add_tablet_tool(wlr::input_device_t& device) -> Tablet&;
    SeatView& add_view(View& view);

  private:
    friend SeatView;
    View* _focused_view = nullptr;
  };

} // namespace cloth
