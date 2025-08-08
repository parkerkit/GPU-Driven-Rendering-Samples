#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "buffer.h"
#include "vertex_array.h"

namespace fw {
class Drawable {
public:
  explicit Drawable(GLenum primitive = GL_TRIANGLES);

  ~Drawable() noexcept = default;

  Drawable(const Drawable&) = delete;
  Drawable& operator=(const Drawable&) = delete;

  Drawable(Drawable&&) noexcept = default;
  Drawable& operator=(Drawable&&) noexcept = default;

  template <typename T, std::size_t Extent>
    requires(std::is_trivially_copyable_v<std::remove_cv_t<T>> && !std::is_volatile_v<T>)
  void AddVertexBuffer(GLuint binding_index, std::span<T, Extent> data, GLsizei stride, GLbitfield storage_flags = 0) {
    using ValueType = std::remove_cv_t<T>;

    const std::span<const ValueType> const_data{data.data(), data.size()};

    m_vertex_buffers.push_back(VertexBufferBinding{binding_index, Buffer{const_data, storage_flags}});

    const auto& vertex_buffer = m_vertex_buffers.back();

    m_vertex_array.SetVertexBuffer(binding_index, vertex_buffer.buffer, 0, stride);
  }

  template <typename T, std::size_t Extent>
    requires(std::is_trivially_copyable_v<std::remove_cv_t<T>> && !std::is_volatile_v<T>)
  void SetIndexBuffer(std::span<T, Extent> data, GLbitfield storage_flags = 0) {
    using IndexType = std::remove_cv_t<T>;

    static_assert(std::is_same_v<IndexType, std::uint8_t> || std::is_same_v<IndexType, std::uint16_t> ||
                    std::is_same_v<IndexType, std::uint32_t>,
                  "Index buffer type must be uint8_t, uint16_t or uint32_t.");

    if constexpr (std::is_same_v<IndexType, std::uint8_t>) {
      m_index_type = GL_UNSIGNED_BYTE;
    } else if constexpr (std::is_same_v<IndexType, std::uint16_t>) {
      m_index_type = GL_UNSIGNED_SHORT;
    } else {
      m_index_type = GL_UNSIGNED_INT;
    }

    const std::span<const IndexType> const_data{data.data(), data.size()};

    m_index_buffer.emplace(const_data, storage_flags);

    m_vertex_array.SetIndexBuffer(*m_index_buffer);

    m_count = static_cast<GLsizei>(data.size());
  }

  void SetVertexCount(GLsizei count) noexcept;

  [[nodiscard]]
  VertexArray& GetVertexArray() noexcept;

  [[nodiscard]]
  const VertexArray& GetVertexArray() const noexcept;

  void Draw() const noexcept;
  void DrawInstanced(GLsizei instance_count) const noexcept;
  void DrawInstanced(GLsizei instance_count, GLuint base_instance) const noexcept;
  void DrawIndirect(GLuint indirect_buffer, GLintptr offset) const noexcept;

private:
  struct VertexBufferBinding {
    GLuint binding{};
    Buffer buffer;
  };

private:
  std::vector<VertexBufferBinding> m_vertex_buffers{};

  std::optional<Buffer> m_index_buffer{};

  VertexArray m_vertex_array{};

  GLenum m_primitive;
  GLenum m_index_type{GL_UNSIGNED_INT};

  GLsizei m_count{};
};
}  // namespace fw
