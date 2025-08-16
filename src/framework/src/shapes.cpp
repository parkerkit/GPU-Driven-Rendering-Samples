#include "shapes.h"

#include <cmath>
#include <numbers>
#include <utility>

using namespace fw;

PrimitiveData Shapes::Cube(float size, const glm::vec3& center) {
  PrimitiveData primitive{};
  if (size <= 0.0F) {
    return primitive;
  }

  const float half = size * 0.5F;

  // 局部顶点坐标定义（以原点为中心），前后左右上下
  std::vector<glm::vec3> local_positions = {
    {-half, -half, half},  {half, -half, half},  {half, half, half},  {-half, half, half},   {half, -half, -half},
    {-half, -half, -half}, {-half, half, -half}, {half, half, -half}, {-half, -half, -half}, {-half, -half, half},
    {-half, half, half},   {-half, half, -half}, {half, -half, half}, {half, -half, -half},  {half, half, -half},
    {half, half, half},    {-half, half, half},  {half, half, half},  {half, half, -half},   {-half, half, -half},
    {-half, -half, -half}, {half, -half, -half}, {half, -half, half}, {-half, -half, half},
  };

  std::vector<glm::vec3> world_positions{};
  world_positions.reserve(local_positions.size());

  for (const auto& local_pos : local_positions) {
    world_positions.emplace_back(local_pos + center);
  }

  primitive.attributes.positions = std::move(world_positions);

  // 法线方向不受平移和等比缩放影响
  primitive.attributes.normals = {{0.0F, 0.0F, 1.0F},  {0.0F, 0.0F, 1.0F},  {0.0F, 0.0F, 1.0F},  {0.0F, 0.0F, 1.0F},
                                  {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, -1.0F},
                                  {-1.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
                                  {1.0F, 0.0F, 0.0F},  {1.0F, 0.0F, 0.0F},  {1.0F, 0.0F, 0.0F},  {1.0F, 0.0F, 0.0F},
                                  {0.0F, 1.0F, 0.0F},  {0.0F, 1.0F, 0.0F},  {0.0F, 1.0F, 0.0F},  {0.0F, 1.0F, 0.0F},
                                  {0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};

  primitive.attributes.texcoord_sets.resize(1);
  primitive.attributes.texcoord_sets[0] = {
    {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}, {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F},
    {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}, {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F},
    {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}, {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}};

  for (std::uint32_t i = 0; i < 6; ++i) {
    const std::uint32_t offset = i * 4;
    primitive.indices.push_back(offset);
    primitive.indices.push_back(offset + 1);
    primitive.indices.push_back(offset + 2);
    primitive.indices.push_back(offset + 2);
    primitive.indices.push_back(offset + 3);
    primitive.indices.push_back(offset);
  }

  primitive.bounds = {glm::vec3{-half} + center, glm::vec3{half} + center};

  return primitive;
}

PrimitiveData Shapes::Sphere(float radius, std::uint32_t x_segments, std::uint32_t y_segments,
                             const glm::vec3& center) {
  PrimitiveData primitive{};
  if (radius <= 0.0F || x_segments < 3 || y_segments < 2) {
    return primitive;
  }

  primitive.attributes.texcoord_sets.resize(1);

  for (std::uint32_t y = 0; y <= y_segments; ++y) {
    const float y_segment = static_cast<float>(y) / static_cast<float>(y_segments);
    const float y_angle = y_segment * std::numbers::pi_v<float>;

    for (std::uint32_t x = 0; x <= x_segments; ++x) {
      const float x_segment = static_cast<float>(x) / static_cast<float>(x_segments);
      const float x_angle = x_segment * 2.0F * std::numbers::pi_v<float>;

      // 基于单位球计算方向单位向量
      const float x_dir = std::cos(x_angle) * std::sin(y_angle);
      const float y_dir = std::cos(y_angle);
      const float z_dir = std::sin(x_angle) * std::sin(y_angle);

      const glm::vec3 normal_direction{x_dir, y_dir, z_dir};

      primitive.attributes.positions.push_back(normal_direction * radius + center);
      primitive.attributes.normals.push_back(normal_direction);
      primitive.attributes.texcoord_sets[0].emplace_back(x_segment, y_segment);
    }
  }

  for (std::uint32_t y = 0; y < y_segments; ++y) {
    for (std::uint32_t x = 0; x < x_segments; ++x) {
      const std::uint32_t current = y * (x_segments + 1) + x;
      const std::uint32_t next = current + x_segments + 1;

      // 从球外观察时使用逆时针绕序，使几何法线与顶点的向外法线一致。
      primitive.indices.push_back(current);
      primitive.indices.push_back(current + 1);
      primitive.indices.push_back(next);

      primitive.indices.push_back(current + 1);
      primitive.indices.push_back(next + 1);
      primitive.indices.push_back(next);
    }
  }

  primitive.bounds = {glm::vec3{-radius} + center, glm::vec3{radius} + center};

  return primitive;
}
