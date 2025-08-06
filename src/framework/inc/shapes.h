#pragma once

#include "primitive.h"

namespace fw {
class Shapes {
public:
  [[nodiscard]] static PrimitiveData Cube(float size = 1.0F, const glm::vec3& center = glm::vec3(0.0F));

  [[nodiscard]] static PrimitiveData Sphere(float radius = 0.5F, std::uint32_t x_segments = 32,
                                            std::uint32_t y_segments = 32, const glm::vec3& center = glm::vec3(0.0F));
};
}  // namespace fw
