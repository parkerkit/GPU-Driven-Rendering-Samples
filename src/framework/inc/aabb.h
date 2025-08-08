#pragma once

#include <glm/glm.hpp>
#include <initializer_list>
#include <limits>
#include <span>

namespace fw {
class AABB {
public:
  AABB() = default;

  AABB(const glm::vec3& min, const glm::vec3& max);

  [[nodiscard]] const glm::vec3& GetMin() const noexcept;

  [[nodiscard]] const glm::vec3& GetMax() const noexcept;

  [[nodiscard]] bool IsValid() const noexcept;

  [[nodiscard]] glm::vec3 GetCenter() const noexcept;

  [[nodiscard]] glm::vec3 GetExtent() const noexcept;

  [[nodiscard]] float GetBoundingRadius() const noexcept;

  void Expand(const glm::vec3& point) noexcept;

  void Merge(const AABB& other) noexcept;

  [[nodiscard]] static AABB Merge(std::span<const AABB> boxes) noexcept;

  [[nodiscard]] static AABB Merge(std::initializer_list<AABB> boxes) noexcept;

private:
  glm::vec3 m_min{std::numeric_limits<float>::max()};
  glm::vec3 m_max{std::numeric_limits<float>::lowest()};
};
}  // namespace fw
