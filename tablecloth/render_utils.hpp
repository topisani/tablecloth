#pragma once

#include <string>
#include <string_view>

#include "render.hpp"
#include "wlroots.hpp"

namespace cloth::render {

  struct Shader {
    /// the program ID
    unsigned int ID;

    Shader(const std::string& vertex_shader, const std::string& frag_shader);

    /// use/activate the shader
    void use();
    void restore();

    void set(const std::string& name, bool value) const;
    void set(const std::string& name, int value) const;
    void set(const std::string& name, float value) const;
    void set(const std::string& name, float v1, float v2) const;
    void set(const std::string& name, float v1, float v2, float v3) const;
    void set(const std::string& name, float v1, float v2, float v3, float v4) const;

  private:
    void check_compilation(unsigned int shader, std::string type);

    unsigned int prevID;
  };

  /**
   * Rotate a child's position relative to a parent. The parent size is (pw, ph),
   * the child position is (*sx, *sy) and its size is (sw, sh).
   */
  auto rotate_child_position(double& sx,
                             double& sy,
                             double sw,
                             double sh,
                             double pw,
                             double ph,
                             float rotation) -> void;


  auto get_layout_position(const LayoutData& data,
                           double& lx,
                           double& ly,
                           const wlr::surface_t& surface,
                           int sx,
                           int sy) -> void;

  auto has_standalone_surface(View& view) -> bool;

  /**
   * Checks whether a surface at (lx, ly) intersects an output. If `box` is not
   * nullptr, it populates it with the surface box in the output, in output-local
   * coordinates.
   */
  auto surface_intersect_output(wlr::surface_t& surface,
                                wlr::output_layout_t& output_layout,
                                wlr::output_t& wlr_output,
                                double lx,
                                double ly,
                                float rotation,
                                wlr::box_t* box) -> bool;

  auto scissor_output(Output& output, pixman_box32_t* rect) -> void;

} // namespace cloth::render
