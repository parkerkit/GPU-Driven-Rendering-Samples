#include "image.h"

using namespace fw;

#include <stb_image.h>

#include <stdexcept>
#include <string>

Image::Image(const std::filesystem::path& filepath, int desired_channels) {
  // Flip vertical orientation on load to match OpenGL's bottom-left origin
  stbi_set_flip_vertically_on_load(true);

  m_pixels = stbi_load(filepath.string().c_str(), &m_width, &m_height, &m_channels, desired_channels);
  if (!m_pixels) {
    throw std::runtime_error("Failed to load image: " + filepath.string() + " - " + stbi_failure_reason());
  }

  if (desired_channels > 0) {
    m_channels = desired_channels;
  }
}

Image::~Image() noexcept {
  if (m_pixels) {
    stbi_image_free(m_pixels);
  }
}

Image::Image(Image&& other) noexcept
    : m_pixels(std::exchange(other.m_pixels, nullptr)),
      m_width(std::exchange(other.m_width, 0)),
      m_height(std::exchange(other.m_height, 0)),
      m_channels(std::exchange(other.m_channels, 0)) {}

Image& Image::operator=(Image&& other) noexcept {
  if (this != &other) {
    if (m_pixels) {
      stbi_image_free(m_pixels);
    }
    m_pixels = std::exchange(other.m_pixels, nullptr);
    m_width = std::exchange(other.m_width, 0);
    m_height = std::exchange(other.m_height, 0);
    m_channels = std::exchange(other.m_channels, 0);
  }
  return *this;
}

const uint8_t* Image::GetPixels() const noexcept { return m_pixels; }

int Image::GetWidth() const noexcept { return m_width; }

int Image::GetHeight() const noexcept { return m_height; }

int Image::GetChannels() const noexcept { return m_channels; }

GLenum Image::GetGLInternalFormat() const noexcept {
  switch (m_channels) {
    case 1:
      return GL_R8;
    case 2:
      return GL_RG8;
    case 3:
      return GL_RGB8;
    case 4:
      return GL_RGBA8;
    default:
      return GL_NONE;
  }
}

GLenum Image::GetGLFormat() const noexcept {
  switch (m_channels) {
    case 1:
      return GL_RED;
    case 2:
      return GL_RG;
    case 3:
      return GL_RGB;
    case 4:
      return GL_RGBA;
    default:
      return GL_NONE;
  }
}
