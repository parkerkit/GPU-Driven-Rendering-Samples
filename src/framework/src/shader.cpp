#include "shader.h"

#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace fw;

namespace {
constexpr GLuint k_invalid_handle{0};

// Keep stage ownership local until linking completes; the Shader base owns the program separately.
class ShaderStage final {
public:
  explicit ShaderStage(GLuint handle) noexcept : m_handle{handle} {}

  ~ShaderStage() noexcept {
    if (m_program != k_invalid_handle) {
      glDetachShader(m_program, m_handle);
    }
    if (m_handle != k_invalid_handle) {
      glDeleteShader(m_handle);
    }
  }

  ShaderStage(const ShaderStage&) = delete;
  ShaderStage& operator=(const ShaderStage&) = delete;
  ShaderStage(ShaderStage&&) = delete;
  ShaderStage& operator=(ShaderStage&&) = delete;

  void AttachTo(GLuint program) noexcept {
    glAttachShader(program, m_handle);
    m_program = program;
  }

  [[nodiscard]] GLuint GetHandle() const noexcept { return m_handle; }

  // CompileShader transfers an unattached stage to its caller after successful compilation.
  [[nodiscard]] GLuint Release() noexcept { return std::exchange(m_handle, k_invalid_handle); }

private:
  GLuint m_handle{};
  GLuint m_program{};
};

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
  const auto source_code{ReadFileToString(source_path)};
  ShaderStage shader{glCreateShader(shader_type)};
  if (shader.GetHandle() == k_invalid_handle) {
    throw std::runtime_error{"Failed to create shader: " + source_path.string()};
  }
  const char* src_ptr{source_code.c_str()};
  const auto src_len{static_cast<int>(source_code.size())};

  glShaderSource(shader.GetHandle(), 1, &src_ptr, &src_len);
  glCompileShader(shader.GetHandle());

  GLint compile_status{};
  glGetShaderiv(shader.GetHandle(), GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE) {
    const std::string log{GetShaderLog(shader.GetHandle())};
    throw std::runtime_error{"Failed to compile shader: " + source_path.string() + "\n" + log};
  }

  return shader.Release();
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

Shader::Shader() : m_handle(glCreateProgram()) {
  if (m_handle == k_invalid_handle) {
    throw std::runtime_error{"Failed to create shader program."};
  }
}

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

void Shader::SetMat4(std::string_view name, const glm::mat4& value) const {
  glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3(std::string_view name, const glm::vec3& value) const {
  glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec2(std::string_view name, const glm::vec2& value) const {
  glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetFloat(std::string_view name, float value) const { glUniform1f(GetUniformLocation(name), value); }

void Shader::SetInt(std::string_view name, int value) const { glUniform1i(GetUniformLocation(name), value); }

void Shader::SetUInt(std::string_view name, uint32_t value) const { glUniform1ui(GetUniformLocation(name), value); }

GraphicsShader::GraphicsShader(const ShaderPaths& paths) {
  ShaderStage vertex_shader{CompileShader(GL_VERTEX_SHADER, paths.vertex)};
  ShaderStage fragment_shader{CompileShader(GL_FRAGMENT_SHADER, paths.fragment)};

  vertex_shader.AttachTo(GetHandle());
  fragment_shader.AttachTo(GetHandle());
  glLinkProgram(GetHandle());

  GLint link_status{};
  glGetProgramiv(GetHandle(), GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    const std::string log{GetProgramLog(GetHandle())};
    throw std::runtime_error{"Failed to link shader program:\n" + log};
  }
}

ComputeShader::ComputeShader(const std::filesystem::path& source_path) {
  ShaderStage compute_shader{CompileShader(GL_COMPUTE_SHADER, source_path)};

  compute_shader.AttachTo(GetHandle());
  glLinkProgram(GetHandle());

  GLint link_status{};
  glGetProgramiv(GetHandle(), GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    const std::string log{GetProgramLog(GetHandle())};
    throw std::runtime_error{"Failed to link shader program:\n" + log};
  }
}

void ComputeShader::Dispatch(GLuint group_x, GLuint group_y, GLuint group_z) const noexcept {
  glDispatchCompute(group_x, group_y, group_z);
}

MeshShader::MeshShader(const MeshShaderPaths& paths) {
  ShaderStage mesh_shader{CompileShader(GL_MESH_SHADER_NV, paths.mesh)};
  ShaderStage fragment_shader{CompileShader(GL_FRAGMENT_SHADER, paths.fragment)};

  mesh_shader.AttachTo(GetHandle());
  fragment_shader.AttachTo(GetHandle());
  glLinkProgram(GetHandle());

  GLint link_status{};
  glGetProgramiv(GetHandle(), GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    const std::string log{GetProgramLog(GetHandle())};
    throw std::runtime_error{"Failed to link shader program:\n" + log};
  }
}
