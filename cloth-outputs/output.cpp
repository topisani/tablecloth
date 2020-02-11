#include "output.hpp"

#include "client.hpp"

namespace cloth::outputs {

  Output::Output(Client& client, std::unique_ptr<wl::output_t>&& p_output)
    : client(client),
      output(std::move(p_output)),
      xdg_output(client.output_manager.get_xdg_output(*output))
  {
    output->on_scale() = [this](int scale) { this->scale = scale; };
    output->on_mode() = [this](auto flags, auto width, auto height, auto refresh) {
      avaliable_modes.push_back({.preferred = flags & wl::output_mode::preferred,
                                 .current = flags & wl::output_mode::current,
                                 .width = width,
                                 .height = height,
                                 .refresh = refresh / 1000});
    };
    output->on_geometry() = [this](int32_t, int32_t, int32_t, int32_t, wl::output_subpixel,
                                   std::string, std::string, wl::output_transform transform) {
      if (transform & wl::output_transform::normal) this->transform = Transform::normal;
      if (transform & wl::output_transform::_90) this->transform = Transform::_90;
      if (transform & wl::output_transform::_180) this->transform = Transform::_180;
      if (transform & wl::output_transform::_270) this->transform = Transform::_270;
      if (transform & wl::output_transform::flipped) this->transform = Transform::flipped;
      if (transform & wl::output_transform::flipped_90) this->transform = Transform::flipped_90;
      if (transform & wl::output_transform::flipped_180) this->transform = Transform::flipped_180;
      if (transform & wl::output_transform::flipped_270) this->transform = Transform::flipped_270;
    };
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
    if (p != logical_position) send_sway_cmd("output '{}' pos {} {}", name, p.x, p.y);
  }

  void Output::set_size(Size s)
  {
    if (s != logical_size) send_sway_cmd("output '{}' size '{} {}'", name, s.width, s.height);
  }

  void Output::toggle()
  {
    send_sway_cmd("output '{}' toggle", name);
  }

  void Output::set_mode(Mode m)
  {
    send_sway_cmd("output '{}' mode {}x{}@{}Hz", name, m.width, m.height, m.refresh);
  }

  auto Output::set_transform(Transform t) -> void
  {
    send_sway_cmd("output '{}' transform {}", name, to_string(t));
  }

  std::string to_string(Transform t)
  {
    switch (t) {
    case Transform::normal: return "normal";
    case Transform::_90: return "90";
    case Transform::_180: return "180";
    case Transform::_270: return "270";
    case Transform::flipped: return "flipped";
    case Transform::flipped_90: return "flipped_90";
    case Transform::flipped_180: return "flipped_180";
    case Transform::flipped_270: return "flipped_270";
    }
    return NULL;
  }

  Transform transform_from_string(std::string s)
  {
    if (s == "normal") return Transform::normal;
    if (s == "90") return Transform::_90;
    if (s == "180") return Transform::_180;
    if (s == "270") return Transform::_270;
    if (s == "flipped") return Transform::flipped;
    if (s == "flipped_90") return Transform::flipped_90;
    if (s == "flipped_180") return Transform::flipped_180;
    if (s == "flipped_270") return Transform::flipped_270;
    throw std::logic_error("Unreachable");
  }
} // namespace cloth::outputs
