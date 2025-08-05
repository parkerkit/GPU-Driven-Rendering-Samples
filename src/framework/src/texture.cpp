#include "texture.h"

using namespace fw;

Texture::Texture(GLenum target) : m_target{target} { glCreateTextures(target, 1, &m_handle); }

Texture::~Texture() noexcept {
  if (m_handle != 0) {
    glDeleteTextures(1, &m_handle);
  }
}

Texture::Texture(Texture&& other) noexcept
    : m_handle{std::exchange(other.m_handle, 0)}, m_target{std::exchange(other.m_target, 0)} {}

Texture& Texture::operator=(Texture&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (m_handle != 0) {
    glDeleteTextures(1, &m_handle);
  }

  m_handle = std::exchange(other.m_handle, 0);
  m_target = std::exchange(other.m_target, 0);

  return *this;
}

void Texture::Storage1D(GLsizei levels, GLenum internal_format, GLsizei width) const noexcept {
  glTextureStorage1D(m_handle, levels, internal_format, width);
}

void Texture::Storage2D(GLsizei levels, GLenum internal_format, GLsizei width, GLsizei height) const noexcept {
  glTextureStorage2D(m_handle, levels, internal_format, width, height);
}

void Texture::Storage3D(GLsizei levels, GLenum internal_format, GLsizei width, GLsizei height,
                        GLsizei depth) const noexcept {
  glTextureStorage3D(m_handle, levels, internal_format, width, height, depth);
}

void Texture::SetSubImage1D(GLint level, GLint x_offset, GLsizei width, GLenum format, GLenum type,
                            const void* data) const noexcept {
  glTextureSubImage1D(m_handle, level, x_offset, width, format, type, data);
}

void Texture::SetSubImage2D(GLint level, GLint x_offset, GLint y_offset, GLsizei width, GLsizei height, GLenum format,
                            GLenum type, const void* data) const noexcept {
  glTextureSubImage2D(m_handle, level, x_offset, y_offset, width, height, format, type, data);
}

void Texture::SetSubImage3D(GLint level, GLint x_offset, GLint y_offset, GLint z_offset, GLsizei width, GLsizei height,
                            GLsizei depth, GLenum format, GLenum type, const void* data) const noexcept {
  glTextureSubImage3D(m_handle, level, x_offset, y_offset, z_offset, width, height, depth, format, type, data);
}

void Texture::SetParameter(GLenum name, GLint value) const noexcept { glTextureParameteri(m_handle, name, value); }

void Texture::SetParameter(GLenum name, GLfloat value) const noexcept { glTextureParameterf(m_handle, name, value); }

void Texture::SetBorderColor(std::span<const GLfloat, 4> color) const noexcept {
  glTextureParameterfv(m_handle, GL_TEXTURE_BORDER_COLOR, color.data());
}

void Texture::GenerateMipmap() const noexcept { glGenerateTextureMipmap(m_handle); }

void Texture::BindUnit(GLuint unit) const noexcept { glBindTextureUnit(unit, m_handle); }

GLuint Texture::GetHandle() const noexcept { return m_handle; }

GLenum Texture::GetTarget() const noexcept { return m_target; }
