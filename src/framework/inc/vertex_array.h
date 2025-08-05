#pragma once

#include <glad/gl.h>

#include <utility>

#include "buffer.h"

namespace fw {
class VertexArray {
public:
  VertexArray();
  ~VertexArray() noexcept;

  VertexArray(const VertexArray&) = delete;
  VertexArray& operator=(const VertexArray&) = delete;

  VertexArray(VertexArray&& other) noexcept;
  VertexArray& operator=(VertexArray&& other) noexcept;

  void SetVertexBuffer(GLuint binding_index, const Buffer& buffer, GLintptr byte_offset, GLsizei stride) const noexcept;

  void SetIndexBuffer(const Buffer& buffer) const noexcept;

  void SetAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                    GLuint relative_offset, bool normalized = false) const noexcept;

  void SetIntegerAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                           GLuint relative_offset) const noexcept;

  void SetDoubleAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                          GLuint relative_offset) const noexcept;

  void SetAttributeBinding(GLuint attribute_index, GLuint binding_index) const noexcept;

  void EnableAttribute(GLuint attribute_index) const noexcept;

  void DisableAttribute(GLuint attribute_index) const noexcept;

  void SetBindingDivisor(GLuint binding_index, GLuint divisor) const noexcept;

  void Bind() const noexcept;

  [[nodiscard]]
  GLuint GetHandle() const noexcept;

private:
  GLuint m_handle{};
};
}  // namespace fw
