#include "aabb.h"

using namespace fw;

AABB::AABB(const glm::vec3& min, const glm::vec3& max) : m_min(min), m_max(max) {}

const glm::vec3& AABB::GetMin() const noexcept { return m_min; }

const glm::vec3& AABB::GetMax() const noexcept { return m_max; }

bool AABB::IsValid() const noexcept { return glm::all(glm::lessThanEqual(m_min, m_max)); }

glm::vec3 AABB::GetCenter() const noexcept { return IsValid() ? (m_min + m_max) * 0.5F : glm::vec3{}; }

glm::vec3 AABB::GetExtent() const noexcept { return IsValid() ? (m_max - m_min) * 0.5F : glm::vec3{}; }

float AABB::GetBoundingRadius() const noexcept { return IsValid() ? glm::length(m_max - m_min) * 0.5F : 0.0F; }

void AABB::Expand(const glm::vec3& point) noexcept {
  m_min = glm::min(m_min, point);
  m_max = glm::max(m_max, point);
}

void AABB::Merge(const AABB& other) noexcept {
  if (!other.IsValid()) {
    return;
  }

  m_min = glm::min(m_min, other.m_min);
  m_max = glm::max(m_max, other.m_max);
}

AABB AABB::Merge(std::span<const AABB> boxes) noexcept {
  AABB result{};
  for (const AABB& box : boxes) {
    result.Merge(box);
  }
  return result;
}

AABB AABB::Merge(std::initializer_list<AABB> boxes) noexcept {
  return Merge(std::span<const AABB>{boxes.begin(), boxes.size()});
}
