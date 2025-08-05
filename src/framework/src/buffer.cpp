#include "buffer.h"

using namespace fw;

Buffer::Buffer() { glCreateBuffers(1, &m_handle); }

Buffer::Buffer(GLsizeiptr size, GLbitfield storage_flags) : Buffer{} { Allocate(size, storage_flags); }

Buffer::Buffer(std::span<const std::byte> data, GLbitfield storage_flags) : Buffer{} {
  SetStorage(data, storage_flags);
}

Buffer::~Buffer() noexcept {
  if (m_handle != 0) {
    glDeleteBuffers(1, &m_handle);
  }
}

Buffer::Buffer(Buffer&& other) noexcept : m_handle{std::exchange(other.m_handle, 0)} {}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (m_handle != 0) {
    glDeleteBuffers(1, &m_handle);
  }

  m_handle = std::exchange(other.m_handle, 0);

  return *this;
}

void Buffer::Allocate(GLsizeiptr size, GLbitfield storage_flags) const {
  glNamedBufferStorage(m_handle, size, nullptr, storage_flags);
}

void Buffer::SetStorage(std::span<const std::byte> data, GLbitfield storage_flags) const {
  glNamedBufferStorage(m_handle, static_cast<GLsizeiptr>(data.size_bytes()), data.data(), storage_flags);
}

void Buffer::SetData(GLintptr byte_offset, std::span<const std::byte> data) const {
  glNamedBufferSubData(m_handle, byte_offset, static_cast<GLsizeiptr>(data.size_bytes()), data.data());
}

std::span<std::byte> Buffer::MapBytes(GLintptr byte_offset, GLsizeiptr byte_size, GLbitfield access) const {
  void* pointer = glMapNamedBufferRange(m_handle, byte_offset, byte_size, access);

  if (pointer == nullptr) {
    return {};
  }

  return {static_cast<std::byte*>(pointer), static_cast<std::size_t>(byte_size)};
}

bool Buffer::Unmap() const noexcept { return glUnmapNamedBuffer(m_handle) == GL_TRUE; }

void Buffer::BindBase(GLenum target, GLuint index) const noexcept { glBindBufferBase(target, index, m_handle); }

void Buffer::BindRange(GLenum target, GLuint index, GLintptr byte_offset, GLsizeiptr byte_size) const noexcept {
  glBindBufferRange(target, index, m_handle, byte_offset, byte_size);
}

GLuint Buffer::GetHandle() const noexcept { return m_handle; }
