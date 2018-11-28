#include "client.hpp"

#include <iostream>

#include "util/logging.hpp"

/// Fun idea im trying out. I know its horrible
#define self (*this)

namespace cloth::outputs {

  Output::Output(Client& client, std::unique_ptr<wl::output_t>&& p_output)
    : client(client),
      output(std::move(p_output)),
      xdg_output(client.output_manager.get_xdg_output(*output))
  {
    output->on_scale() = [this] (int scale) { self.scale = scale; };
    xdg_output.on_name() = [this](std::string name) { self.name = name; };
    xdg_output.on_logical_position() = [this](int x, int y) { self.logical_position = {x, y}; };
    xdg_output.on_logical_size() = [this](int w, int h) { self.logical_size = {w, h}; };
    xdg_output.on_description() = [this](std::string desc) { self.description = desc; };
    xdg_output.on_done() = [this] {
      self.done = true;
      self.client.signals.output_list_updated.emit();

      auto& box = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
      auto& title =*Gtk::manage(new Gtk::Label(name, Gtk::ALIGN_START));
      title.set_markup(fmt::format("<big>{}</big>", name));
      box.add(title);
      box.add(*Gtk::manage(
        new Gtk::Label(fmt::format("{}x{}+{}+{} * {}", logical_size.width, logical_size.height,
                                   logical_position.x, logical_position.y, scale), Gtk::ALIGN_START)));
      box.add(*Gtk::manage(new Gtk::Label(description, Gtk::ALIGN_START)));
      box.show_all();
      lbr.add(box);
    };
  }

  Output::operator Gtk::Widget&()
  {
    return lbr;
  }

  auto Client::bind_interfaces()
  {
    registry = display.get_registry();
    registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
      cloth_debug("Global: {}", interface);
      if (interface == output_manager.interface_name) {
        registry.bind(name, output_manager, version);
      } else if (interface == wl::output_t::interface_name) {
        auto output = std::make_unique<wl::output_t>();
        registry.bind(name, *output, version);
        outputs.emplace_back(self, std::move(output));
      }
    };
    display.roundtrip();
  }

  int Client::main(int argc, char* argv[])
  {
    auto cli = make_cli();
    auto result = cli.parse(clara::Args(argc, argv));
    wlr_log_init(WLR_DEBUG, nullptr);

    if (!result) {
      cloth_error("Error in command line: {}", result.errorMessage());
      return 1;
    }
    if (show_help) {
      std::cout << cli;
      return 1;
    }

    bind_interfaces();
    setup_gui();

    gtk_main.run();
    return 0;
  }

  auto Client::setup_gui() -> void
  {
    window.set_title("Output settings");
    window.show();

    auto& list = *Gtk::manage(new Gtk::ListBox());
    window.add(list);

    signals.output_list_updated.connect([&] {
      for (auto& chld : list.get_children()) {
        list.remove(*chld);
      }
      for (auto& output : outputs) {
        list.add(output);
      }
      list.show_all();
    });

    window.show_all();
  }

} // namespace cloth::outputs
