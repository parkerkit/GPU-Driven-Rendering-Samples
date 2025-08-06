#pragma once

#include <glad/gl.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "aabb.h"

namespace fw {
struct VertexAttributes {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec4> tangents;
  std::vector<glm::vec4> colors;
  std::vector<std::vector<glm::vec2>> texcoord_sets;
};

struct PrimitiveData {
  GLenum topology{GL_TRIANGLES};

  VertexAttributes attributes;
  std::vector<std::uint32_t> indices;

  AABB bounds;
};
}  // namespace fw
