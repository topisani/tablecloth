#include "decoration.hpp"

#include "desktop.hpp"
#include "output.hpp"
#include "view.hpp"

#include "render.hpp"
#include "render_utils.hpp"

#include <GLES2/gl2.h>

namespace cloth {

  Decoration::Decoration(View& v) : view(v) {}

  auto Decoration::update() -> void {}

  auto Decoration::set_visible(bool visible) -> void
  {
    view.damage_whole();
    _visible = visible;
    if (visible) {
      _border_width = 4;
      _titlebar_height = 0;
    } else {
      _border_width = 0;
      _titlebar_height = 0;
    }
    view.damage_whole();
  }

  auto Decoration::box() const noexcept -> wlr::box_t
  {
    auto box = view.get_box();
    if (_visible) {
      box.x -= _border_width;
      box.y -= (_border_width + _titlebar_height);
      box.width += _border_width * 2;
      box.height += (_border_width * 2 + _titlebar_height);
    }
    return box;
  }

  auto Decoration::part_at(double sx, double sy) const noexcept -> DecoPart
  {
    if (!_visible) {
      return DecoPart::none;
    }

    int sw = view.wlr_surface->current.width;
    int sh = view.wlr_surface->current.height;
    int bw = _border_width;
    int titlebar_h = _titlebar_height;

    if (sx > 0 && sx < sw && sy < 0 && sy > -_titlebar_height) {
      return DecoPart::titlebar;
    }

    DecoPart parts = DecoPart::none;
    if (sy >= -(titlebar_h + bw) && sy <= sh + bw) {
      if (sx < 0 && sx > -bw) {
        parts |= DecoPart::left_border;
      } else if (sx > sw && sx < sw + bw) {
        parts |= DecoPart::right_border;
      }
    }

    if (sx >= -bw && sx <= sw + bw) {
      if (sy > sh && sy <= sh + bw) {
        parts |= DecoPart::bottom_border;
      } else if (sy >= -(titlebar_h + bw) && sy < 0) {
        parts |= DecoPart::top_border;
      }
    }

    // TODO corners
    return parts;
  }

  auto Decoration::damage() -> void
  {
    view.damage_whole();
  }

  auto Decoration::shadow_offset() -> float
  {
    return _shadow_offset * (view.active ? 2 : 1);
  }

  auto Decoration::shadow_radius() -> float
  {
    return _shadow_radius * (view.active ? 2 : 1);
  }

  auto Decoration::color() const -> std::array<float, 4>
  {
    if (view.active)
      return {0x00 / 255.f, 0x59 / 255.f, 0x73 / 255.f, 1.f};
    else
      return {0.2, 0.2, 0.23, 1.f};
  }

  // Rendering

  namespace render {

    auto shadow_shader()
    {
      static Shader shader = {R"END(
uniform mat3 proj;
uniform vec4 color;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec4 v_color;
varying vec2 v_texcoord;

void main() {
	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
	v_color = color;
	v_texcoord = texcoord;
}
)END",
                              R"END(
precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;
uniform float aspect;
uniform float radius;

void main()
{
  vec2 pos = v_texcoord;

  //pos.x /= aspect;

  float dx = smoothstep(0.0, radius * aspect, min(pos.x, 1.0 - pos.x));
  float dy = smoothstep(0.0, radius, min(pos.y, 1.0 - pos.y));

  float alpha = dx * dy;
  gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
    )END"};
      return shader;
    } // namespace render

    static void draw_quad(void)
    {
      GLfloat verts[] = {
        1, 0, // top right
        0, 0, // top left
        1, 1, // bottom right
        0, 1, // bottom left
      };
      GLfloat texcoord[] = {
        1, 0, // top right
        0, 0, // top left
        1, 1, // bottom right
        0, 1, // bottom left
      };

      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texcoord);

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }

    auto Context::draw_shadow(wlr::box_t box,
                              float rotation,
                              float alpha,
                              float radius,
                              float offset) -> void
    {
      box.x += offset - radius / 2.f;
      box.y += offset - radius / 2.f;
      box.height += radius;
      box.width += radius;

      wlr::box_t rotated;
      wlr_box_rotated_bounds(&box, rotation, &rotated);

      pixman_region32_t damage;
      pixman_region32_init(&damage);
      pixman_region32_union_rect(&damage, &damage, rotated.x, rotated.y, rotated.width,
                                 rotated.height);
      pixman_region32_intersect(&damage, &damage, &pixman_damage);
      bool damaged = pixman_region32_not_empty(&damage);
      if (damaged) {
        float matrix[9];
        wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, rotation,
                               output.wlr_output.transform_matrix);

        int nrects;
        pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
        for (int i = 0; i < nrects; ++i) {
          scissor_output(output, &rects[i]);

          float transposition[9];
          wlr_matrix_transpose(transposition, matrix);

          shadow_shader().use();

          // update the uniform color
          glUniformMatrix3fv(glGetUniformLocation(shadow_shader().ID, "proj"), 1, false,
                             transposition);
          shadow_shader().set("color", 0.f, 0.f, 0.f, alpha);
          shadow_shader().set("aspect", box.height / (float) box.width);
          shadow_shader().set("radius", radius / (float) box.height);
          draw_quad();
        }
      }

      pixman_region32_fini(&damage);
    }

    static wlr::box_t get_decoration_box(View& view, Output& output)
    {
      wlr::output_t& wlr_output = output.wlr_output;

      wlr::box_t deco_box = view.deco.box();
      double sx = deco_box.x - view.x;
      double sy = deco_box.y - view.y;
      rotate_child_position(sx, sy, deco_box.width, deco_box.height,
                            view.wlr_surface->current.width, view.wlr_surface->current.height,
                            view.rotation);
      double x = sx + view.x;
      double y = sy + view.y;

      wlr::box_t box;

      wlr_output_layout_output_coords(output.desktop.layout, &wlr_output, &x, &y);

      box.x = x * wlr_output.scale;
      box.y = y * wlr_output.scale;
      box.width = deco_box.width * wlr_output.scale;
      box.height = deco_box.height * wlr_output.scale;
      return box;
    }


    auto Context::render_decorations(View& view, RenderData& data) -> void
    {
      if (view.maximized || view.wlr_surface == nullptr) {
        return;
      }

      wlr::renderer_t* renderer = wlr_backend_get_renderer(output.wlr_output.backend);
      assert(renderer);

      wlr::box_t box = get_decoration_box(view, output);
      double x_scale = data.layout.width / double(view.width);
      double y_scale = data.layout.height / double(view.height);
      box.x = data.layout.x + (box.x - view.x) * x_scale;
      box.y = data.layout.y + (box.y - view.y) * y_scale;
      box.width *= x_scale;
      box.height *= y_scale;

      draw_shadow(box, view.rotation, 0.5 * data.alpha, view.deco.shadow_radius(),
                  view.deco.shadow_offset());

      if (!view.deco.is_visible()) return;

      wlr::box_t rotated;
      wlr_box_rotated_bounds(&box, view.rotation, &rotated);

      pixman_region32_t damage;
      pixman_region32_init(&damage);
      pixman_region32_union_rect(&damage, &damage, rotated.x, rotated.y, rotated.width,
                                 rotated.height);
      pixman_region32_intersect(&damage, &damage, &pixman_damage);
      bool damaged = pixman_region32_not_empty(&damage);
      if (damaged) {
        float matrix[9];
        wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, view.rotation,
                               output.wlr_output.transform_matrix);
        int nrects;
        pixman_box32_t* rects = pixman_region32_rectangles(&damage, &nrects);
        for (int i = 0; i < nrects; ++i) {
          scissor_output(output, &rects[i]);
          auto color = view.deco.color();
          color[3] *= data.alpha;
          wlr_render_quad_with_matrix(renderer, color.data(), matrix);
        }
      }

      pixman_region32_fini(&damage);
    }

    auto Context::damage_whole_decoration(View& view) -> void
    {
      if (!view.deco.is_visible() && view.deco.shadow_radius() == 0) {
        return;
      }

      wlr::box_t box = get_decoration_box(view, output);

      float offset = view.deco.shadow_offset() * (view.active ? 1.f : 4.f);
      float radius = view.deco.shadow_radius() * (view.active ? 1.f : 4.f);

      float diff = std::min(0.f, offset - radius / 2.f);

      box.x += diff;
      box.y += diff;
      box.height += radius;
      box.width += radius;

      wlr_box_rotated_bounds(&box, view.rotation, &box);

      wlr_output_damage_add_box(damage, &box);
    }

  } // namespace render

} // namespace cloth
