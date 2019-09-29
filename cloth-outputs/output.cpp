#include "output.hpp"

#include "client.hpp"

namespace cloth::outputs {

  Output::Output(Client& client, std::unique_ptr<wl::output_t>&& p_output)
    : client(client),
      output(std::move(p_output)),
      xdg_output(client.output_manager.get_xdg_output(*output))
  {
    output->on_scale() = [this](int scale) { this->scale = scale; };
    xdg_output.on_name() = [this](std::string name) { this->name = name; };
    xdg_output.on_logical_position() = [this](int x, int y) { this->logical_position = {x, y}; };
    xdg_output.on_logical_size() = [this](int w, int h) { this->logical_size = {w, h}; };
    xdg_output.on_description() = [this](std::string desc) { this->description = desc; };
    xdg_output.on_done() = [this] {
      this->done = true;
      this->client.signals.output_list_updated.emit();
    };
  }

  void Output::set_position(Position p)
  {
    send_sway_cmd("output '{}' pos {} {}", name, p.x, p.y);
  }

  void Output::set_size(Size s)
  {
    send_sway_cmd("output '{}' size '{} {}'", name, s.width, s.height);
  }

  void Output::toggle()
  {
    send_sway_cmd("output '{}' toggle", name);
  }

} // namespace cloth::outputs
