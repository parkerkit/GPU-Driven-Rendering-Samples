#include "vertex_array.h"

using namespace fw;

VertexArray::VertexArray() { glCreateVertexArrays(1, &m_handle); }

VertexArray::~VertexArray() noexcept {
  if (m_handle != 0) {
    glDeleteVertexArrays(1, &m_handle);
  }
}

VertexArray::VertexArray(VertexArray&& other) noexcept : m_handle{std::exchange(other.m_handle, 0)} {}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (m_handle != 0) {
    glDeleteVertexArrays(1, &m_handle);
  }

  m_handle = std::exchange(other.m_handle, 0);

  return *this;
}

void VertexArray::SetVertexBuffer(GLuint binding_index, const Buffer& buffer, GLintptr byte_offset,
                                  GLsizei stride) const noexcept {
  glVertexArrayVertexBuffer(m_handle, binding_index, buffer.GetHandle(), byte_offset, stride);
}

void VertexArray::SetIndexBuffer(const Buffer& buffer) const noexcept {
  glVertexArrayElementBuffer(m_handle, buffer.GetHandle());
}

void VertexArray::SetAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                               GLuint relative_offset, bool normalized) const noexcept {
  glVertexArrayAttribFormat(m_handle, attribute_index, component_count, type, normalized ? GL_TRUE : GL_FALSE,
                            relative_offset);

  glVertexArrayAttribBinding(m_handle, attribute_index, binding_index);

  glEnableVertexArrayAttrib(m_handle, attribute_index);
}

void VertexArray::SetIntegerAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                                      GLuint relative_offset) const noexcept {
  glVertexArrayAttribIFormat(m_handle, attribute_index, component_count, type, relative_offset);

  glVertexArrayAttribBinding(m_handle, attribute_index, binding_index);

  glEnableVertexArrayAttrib(m_handle, attribute_index);
}

void VertexArray::SetDoubleAttribute(GLuint attribute_index, GLuint binding_index, GLint component_count, GLenum type,
                                     GLuint relative_offset) const noexcept {
  glVertexArrayAttribLFormat(m_handle, attribute_index, component_count, type, relative_offset);

  glVertexArrayAttribBinding(m_handle, attribute_index, binding_index);

  glEnableVertexArrayAttrib(m_handle, attribute_index);
}

void VertexArray::SetAttributeBinding(GLuint attribute_index, GLuint binding_index) const noexcept {
  glVertexArrayAttribBinding(m_handle, attribute_index, binding_index);
}

void VertexArray::EnableAttribute(GLuint attribute_index) const noexcept {
  glEnableVertexArrayAttrib(m_handle, attribute_index);
}

void VertexArray::DisableAttribute(GLuint attribute_index) const noexcept {
  glDisableVertexArrayAttrib(m_handle, attribute_index);
}

void VertexArray::SetBindingDivisor(GLuint binding_index, GLuint divisor) const noexcept {
  glVertexArrayBindingDivisor(m_handle, binding_index, divisor);
}

void VertexArray::Bind() const noexcept { glBindVertexArray(m_handle); }

GLuint VertexArray::GetHandle() const noexcept { return m_handle; }
