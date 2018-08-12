#pragma once

#include <gtkmm.h>

#include <protocols.hpp>

#include "layouts.hpp"
#include "util/ptr_vec.hpp"

namespace cloth::kbd {

  namespace wl = wayland;

  struct Client;

  struct LayerWidget {
    LayerWidget(KeyboardState& state, Layer in_layer) : state(state), layer(std::move(in_layer))
    {
      int row_idx = 0;
      for (auto& row : layer) {
        int col_idx = 0;
        for (auto& key : row) {
          auto& btn = *Gtk::manage(new Gtk::Button(key.label));
          btn.signal_pressed().connect([&] {
            if (key.press_func) key.press_func(state);
          });
          btn.signal_released().connect([&] {
            if (key.release_func) key.release_func(state);
          });
          _widget.attach(btn, col_idx, row_idx, key.width, 1);
          col_idx += key.width;
        }
        row_idx++;
      }
    }

    operator Gtk::Widget&() noexcept
    {
      return _widget;
    }

    KeyboardState& state;
    const Layer layer;
    ;

  private:
    Gtk::Grid _widget;
  };

  struct LayerWidgets {
    LayerWidgets(KeyboardState& state) : state(state) {}
    LayerWidget& operator[](LayerType t)
    {
      return layers.at(util::enum_cast(t));
    }

    const LayerWidget& operator[](LayerType t) const
    {
      return layers.at(util::enum_cast(t));
    }

    auto begin()
    {
      return layers.begin();
    }
    auto end()
    {
      return layers.end();
    }
    auto begin() const
    {
      return layers.begin();
    }
    auto end() const
    {
      return layers.end();
    }

    KeyboardState& state;
    std::array<LayerWidget, 3> layers = {
      {{state, letters_layer}, {state, number_layer}, {state, number_layer}}};
  };


  struct VirtualKeyboard;

  struct KeyboardState {
    KeyboardState(VirtualKeyboard& vkbd) : vkbd(vkbd) {};
    auto send_key_press(xkb_keysym_t) -> void;
    auto send_key_release(xkb_keysym_t) -> void;
    auto send_modifier_press(Modifiers m) -> void;
    auto set_layer(LayerType) -> void;

    VirtualKeyboard& vkbd;
    LayerWidgets layers = {*this};
    util::non_null_ptr<LayerWidget> current_layer = &layers[LayerType::Letters];
    Modifiers modifiers;
  };

  struct VirtualKeyboard {
    VirtualKeyboard(Client& client);

    VirtualKeyboard(const VirtualKeyboard&) = delete;

    Client& client;

    Gtk::Window window;
    wl::surface_t surface;
    wl::zwlr_layer_surface_v1_t layer_surface;
    wl::zwp_virtual_keyboard_v1_t wp_keyboard;

    KeyboardState state;

    auto set_size(int width, int height) -> void;

  private:
    auto setup_widgets() -> void;
    auto setup_css() -> void;
    auto setup_kbd_protocol() -> void;

    int width = 10;
    int height = 10;
    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;
  };

  inline Key::Key(std::string label,
                  std::function<void(KeyboardState&)> press_func,
                  std::function<void(KeyboardState&)> release_func, int width)
    : label(std::move(label)), press_func(std::move(press_func)), release_func(std::move(release_func)), width(width)
  {}

  inline Key::Key(std::string label, xkb_keysym_t key, int width)
    : label(std::move(label)),
      press_func([key](KeyboardState& ks) { ks.send_key_press(key); }),
      release_func([key](KeyboardState& ks) { ks.send_key_release(key); }),
      width(width)
  {}

  inline Key::Key(std::string label, LayerType layer, int width)
    : label(std::move(label)),
      press_func([layer](KeyboardState& ks) { ks.set_layer(layer); }),
      release_func(nullptr),
      width(width)
  {}

  inline Key::Key(std::string label, Modifiers modifier, int width)
    : label(std::move(label)),
      press_func([modifier](KeyboardState& ks) { ks.send_modifier_press(modifier); }),
      width(width)
  {}

} // namespace cloth::kbd
