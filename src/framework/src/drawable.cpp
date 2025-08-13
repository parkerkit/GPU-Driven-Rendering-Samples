#include "drawable.h"

using namespace fw;

Drawable::Drawable(GLenum primitive) : m_primitive{primitive} {}

void Drawable::SetVertexCount(GLsizei count) noexcept { m_count = count; }

VertexArray& Drawable::GetVertexArray() noexcept { return m_vertex_array; }

const VertexArray& Drawable::GetVertexArray() const noexcept { return m_vertex_array; }

void Drawable::Draw() const noexcept {
  m_vertex_array.Bind();

  if (m_index_buffer.has_value()) {
    glDrawElements(m_primitive, m_count, m_index_type, nullptr);

    return;
  }

  glDrawArrays(m_primitive, 0, m_count);
}

void Drawable::DrawInstanced(GLsizei instance_count) const noexcept {
  m_vertex_array.Bind();

  if (m_index_buffer.has_value()) {
    glDrawElementsInstanced(m_primitive, m_count, m_index_type, nullptr, instance_count);
    return;
  }

  glDrawArraysInstanced(m_primitive, 0, m_count, instance_count);
}

void Drawable::DrawInstanced(GLsizei instance_count, GLuint base_instance) const noexcept {
  m_vertex_array.Bind();

  if (m_index_buffer.has_value()) {
    glDrawElementsInstancedBaseInstance(m_primitive, m_count, m_index_type, nullptr, instance_count, base_instance);
    return;
  }

  glDrawArraysInstancedBaseInstance(m_primitive, 0, m_count, instance_count, base_instance);
}

void Drawable::DrawIndirect(GLuint indirect_buffer, GLintptr byte_offset) const noexcept {
  m_vertex_array.Bind();

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer);

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  const void* offset = reinterpret_cast<const void*>(byte_offset);

  if (m_index_buffer.has_value()) {
    glDrawElementsIndirect(m_primitive, m_index_type, offset);
    return;
  }

  glDrawArraysIndirect(m_primitive, offset);
}

void Drawable::DrawMultiIndirect(GLuint indirect_buffer, GLsizei draw_count, GLintptr byte_offset,
                                 GLsizei stride) const noexcept {
  m_vertex_array.Bind();

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer);

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  const void* offset = reinterpret_cast<const void*>(byte_offset);

  if (m_index_buffer.has_value()) {
    glMultiDrawElementsIndirect(m_primitive, m_index_type, offset, draw_count, stride);
    return;
  }

  glMultiDrawArraysIndirect(m_primitive, offset, draw_count, stride);
}
