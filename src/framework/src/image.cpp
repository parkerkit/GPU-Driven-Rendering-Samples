#include "image.h"

#include <stb_image.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace fw;

Image::Image(const std::filesystem::path& file_path, int desired_channels, bool flip_vertically) {
  stbi_set_flip_vertically_on_load_thread(flip_vertically);

  m_pixels = stbi_load(file_path.string().c_str(), &m_width, &m_height, &m_channels, desired_channels);
  if (!m_pixels) {
    throw std::runtime_error("Failed to load image: " + file_path.string() + " - " + stbi_failure_reason());
  }

  SetDesiredChannels(desired_channels);
}

Image::Image(std::span<const std::uint8_t> encoded_data, int desired_channels, bool flip_vertically) {
  if (encoded_data.empty()) {
    throw std::runtime_error("Failed to load image from memory: encoded data is empty.");
  }

  if (encoded_data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("Failed to load image from memory: encoded data is too large.");
  }

  stbi_set_flip_vertically_on_load_thread(flip_vertically);

  m_pixels = stbi_load_from_memory(encoded_data.data(), static_cast<int>(encoded_data.size()), &m_width, &m_height,
                                   &m_channels, desired_channels);
  if (!m_pixels) {
    throw std::runtime_error(std::string{"Failed to load image from memory: "} + stbi_failure_reason());
  }

  SetDesiredChannels(desired_channels);
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

const std::uint8_t* Image::GetPixels() const noexcept { return m_pixels; }

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

void Image::SetDesiredChannels(int desired_channels) noexcept {
  if (desired_channels > 0) {
    m_channels = desired_channels;
  }
}
