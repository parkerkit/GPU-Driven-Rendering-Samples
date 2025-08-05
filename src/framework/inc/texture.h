#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <span>
#include <utility>

namespace fw {
class Texture {
public:
  explicit Texture(GLenum target);

  ~Texture() noexcept;

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;

  void Storage1D(GLsizei levels, GLenum internal_format, GLsizei width) const noexcept;

  void Storage2D(GLsizei levels, GLenum internal_format, GLsizei width, GLsizei height) const noexcept;

  void Storage3D(GLsizei levels, GLenum internal_format, GLsizei width, GLsizei height, GLsizei depth) const noexcept;

  void SetSubImage1D(GLint level, GLint x_offset, GLsizei width, GLenum format, GLenum type,
                     const void* data) const noexcept;

  void SetSubImage2D(GLint level, GLint x_offset, GLint y_offset, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, const void* data) const noexcept;

  void SetSubImage3D(GLint level, GLint x_offset, GLint y_offset, GLint z_offset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum format, GLenum type, const void* data) const noexcept;

  void SetParameter(GLenum name, GLint value) const noexcept;

  void SetParameter(GLenum name, GLfloat value) const noexcept;

  void SetBorderColor(std::span<const GLfloat, 4> color) const noexcept;

  void GenerateMipmap() const noexcept;

  void BindUnit(GLuint unit) const noexcept;

  [[nodiscard]]
  GLuint GetHandle() const noexcept;

  [[nodiscard]]
  GLenum GetTarget() const noexcept;

private:
  GLuint m_handle{};
  GLenum m_target{};
};
}  // namespace fw
