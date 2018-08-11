#include "bar.hpp"

#include <condition_variable>
#include <thread>

#include "client.hpp"

#include "util/chrono.hpp"
#include "util/logging.hpp"

#include "gdkwayland.hpp"

#include "widgets/battery.hpp"

namespace cloth::bar {

  Bar::Bar(Client& client, std::unique_ptr<wl::output_t>&& p_output)
    : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL}, output(std::move(p_output))
  {
    output->on_mode() = [this](wl::output_mode, int32_t w, int32_t h, int32_t refresh) {
      LOGI("Bar width configured: {}", w);
      set_width(w);
    };
    window.set_title("tablecloth panel");
    window.set_decorated(false);

    setup_css();
    setup_widgets();

    gtk_widget_realize(GTK_WIDGET(window.gobj()));
    Gdk::wayland::window::set_use_custom_surface(window);
    surface = Gdk::wayland::window::get_wl_surface(window);
    layer_surface = client.layer_shell.get_layer_surface(
      surface, *output, wl::zwlr_layer_shell_v1_layer::top, "cloth.panel");
    layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::left |
                             wl::zwlr_layer_surface_v1_anchor::top |
                             wl::zwlr_layer_surface_v1_anchor::right);
    layer_surface.set_size(width, client.height);
    layer_surface.on_configure() = [&](uint32_t serial, uint32_t width, uint32_t height) {
      window.show_all();
      layer_surface.ack_configure(serial);
      if (client.height != height) {
        height = client.height;
        LOGD("New Height: {}", height);
        layer_surface.set_size(width, height);
        layer_surface.set_exclusive_zone(visible ? height : 0);
        surface.commit();
      }
    };
    layer_surface.on_closed() = [&] { window.close(); };

    surface.commit();
  }

  auto Bar::setup_css() -> void
  {
    css_provider = Gtk::CssProvider::create();
    style_context = Gtk::StyleContext::create();

    // load our css file, wherever that may be hiding
    if (css_provider->load_from_path(client.css_file)) {
      Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
      style_context->add_provider_for_screen(screen, css_provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
  }

  auto Bar::set_width(int width) -> void
  {
    this->width = width;
    window.set_size_request(width);
    window.resize(width, client.height);
    layer_surface.set_size(width, 40);
    surface.commit();
  }

  auto Bar::toggle() -> void
  {
    visible = !visible;
    auto zone = visible ? client.height : 0;
    layer_surface.set_exclusive_zone(zone);
    surface.commit();
  }

  struct WorkspaceSelectorWidget {
    WorkspaceSelectorWidget(Bar& bar) : bar(bar)
    {
      box.get_style_context()->add_class("workspace-selector");
      bar.client.signals.workspace_state.connect(
        [&](int current, int count) { update(current, count); });
    }

    auto update(int current, int count) -> void
    {
      if (count != buttons.size()) {
        buttons.clear();
        for (int i = 0; i < count; i++) {
          auto& button = buttons.emplace_back(std::to_string(i + 1));
          button.signal_clicked().connect([this, i] { bar.client.workspaces.switch_to(i); });
          box.pack_start(button, false, false, 0);
        }
      }
      for (auto& button : buttons) {
        button.get_style_context()->remove_class("current");
      }
      buttons.at(current).get_style_context()->add_class("current");
    }

    operator Gtk::Widget&()
    {
      return box;
    }

  private:
    Bar& bar;
    Gtk::Box box;
    std::vector<Gtk::Button> buttons;
  };

  struct ClockWidget {
    ClockWidget()
    {
      label.get_style_context()->add_class("clock-widget");
      thread = [this] {
        using namespace chrono;
        auto now = clock::now();
        auto t = std::time(nullptr);
        auto localtime = std::localtime(&t);
        label.set_text(fmt::format("{:02}:{:02}", localtime->tm_hour, localtime->tm_min));
        thread.sleep_until(floor<minutes>(now + minutes(1)));
      };
    };

    operator Gtk::Widget&()
    {
      return label;
    }

    Gtk::Label label;
    util::SleeperThread thread;
  };

  auto Bar::setup_widgets() -> void
  {
    auto& left = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    auto& center = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    auto& right = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));

    auto& box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    window.add(box1);
    box1.set_homogeneous(true);
    box1.pack_start(left, true, true);
    box1.pack_start(center, false, false);
    box1.pack_end(right, true, true);

    auto& rofi_btn = *Gtk::manage(new Gtk::Button("ROFI"));
    rofi_btn.signal_clicked().connect(
      [&]() { client.window_manager.run_command("exec killall rofi || rofi -show drun &"); });

    auto& focused_window = *Gtk::manage(new Gtk::Button());
    focused_window.get_style_context()->add_class("focused-window-title");
    client.signals.focused_window_name.connect([&focused_window](std::string focused_window_name) {
      if (focused_window_name.size() > 70) {
        focused_window_name.erase(67);
        focused_window_name += "...";
      }
      focused_window.set_label(focused_window_name);
    });

    focused_window.signal_clicked().connect([&] { client.window_manager.run_command("next_window");});
    focused_window.set_hexpand(false);

    auto& button = *Gtk::manage(new Gtk::Button("VKBD"));
    button.signal_clicked().connect(
      [&]() { client.window_manager.run_command("exec killall virtboard || virtboard &"); });

    auto& clock = *new ClockWidget();

    auto& workspace_selector = *new WorkspaceSelectorWidget(*this);
    workspace_selector.update(1, 10);

    auto& battery = *new widgets::Battery();

    left.pack_start(focused_window, false, true, 0);
    center.pack_start(workspace_selector, true, false, 10);
    right.pack_end(rofi_btn, false, true, 0);
    right.pack_end(button, false, false, 0);
    right.pack_end(clock, false, false, 0);
    right.pack_end(battery, false, false, 0);
  }

} // namespace cloth::bar
