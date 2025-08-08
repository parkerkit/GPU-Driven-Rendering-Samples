#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace fw {
class Buffer {
public:
  Buffer();

  explicit Buffer(GLsizeiptr size, GLbitfield storage_flags = 0);

  explicit Buffer(std::span<const std::byte> data, GLbitfield storage_flags = 0);

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  explicit Buffer(std::span<const T> data, GLbitfield storage_flags = 0) : Buffer(std::as_bytes(data), storage_flags) {}

  ~Buffer() noexcept;

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&& other) noexcept;

  void Allocate(GLsizeiptr size, GLbitfield storage_flags = 0) const;

  void SetStorage(std::span<const std::byte> data, GLbitfield storage_flags = 0) const;

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  void SetStorage(std::span<const T> data, GLbitfield storage_flags = 0) const {
    SetStorage(std::as_bytes(data), storage_flags);
  }

  void SetData(GLintptr byte_offset, std::span<const std::byte> data) const;

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  void SetData(GLintptr byte_offset, std::span<const T> data) const {
    SetData(byte_offset, std::as_bytes(data));
  }

  [[nodiscard]]
  std::span<std::byte> MapBytes(GLintptr byte_offset, GLsizeiptr byte_size, GLbitfield access) const;

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  [[nodiscard]]
  std::span<T> Map(std::size_t element_offset, std::size_t element_count, GLbitfield access) const {
    const auto byte_offset = static_cast<GLintptr>(element_offset * sizeof(T));

    const auto byte_size = static_cast<GLsizeiptr>(element_count * sizeof(T));

    auto bytes = MapBytes(byte_offset, byte_size, access);

    if (bytes.empty()) {
      return {};
    }

    return {reinterpret_cast<T*>(bytes.data()), element_count};
  }

  [[nodiscard]]
  bool Unmap() const noexcept;

  void BindBase(GLenum target, GLuint index) const noexcept;

  void BindRange(GLenum target, GLuint index, GLintptr byte_offset, GLsizeiptr byte_size) const noexcept;

  [[nodiscard]]
  GLuint GetHandle() const noexcept;

private:
  GLuint m_handle{};
};
}  // namespace fw
