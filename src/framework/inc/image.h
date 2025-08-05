#pragma once

#include <glad/gl.h>

#include <cstdint>
#include <filesystem>

namespace fw {
class Image {
public:
  // Load image from disk. Desired channels: 0 = autodetect, 3 = RGB, 4 = RGBA
  explicit Image(const std::filesystem::path& filepath, int desired_channels = 0);

  ~Image() noexcept;

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  Image(Image&& other) noexcept;

  Image& operator=(Image&& other) noexcept;

  [[nodiscard]] const uint8_t* GetPixels() const noexcept;
  [[nodiscard]] int GetWidth() const noexcept;
  [[nodiscard]] int GetHeight() const noexcept;
  [[nodiscard]] int GetChannels() const noexcept;

  // Map channels to OpenGL GPU internal formats
  [[nodiscard]] GLenum GetGLInternalFormat() const noexcept;

  // Map channels to OpenGL pixel transfer format
  [[nodiscard]] GLenum GetGLFormat() const noexcept;

private:
  uint8_t* m_pixels{nullptr};
  int m_width{0};
  int m_height{0};
  int m_channels{0};
};
}  // namespace fw
