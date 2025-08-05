#pragma once

#include <glm/glm.hpp>

namespace fw {
struct AABB {
  glm::vec3 min{};
  glm::vec3 max{};

  glm::vec3 GetCenter() const noexcept { return (min + max) * 0.5F; };
  float GetBoundingRadius() const noexcept { return glm::length(max - min) * 0.5F; }
};
}  // namespace fw
