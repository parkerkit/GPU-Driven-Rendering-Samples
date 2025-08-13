#include "shader.h"

#include <fstream>
#include <glm/gtc/type_ptr.hpp>

using namespace fw;

namespace {
constexpr GLuint k_invalid_handle{0};

std::string ReadFileToString(const std::filesystem::path& file_path) {
  std::ifstream file(file_path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + file_path.string());
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}
}  // namespace

GLuint Shader::CompileShader(GLenum shader_type, const std::filesystem::path& source_path) {
  const GLuint shader{glCreateShader(shader_type)};

  const auto source_code{ReadFileToString(source_path)};
  const char* src_ptr{source_code.c_str()};
  const auto src_len{static_cast<int>(source_code.size())};

  glShaderSource(shader, 1, &src_ptr, &src_len);
  glCompileShader(shader);

  GLint compile_status{};
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE) {
    const std::string log{GetShaderLog(shader)};
    glDeleteShader(shader);
    throw std::runtime_error{"Failed to compile shader: " + source_path.string() + "\n" + log};
  }

  return shader;
}

std::string Shader::GetShaderLog(GLuint shader) {
  GLint log_length{};
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetShaderInfoLog(shader, log_length, nullptr, log.data());
  return log;
}

std::string Shader::GetProgramLog(GLuint program) {
  GLint log_length{};
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetProgramInfoLog(program, log_length, nullptr, log.data());
  return log;
}

Shader::Shader() : m_handle(glCreateProgram()) {}

Shader::~Shader() noexcept {
  if (m_handle != k_invalid_handle) {
    glDeleteProgram(m_handle);
  }
}

Shader::Shader(Shader&& other) noexcept : m_handle{other.m_handle} { other.m_handle = k_invalid_handle; }

Shader& Shader::operator=(Shader&& other) noexcept {
  if (this != &other) {
    if (m_handle != k_invalid_handle) {
      glDeleteProgram(m_handle);
    }
    m_handle = other.m_handle;
    other.m_handle = k_invalid_handle;
  }
  return *this;
}

GLuint Shader::GetHandle() const noexcept { return m_handle; }

void Shader::Bind() const noexcept { glUseProgram(m_handle); }

void Shader::UnBind() const noexcept { glUseProgram(k_invalid_handle); }

[[nodiscard]] GLint Shader::GetUniformLocation(std::string_view name) const {
  const GLint location{glGetUniformLocation(m_handle, std::string{name}.c_str())};
  if (location < 0) {
    throw std::runtime_error{"Uniform not found: " + std::string{name}};
  }
  return location;
}

void Shader::SetMat4(std::string_view name, const glm::mat4& value) const noexcept {
  glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3(std::string_view name, const glm::vec3& value) const noexcept {
  glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec2(std::string_view name, const glm::vec2& value) const noexcept {
  glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetFloat(std::string_view name, float value) const noexcept {
  glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetInt(std::string_view name, int value) const noexcept { glUniform1i(GetUniformLocation(name), value); }

void Shader::SetUInt(std::string_view name, uint32_t value) const noexcept {
  glUniform1ui(GetUniformLocation(name), value);
}

GraphicsShader::GraphicsShader(const ShaderPaths& paths) {
  const GLuint vertex_shader{CompileShader(GL_VERTEX_SHADER, paths.vertex)};
  const GLuint fragment_shader{CompileShader(GL_FRAGMENT_SHADER, paths.fragment)};

  glAttachShader(GetHandle(), vertex_shader);
  glAttachShader(GetHandle(), fragment_shader);
  glLinkProgram(GetHandle());
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint link_status{};
  glGetProgramiv(GetHandle(), GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    const std::string log{GetProgramLog(GetHandle())};
    glDeleteProgram(GetHandle());
    throw std::runtime_error{"Failed to link shader program:\n" + log};
  }
}

ComputeShader::ComputeShader(const std::filesystem::path& source_path) {
  const GLuint compute_shader{CompileShader(GL_COMPUTE_SHADER, source_path)};

  glAttachShader(GetHandle(), compute_shader);
  glLinkProgram(GetHandle());
  glDeleteShader(compute_shader);

  GLint link_status{};
  glGetProgramiv(GetHandle(), GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    const std::string log{GetProgramLog(GetHandle())};
    glDeleteProgram(GetHandle());
    throw std::runtime_error{"Failed to link shader program:\n" + log};
  }
}

void ComputeShader::Dispatch(GLuint group_x, GLuint group_y, GLuint group_z) const noexcept {
  glDispatchCompute(group_x, group_y, group_z);
}
