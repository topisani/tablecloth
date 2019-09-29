#pragma once

#include <gtkmm.h>

#include "util/ptr_vec.hpp"

#include "output.hpp"

namespace cloth::outputs::widgets {

  struct Coords {
    double x = 0;
    double y = 0;
  };

  struct Box {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    Coords center() const {
      return {x + width / 2, y + height / 2};
    }

    bool contains(Coords c) const {
      return c.x >= x && c.x < x + width && c.y >= y && c.y < y + height;
    }
  };

  struct Outputs : Gtk::DrawingArea {
    Outputs(util::ptr_vec<Output>& outputs);
    virtual ~Outputs();

    util::ptr_vec<Output>& outputs;

  protected:

    struct Drag {
      Output* output = nullptr;
      Coords grab_coords;
    } drag;

    Coords current_drag_coords() const;

    double xscale = 0;
    double yscale = 0;

    Size max_size = {4000, 4000};

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_button_press_event(GdkEventButton * event) override;
    bool on_button_release_event(GdkEventButton * event) override;
    bool on_motion_notify_event(GdkEventMotion* motion_event) override;

    Box output_box(Output&, bool dragged = true) const;
    Output* output_at(Coords) const;

    void draw_output_box(const Cairo::RefPtr<Cairo::Context>& cr, Output& o);
  };

} // namespace cloth::outputs::widgets
