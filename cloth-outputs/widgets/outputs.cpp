#include "outputs.hpp"

#include "util/logging.hpp"

namespace cloth::outputs::widgets {

  Outputs::Outputs(util::ptr_vec<Output>& outputs) : outputs(outputs)
  {
    set_events(Gdk::EventMask::BUTTON_MOTION_MASK | Gdk::EventMask::BUTTON_PRESS_MASK |
               Gdk::EventMask::BUTTON_RELEASE_MASK);
  }

  Outputs::~Outputs() {}

  bool Outputs::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
  {
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    xscale = width / double(max_size.width);
    yscale = height / double(max_size.height);

    for (auto& o : outputs) {
      draw_output_box(cr, o);
    }

    return true;
  }

  void Outputs::draw_output_box(const Cairo::RefPtr<Cairo::Context>& cr, Output& o)
  {
    auto box = output_box(o);
    cr->set_line_width(3);
    cr->rectangle(box.x, box.y, box.width, box.height);
    if (&o == current)
      cr->set_source_rgb(0.9, 0.9, 0.9);
    else
      cr->set_source_rgb(0.7, 0.7, 0.7);
    cr->fill_preserve();
    cr->set_source_rgb(0.2, 0.2, 0.2);
    cr->stroke();

    auto layout = create_pango_layout(fmt::format("{}\n{}x{}+{}+{}", o.name, o.logical_size.width,
                                                  o.logical_size.height, o.logical_position.x,
                                                  o.logical_position.y));
    int text_width, text_height;

    layout->get_pixel_size(text_width, text_height);
    cr->move_to(box.center().x - text_width / 2, box.center().y - text_height / 2);
    layout->show_in_cairo_context(cr);
  }

  bool Outputs::on_button_press_event(GdkEventButton* event)
  {
    auto output = output_at({event->x, event->y});
    if (output == nullptr) return true;
    current = output;
    if (event->button == 1) {
      auto box = output_box(*output);
      drag.output = output;
      drag.grab_coords = {event->x - box.x, event->y - box.y};
      queue_draw();
    } else if (event->button == 3) {
      auto menu = menu_for(*output);
      menu->popup(event->button, event->time);
    }
    return true;
  }

  bool Outputs::on_button_release_event(GdkEventButton* event)
  {
    if (drag.output == nullptr) return false;
    auto coords = current_drag_coords();
    drag.output->set_position(
      {int(std::round(coords.x / xscale)), int(std::round(coords.y / yscale))});
    drag.output = nullptr;
    queue_draw();
    return true;
  }

  bool Outputs::on_motion_notify_event(GdkEventMotion* motion_event)
  {
    queue_draw();
    return false;
  }

  Box Outputs::output_box(Output& o, bool dragged) const
  {
    if (dragged && &o == drag.output) {
      auto coords = current_drag_coords();
      return {coords.x, coords.y, o.logical_size.width * xscale, o.logical_size.height * yscale};
    }
    return {o.logical_position.x * xscale, o.logical_position.y * yscale,
            o.logical_size.width * xscale, o.logical_size.height * yscale};
  }

  Output* Outputs::output_at(Coords c) const
  {
    for (auto& o : outputs) {
      if (output_box(o).contains(c)) return &o;
    }
    return nullptr;
  }

  Coords Outputs::current_drag_coords() const
  {
    int curs_x, curs_y;
    get_pointer(curs_x, curs_y);
    auto res = Coords{curs_x - drag.grab_coords.x, curs_y - drag.grab_coords.y};
    if (drag.output == nullptr) return res;

    res.x = std::max(res.x, 0.0);
    res.y = std::max(res.y, 0.0);

    // Snap
    constexpr auto snap = 10;

    auto lhs = output_box(*drag.output, false);
    lhs.x = res.x;
    lhs.y = res.y;

    for (auto& o : outputs) {
      if (&o == drag.output) continue;
      auto rhs = output_box(o);

      if (std::abs(lhs.x - rhs.x) < snap) res.x = rhs.x;
      if (std::abs(lhs.y - rhs.y) < snap) res.y = rhs.y;

      if (std::abs(lhs.x + lhs.width - rhs.x) < snap) res.x = rhs.x - lhs.width;
      if (std::abs(lhs.y + lhs.height - rhs.y) < snap) res.y = rhs.y - lhs.height;

      if (std::abs(lhs.x - (rhs.x + rhs.width)) < snap) res.x = rhs.x + rhs.width;
      if (std::abs(lhs.y - (rhs.y + rhs.height)) < snap) res.y = rhs.y + rhs.height;
    }

    return res;
  }

  Gtk::Menu* resolution_menu_for(Output& o)
  {
    auto menu = Gtk::make_managed<Gtk::Menu>();
    for (auto& m : o.avaliable_modes) {
      auto item = Gtk::make_managed<Gtk::RadioMenuItem>();
      auto preferred = m.preferred ? " - preferred" : "";
      item->set_active(m.current);
      item->set_label(fmt::format("{}x{}@{}Hz{}", m.width, m.height, m.refresh, preferred));
      item->signal_activate().connect([&o, m] { o.set_mode(m); });
      menu->append(*item);
    }
    menu->show_all();
    return menu;
  }

  Gtk::Menu* transform_menu_for(Output& o)
  {
    auto menu = Gtk::make_managed<Gtk::Menu>();
    for (int i = 0; i < 8; i++) {
      Transform t = static_cast<Transform>(i);
      auto item = Gtk::make_managed<Gtk::RadioMenuItem>();
      item->set_active(o.transform == t);
      item->set_label(to_string(t));
      item->signal_activate().connect([&o, t] { o.set_transform(t); });
      menu->append(*item);
    }
    menu->show_all();
    return menu;
  }

  Gtk::Menu* Outputs::menu_for(Output& o)
  {
    auto menu = Gtk::make_managed<Gtk::Menu>();
    auto res_item = Gtk::make_managed<Gtk::MenuItem>("Resolution");
    res_item->set_submenu(*resolution_menu_for(o));
    menu->append(*res_item);
    auto trans_item = Gtk::make_managed<Gtk::MenuItem>("Transform");
    trans_item->set_submenu(*transform_menu_for(o));
    menu->append(*trans_item);
    menu->show_all();
    menu->accelerate(*this);
    return menu;
  }
} // namespace cloth::outputs::widgets
