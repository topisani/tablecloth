#include "seat.hpp"

#include "cursor.hpp"
#include "input.hpp"
#include "keyboard.hpp"
#include "seat.hpp"
#include "xcursor.hpp"

#include "util/algorithm.hpp"
#include "util/exception.hpp"
#include "util/logging.hpp"

namespace cloth {

  Seat::Seat(Input& input, const char* name)
    : wlr_seat(wlr_seat_create(input.server.wl_display, name)), input(input), cursor(*this, nullptr)
  {
    if (!wlr_seat) throw util::exception("Could not create wlr_seat from name {}", name);

    init_cursor();

    on_new_drag_icon = [this](void* data) { handle_new_drag_icon(data); };
    on_new_drag_icon.add_to(wlr_seat->events.new_drag_icon);

    on_destroy = [this] { util::erase_this(this->input.seats, *this); };
    on_destroy.add_to(wlr_seat->events.destroy);
  }

  Seat::~Seat()
  {
    wlr_seat_destroy(wlr_seat);
  }

  void Seat::reset_device_mappings(wlr::input_device_t& device) noexcept
  {
    wlr::cursor_t* cursor = this->cursor.wlr_cursor;
    Config& config = input.config;

    wlr_cursor_map_input_to_output(cursor, &device, nullptr);
    Config::Device* dconfig;
    if ((dconfig = config.get_device(device))) {
      wlr_cursor_map_input_to_region(cursor, &device, &dconfig->mapped_box);
    }
  }

  void Seat::set_device_output_mappings(wlr::input_device_t& device, wlr::output_t* output) noexcept
  {
    wlr::cursor_t* cursor = this->cursor.wlr_cursor;
    Config& config = input.config;
    Config::Device* dconfig = config.get_device(device);

    std::string_view mapped_output = "";
    if (dconfig != nullptr) {
      mapped_output = dconfig->mapped_output;
    }
    if (mapped_output.empty()) {
      mapped_output = device.output_name;
    }

    if (mapped_output == device.output_name) {
      wlr_cursor_map_input_to_output(cursor, &device, output);
    }
  }

  void Seat::configure_cursor()
  {
    Config& config = input.config;
    Desktop& desktop = input.server.desktop;
    wlr::cursor_t* cursor = this->cursor.wlr_cursor;

    // reset mappings
    wlr_cursor_map_to_output(cursor, nullptr);
    for (auto& pointer : pointers) {
      reset_device_mappings(pointer.device);
    }
    for (auto& touch : this->touch) {
      reset_device_mappings(touch.device);
    }
    for (auto& tablet_tool : tablet_tools) {
      reset_device_mappings(tablet_tool.device);
    }

    // configure device to output mappings
    std::string_view mapped_output = "";
    Config::Cursor* cc = config.get_cursor(wlr_seat->name);
    if (cc != nullptr) {
      mapped_output = cc->mapped_output;
    }
    for (auto& output : desktop.outputs) {
      if (mapped_output == output.wlr_output->name) {
        wlr_cursor_map_to_output(cursor, output.wlr_output);
      }

      for (auto& pointer : pointers) {
        set_device_output_mappings(pointer.device, output.wlr_output);
      }
      for (auto& tablet_tool : tablet_tools) {
        set_device_output_mappings(tablet_tool.device, output.wlr_output);
      }
      for (auto& touch : this->touch) {
        set_device_output_mappings(touch.device, output.wlr_output);
      }
    }
  }

  void Seat::init_cursor()
  {
    wlr::cursor_t* wlr_cursor = this->cursor.wlr_cursor;
    Desktop& desktop = input.server.desktop;
    wlr_cursor_attach_output_layout(wlr_cursor, desktop.layout);

    configure_cursor();
    configure_xcursor();
  }

  void Seat::handle_new_drag_icon(void* data)
  {
    auto& wlr_drag_icon = *(wlr::drag_icon_t*) data;

    DragIcon& icon = drag_icons.emplace_back(*this, &wlr_drag_icon);

    icon.on_surface_commit = [&] { icon.update_position(); };
    icon.on_surface_commit.add_to(wlr_drag_icon.surface->events.commit);

    auto handle_damage_whole = [&] { icon.damage_whole(); };

    icon.on_unmap = handle_damage_whole;
    icon.on_unmap.add_to(wlr_drag_icon.events.unmap);
    icon.on_map = handle_damage_whole;
    icon.on_map.add_to(wlr_drag_icon.events.map);
    icon.on_destroy = handle_damage_whole;
    icon.on_destroy.add_to(wlr_drag_icon.events.destroy);

    icon.update_position();
  }

  void DragIcon::update_position()
  {
    damage_whole();

    wlr::drag_icon_t& wlr_icon = wlr_drag_icon;
    wlr::cursor_t* cursor = seat.cursor.wlr_cursor;
    if (wlr_icon.is_pointer) {
      x = cursor->x + wlr_icon.sx;
      y = cursor->y + wlr_icon.sy;
    } else {
      wlr::touch_point_t* point = wlr_seat_touch_get_point(seat.wlr_seat, wlr_icon.touch_id);
      if (point == nullptr) {
        return;
      }
      x = seat.touch_x + wlr_icon.sx;
      y = seat.touch_y + wlr_icon.sy;
    }

    damage_whole();
  }

  void DragIcon::damage_whole()
  {
    for (auto& output : seat.input.server.desktop.outputs) {
      output.damage_whole_drag_icon(*this);
    }
  }

  void Seat::update_capabilities() noexcept
  {
    uint32_t caps = 0;
    if (!keyboards.empty()) {
      caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (!pointers.empty() || !tablet_tools.empty()) {
      caps |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (!touch.empty()) {
      caps |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wlr_seat_set_capabilities(wlr_seat, caps);

    // Hide cursor if seat doesn't have pointer capability
    if ((caps & WL_SEAT_CAPABILITY_POINTER) == 0) {
      wlr_cursor_set_image(cursor.wlr_cursor, nullptr, 0, 0, 0, 0, 0, 0);
    } else {
      wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, cursor.default_xcursor.c_str(),
                                           cursor.wlr_cursor);
    }
  }

  Pointer::Pointer(Seat& seat, wlr::input_device_t& device) noexcept : seat(seat), device(device)
  {
    assert(device.type == WLR_INPUT_DEVICE_POINTER);

    device.data = this;
    wlr_cursor_attach_input_device(seat.cursor.wlr_cursor, &device);

    on_device_destroy.add_to(device.events.destroy);
    on_device_destroy = [this] {
      util::erase_this(this->seat.pointers, this);
      this->seat.update_capabilities();
    };
    seat.configure_cursor();
  }

  Touch::Touch(Seat& seat, wlr::input_device_t& device) noexcept : seat(seat), device(device)
  {
    assert(device.type == WLR_INPUT_DEVICE_TOUCH);

    device.data = this;
    wlr_cursor_attach_input_device(seat.cursor.wlr_cursor, &device);

    on_device_destroy.add_to(device.events.destroy);
    on_device_destroy = [this] {
      util::erase_this(this->seat.touch, this);
      this->seat.update_capabilities();
    };

    seat.configure_cursor();
  }

  TabletTool::TabletTool(Seat& seat, wlr::input_device_t& device) noexcept
    : seat(seat), device(device)
  {
    assert(device.type == WLR_INPUT_DEVICE_TABLET_TOOL);

    device.data = this;
    wlr_cursor_attach_input_device(seat.cursor.wlr_cursor, &device);

    on_device_destroy.add_to(device.events.destroy);
    on_device_destroy = [this] {
      util::erase_this(this->seat.tablet_tools, this);
      this->seat.update_capabilities();
    };

    seat.configure_cursor();
  }

  Pointer::~Pointer() noexcept
  {
    wlr_cursor_detach_input_device(seat.cursor.wlr_cursor, &device);
    this->seat.update_capabilities();
  }

  Touch::~Touch() noexcept
  {
    wlr_cursor_detach_input_device(seat.cursor.wlr_cursor, &device);
    this->seat.update_capabilities();
  }

  TabletTool::~TabletTool() noexcept
  {
    wlr_cursor_detach_input_device(seat.cursor.wlr_cursor, &device);
    this->seat.update_capabilities();
  }

  void Seat::add_keyboard(wlr::input_device_t& device)
  {
    assert(device.type == WLR_INPUT_DEVICE_KEYBOARD);
    keyboards.emplace_back(*this, device);
    wlr_seat_set_keyboard(wlr_seat, &device);
  }

  void Seat::add_pointer(wlr::input_device_t& device)
  {
    assert(device.type == WLR_INPUT_DEVICE_POINTER);
    pointers.emplace_back(*this, device);
  }

  void Seat::add_touch(wlr::input_device_t& device)
  {
    assert(device.type == WLR_INPUT_DEVICE_TOUCH);
    touch.emplace_back(*this, device);
  }

  void Seat::add_tablet_pad(wlr::input_device_t& device)
  {
    assert(device.type == WLR_INPUT_DEVICE_TABLET_PAD);
    // TODO
  }

  void Seat::add_tablet_tool(wlr::input_device_t& device)
  {
    assert(device.type == WLR_INPUT_DEVICE_TABLET_TOOL);
    tablet_tools.emplace_back(*this, device);
  }

  void Seat::add_device(wlr::input_device_t& device) noexcept
  {
    switch (device.type) {
    case WLR_INPUT_DEVICE_KEYBOARD: add_keyboard(device); break;
    case WLR_INPUT_DEVICE_POINTER: add_pointer(device); break;
    case WLR_INPUT_DEVICE_TOUCH: add_touch(device); break;
    case WLR_INPUT_DEVICE_TABLET_PAD: add_tablet_pad(device); break;
    case WLR_INPUT_DEVICE_TABLET_TOOL: add_tablet_tool(device); break;
    }

    update_capabilities();
  }

  void Seat::configure_xcursor()
  {
    const char* cursor_theme = nullptr;
    Config::Cursor* cc = input.config.get_cursor(wlr_seat->name);

    if (cc != nullptr) {
      cursor_theme = cc->theme.c_str();
      if (!cc->default_image.empty()) {
        cursor.default_xcursor = cc->default_image;
      }
    }

    if (!cursor.xcursor_manager) {
      cursor.xcursor_manager = wlr_xcursor_manager_create(cursor_theme, xcursor_size);
      if (cursor.xcursor_manager == nullptr) {
        LOGE("Cannot create XCursor manager for theme {}", cursor_theme);
        return;
      }
    }

    for (auto& output : input.server.desktop.outputs) {
      float scale = output.wlr_output->scale;
      if (wlr_xcursor_manager_load(cursor.xcursor_manager, scale)) {
        LOGE("Cannot load xcursor theme for output '{}' with scale {}", output.wlr_output->name,
             scale);
      }
    }

    wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, cursor.default_xcursor.c_str(),
                                         cursor.wlr_cursor);
    wlr_cursor_warp(cursor.wlr_cursor, nullptr, cursor.wlr_cursor->x, cursor.wlr_cursor->y);
  }

  bool Seat::has_meta_pressed()
  {
    for (auto& keyboard : keyboards) {
      if (!keyboard.config.meta_key) {
        continue;
      }

      uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard.device->keyboard);
      if ((modifiers ^ keyboard.config.meta_key) == 0) {
        return true;
      }
    }

    return false;
  }

  View* Seat::get_focus()
  {
    if (!has_focus || views.empty()) {
      return nullptr;
    }
    return &views.front().view;
  }

  SeatView::SeatView(Seat& seat, View& view) noexcept : seat(seat), view(view)
  {
    on_view_unmap = [&] { util::erase_this(seat, this); };
    on_view_unmap.add_to(view.events.unmap);
    on_view_destroy = [&] { util::erase_this(seat, this); };
    on_view_destroy.add_to(view.events.destroy);
  }

  SeatView::~SeatView() noexcept
  {
    if (&view == seat.get_focus()) {
      seat.has_focus = false;
      seat.cursor.mode = Cursor::Mode::Passthrough;
    }

    if (this == seat.cursor.pointer_view) {
      seat.cursor.pointer_view = nullptr;
    }

    // Focus first view
    if (!seat.views.empty()) {
      seat.set_focus(seat.views.front().view);
    }
  }

  SeatView& Seat::add_view(View& view)
  {
    auto& seat_view = views.emplace_back(*this, view);
    return seat_view;
  }

  SeatView& Seat::seat_view_from_view(View& view)
  {
    auto found = util::find_if(views, [&](auto& sv) { return &sv.view == view; });
    if (found == views.end()) {
      return add_view(view);
    }
    return *found;
  }

  bool Seat::allow_input(wl::resource_t& resource)
  {
    return !exclusive_client || wl_resource_get_client(&resource) == exclusive_client;
  }

  void Seat::set_focus(View* view)
  {
    if (view && !allow_input(*view->wlr_surface->resource)) {
      return;
    }

    // Make sure the view will be rendered on top of others, even if it's
    // already focused in this seat
    if (view != nullptr) {
      // wl_list_remove(&view->link);
      // wl_list_insert(&seat->input->server->desktop->views, &view->link);
    }

    bool unfullscreen = true;

#ifdef WLR_HAS_XWAYLAND
    if (view && view->type() == ViewType::xwayland &&
        view->get_surface<ViewType::xwayland>().wlr_surface->override_redirect) {
      unfullscreen = false;
    }
#endif

    if (view && unfullscreen) {
      Desktop& desktop = view->desktop;
      wlr::box_t box = view->get_box();
      for (auto& output : desktop.outputs)
      {
        if (output.fullscreen_view && output.fullscreen_view != view &&
            wlr_output_layout_intersects(desktop.layout, output.wlr_output, &box)) {
          output.fullscreen_view->set_fullscreen(false, nullptr);
        }
      }
    }

    View* prev_focus = get_focus();
    if (view == prev_focus) {
      return;
    }

#ifdef WLR_HAS_XWAYLAND
    if (view && view->type() == ViewType::xwayland &&
        wlr_xwayland_surface_is_unmanaged(view->get_surface<ViewType::xwayland>().wlr_surface)) {
      return;
    }
#endif

    SeatView* seat_view = nullptr;
    if (view != nullptr) {
      seat_view = &seat_view_from_view(*view);
      if (seat_view == nullptr) {
        return;
      }
    }

    has_focus = false;

    // Deactivate the old view if it is not focused by some other seat
    if (prev_focus != nullptr && !input.view_has_focus(*prev_focus)) {
      prev_focus->activate(false);
    }

    if (view == nullptr) {
      cursor.mode = Cursor::Mode::Passthrough;
      wlr_seat_keyboard_clear_focus(wlr_seat);
      return;
    }

    //wl_list_remove(&seat_view->link);
    //wl_list_insert(&seat->views, &seat_view->link);

    view->damage_whole();

    if (focused_layer) {
      return;
    }

    view->activate(true);
    has_focus = true;

    // An existing keyboard grab might try to deny setting focus, so cancel it
    wlr_seat_keyboard_end_grab(wlr_seat);

    wlr::keyboard_t* keyboard = wlr_seat_get_keyboard(wlr_seat);
    if (keyboard != nullptr) {
      wlr_seat_keyboard_notify_enter(wlr_seat, view->wlr_surface, keyboard->keycodes,
                                     keyboard->num_keycodes, &keyboard->modifiers);
    } else {
      wlr_seat_keyboard_notify_enter(wlr_seat, view->wlr_surface, nullptr, 0, nullptr);
    }
  }

  /**
   * Focus semantics of layer surfaces are somewhat detached from the normal focus
   * flow. For layers above the shell layer, for example, you cannot unfocus them.
   * You also cannot alt-tab between layer surfaces and shell surfaces.
   */
  void Seat::set_focus_layer(wlr::layer_surface_t* layer)
  {
    if (!layer) {
      focused_layer = nullptr;
      return;
    }
    wlr::keyboard_t* keyboard = wlr_seat_get_keyboard(wlr_seat);
    if (!allow_input(*layer->resource)) {
      return;
    }
    if (has_focus) {
      View* prev_focus = get_focus();
      wlr_seat_keyboard_clear_focus(wlr_seat);
      prev_focus->activate(false);
    }
    has_focus = false;
    if (layer->layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
      focused_layer = layer;
    }
    if (keyboard != nullptr) {
      wlr_seat_keyboard_notify_enter(wlr_seat, layer->surface, keyboard->keycodes,
                                     keyboard->num_keycodes, &keyboard->modifiers);
    } else {
      wlr_seat_keyboard_notify_enter(wlr_seat, layer->surface, nullptr, 0, nullptr);
    }
  }

  void Seat::set_exclusive_client(wl::client_t* client)
  {
    if (!client) {
      exclusive_client = client;
      // Triggers a refocus of the topmost surface layer if necessary
      // TODO: Make layer surface focus per-output based on cursor position
      for (auto& output : input.server.desktop.outputs) {
        arrange_layers(output);
      }
      return;
    }
    if (focused_layer) {
      if (wl_resource_get_client(focused_layer->resource) != client) {
        set_focus_layer(nullptr);
      }
    }
    if (has_focus) {
      View* focus = get_focus();
      if (wl_resource_get_client(focus->wlr_surface->resource) != client) {
        set_focus(nullptr);
      }
    }
    if (wlr_seat->pointer_state.focused_client) {
      if (wlr_seat->pointer_state.focused_client->client != client) {
        wlr_seat_pointer_clear_focus(wlr_seat);
      }
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr::touch_point_t* point;
    wl_list_for_each(point, &wlr_seat->touch_state.touch_points, link)
    {
      if (point->client->client != client) {
        wlr_seat_touch_point_clear_focus(wlr_seat, now.tv_nsec / 1000, point->touch_id);
      }
    }
    exclusive_client = client;
  }

  void Seat::cycle_focus()
  {
    if (views.empty()) {
      return;
    }

    SeatView* first_seat_view = &views.front();
    if (!has_focus) {
      set_focus(&first_seat_view->view);
      return;
    }
    if (views.size() < 2) {
      return;
    }

    // Focus the next view
    SeatView* next_seat_view = &*(views.begin() + 1);
    set_focus(&next_seat_view->view);

    // Move the first view to the end of the list
    // PREV: wl_list_remove(&first_seat_view->link);
    // PREV: wl_list_insert(seat->views.prev, &first_seat_view->link);
  }

  void Seat::begin_move(View& view)
  {
    cursor.mode = Cursor::Mode::Move;
    cursor.offs_x = cursor.wlr_cursor->x;
    cursor.offs_y = cursor.wlr_cursor->y;
    if (view.maximized) {
      cursor.view_x = view.saved.x;
      cursor.view_y = view.saved.y;
    } else {
      cursor.view_x = view.x;
      cursor.view_y = view.y;
    }
    view.maximize(false);
    wlr_seat_pointer_clear_focus(wlr_seat);

    wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, xcursor_move,
                                         cursor.wlr_cursor);
  }

  void Seat::begin_resize(View& view, wlr::edges_t edges)
  {
    cursor.mode = Cursor::Mode::Resize;
    cursor.offs_x = cursor.wlr_cursor->x;
    cursor.offs_y = cursor.wlr_cursor->y;
    if (view.maximized) {
      cursor.view_x = view.saved.x;
      cursor.view_y = view.saved.y;
      cursor.view_width = view.saved.width;
      cursor.view_height = view.saved.height;
    } else {
      cursor.view_x = view.x;
      cursor.view_y = view.y;
      wlr::box_t box = view.get_box();
      cursor.view_width = box.width;
      cursor.view_height = box.height;
    }
    cursor.resize_edges = edges;
    view.maximize(false);
    wlr_seat_pointer_clear_focus(wlr_seat);

    const char* resize_name = wlr_xcursor_get_resize_name(edges);
    wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, resize_name,
                                         cursor.wlr_cursor);
  }

  void Seat::begin_rotate(View& view)
  {
    cursor.mode = Cursor::Mode::Rotate;
    cursor.offs_x = cursor.wlr_cursor->x;
    cursor.offs_y = cursor.wlr_cursor->y;
    cursor.view_rotation = view.rotation;
    view.maximize(false);
    wlr_seat_pointer_clear_focus(wlr_seat);

    wlr_xcursor_manager_set_cursor_image(cursor.xcursor_manager, xcursor_rotate,
                                         cursor.wlr_cursor);
  }

  void Seat::end_compositor_grab()
  {
    View* view = get_focus();
    if (view == nullptr) return;

    switch (cursor.mode) {
    case Cursor::Mode::Move: view->move(cursor.view_x, cursor.view_y); break;
    case Cursor::Mode::Resize:
      view->move_resize(cursor.view_x, cursor.view_y, cursor.view_width,
                       cursor.view_height);
      break;
    case Cursor::Mode::Rotate: view->rotation = cursor.view_rotation; break;
    case Cursor::Mode::Passthrough: break;
    }

    cursor.mode = Cursor::Mode::Passthrough;
  }

} // namespace cloth
