#pragma once

#include <glad/gl.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>

namespace fw {
struct ShaderPaths {
  std::filesystem::path vertex{};
  std::filesystem::path fragment{};
  std::optional<std::filesystem::path> geometry{};
  std::optional<std::filesystem::path> tess_control{};
  std::optional<std::filesystem::path> tess_eval{};
};

class Shader {
public:
  virtual ~Shader() noexcept;

  Shader(const Shader& other) = delete;
  Shader& operator=(const Shader& other) = delete;

  Shader(Shader&& other) noexcept;
  Shader& operator=(Shader&& other) noexcept;

  GLuint GetHandle() const noexcept;

  void Bind() const noexcept;
  void UnBind() const noexcept;

  void SetMat4(std::string_view name, const glm::mat4& value) const noexcept;
  void SetVec3(std::string_view name, const glm::vec3& value) const noexcept;
  void SetVec2(std::string_view name, const glm::vec2& value) const noexcept;
  void SetFloat(std::string_view name, float value) const noexcept;
  void SetInt(std::string_view name, int value) const noexcept;
  void SetUInt(std::string_view name, uint32_t value) const noexcept;

protected:
  Shader();

  static GLuint CompileShader(GLenum shader_type, const std::filesystem::path& source_path);
  static std::string GetShaderLog(GLuint shader);
  static std::string GetProgramLog(GLuint program);
  [[nodiscard]] GLint GetUniformLocation(std::string_view name) const;

private:
  GLuint m_handle{};
};

class GraphicsShader final : public Shader {
public:
  explicit GraphicsShader(const ShaderPaths& paths);
};

class ComputeShader final : public Shader {
public:
  explicit ComputeShader(const std::filesystem::path& source_path);

  void Dispatch(GLuint group_x, GLuint group_y = 1, GLuint group_z = 1) const noexcept;
};
}  // namespace fw
