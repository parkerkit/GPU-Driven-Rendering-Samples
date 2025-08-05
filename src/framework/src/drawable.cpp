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
