#pragma once

#include <vector>

#include "cursor.hpp"
#include "keyboard.hpp"
#include "util/bindings.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Seat;
  struct Input;
  struct View;

  struct DragIcon {
    ~DragIcon() noexcept;
    void update_position();
    void damage_whole();

    Seat& seat;
    wlr::drag_icon_t& wlr_drag_icon;

    double x, y;

    wl::Listener on_surface_commit;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_destroy;

  private:
    friend struct Seat;
    DragIcon(Seat&, wlr::drag_icon_t&) noexcept;
  };

  struct Pointer {
    ~Pointer() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;

  private:
    friend struct Seat;
    Pointer(Seat&, wlr::input_device_t&) noexcept;
  };

  struct Touch {
    ~Touch() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;

  private:
    friend struct Seat;
    Touch(Seat&, wlr::input_device_t&) noexcept;
  };

  struct TabletTool {
    ~TabletTool() noexcept;
    Seat& seat;

    wlr::input_device_t& device;
    wl::Listener on_device_destroy;

    wl::Listener on_axis;
    wl::Listener on_proximity;
    wl::Listener on_tip;
    wl::Listener on_button;

  private:
    friend struct Seat;
    TabletTool(Seat&, wlr::input_device_t&) noexcept;
  };

  struct SeatView {
    ~SeatView() noexcept;
    Seat& seat;
    View& view;

    bool has_button_grab;
    double grab_sx;
    double grab_sy;

    wl::Listener on_view_unmap;
    wl::Listener on_view_destroy;

  private:
    friend struct Seat;
    SeatView(Seat&, View&) noexcept;
  };

  struct Seat {
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

    wlr::seat_t* wlr_seat;
    Input& input;
    Cursor cursor;

    // coordinates of the first touch point if it exists
    int touch_id;
    double touch_x, touch_y;

    // If the focused layer is set, views cannot receive keyboard focus
    wlr::layer_surface_t* focused_layer;

    // If non-null, only this client can receive input events
    wl::client_t* exclusive_client;

    std::vector<SeatView> views;
    bool has_focus;

    std::vector<DragIcon> drag_icons; // roots_drag_icon::link

    std::vector<Keyboard> keyboards;
    std::vector<Pointer> pointers;
    std::vector<Touch> touch;
    std::vector<TabletTool> tablet_tools;

    wl::Listener on_new_drag_icon;
    wl::Listener on_destroy;

  private:
    Seat(Input& input, const char* name);
    friend struct Input;

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
