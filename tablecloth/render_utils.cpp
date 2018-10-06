#include "render_utils.hpp"

#include <GLES2/gl2.h>

namespace cloth::render {

  Shader::Shader(const std::string& vertex_shader, const std::string& frag_shader)
  {
    const char* vcode = vertex_shader.c_str();
    const char* fcode = frag_shader.c_str();

    unsigned int vertex, fragment;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vcode, nullptr);
    glCompileShader(vertex);
    check_compilation(vertex, "VERTEX");

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fcode, NULL);
    glCompileShader(fragment);
    check_compilation(fragment, "FRAGMENT");

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    check_compilation(ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
  }

  void Shader::use()
  {
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*) &prevID);
    glUseProgram(ID);
  }

  void Shader::restore()
  {
    glUseProgram(prevID);
  }

  void Shader::set(const std::string& name, bool value) const
  {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int) value);
  }

  void Shader::set(const std::string& name, int value) const
  {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
  }

  void Shader::set(const std::string& name, float value) const
  {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
  }

  void Shader::set(const std::string& name, float v1, float v2) const
  {
    glUniform2f(glGetUniformLocation(ID, name.c_str()), v1, v2);
  }

  void Shader::set(const std::string& name, float v1, float v2, float v3) const
  {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3);
  }

  void Shader::set(const std::string& name, float v1, float v2, float v3, float v4) const
  {
    glUniform4f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3, v4);
  }

  void Shader::check_compilation(unsigned int shader, std::string type)
  {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
      glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
      if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                  << infoLog << "\n -- --------------------------------------------------- -- "
                  << std::endl;
      }
    } else {
      glGetProgramiv(shader, GL_LINK_STATUS, &success);
      if (!success) {
        glGetProgramInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                  << infoLog << "\n -- --------------------------------------------------- -- "
                  << std::endl;
      }
    }
  }
} // namespace cloth::render
