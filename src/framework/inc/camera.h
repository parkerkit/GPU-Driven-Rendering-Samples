#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace fw {
class Camera {
public:
  enum class ProjectionType : uint8_t { Perspective, Orthographic };

  Camera();

  void SetPosition(const glm::vec3& pos) noexcept;
  const glm::vec3& GetPosition() const noexcept;

  void SetTarget(const glm::vec3& target) noexcept;
  const glm::vec3& GetTarget() const noexcept;

  void SetUp(const glm::vec3& up) noexcept;
  const glm::vec3& GetUp() const noexcept;

  // 同时更新 LookAt 参数，减少重复标记
  void LookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) noexcept;

  // 仅设置投影类型；交互切换应使用 CameraController::SetProjectionType 转换取景参数。
  void SetProjectionType(ProjectionType type) noexcept;
  ProjectionType GetProjectionType() const noexcept;

  // 单位是角度
  void SetFovY(float fov_y) noexcept;
  float GetFovY() const noexcept;

  void SetOrthoSize(float size) noexcept;
  float GetOrthoSize() const noexcept;

  void SetNearFar(float near_plane, float far_plane) noexcept;

  void SetAspectRatio(float aspect_ratio) noexcept;
  float GetAspectRatio() const noexcept;

  glm::mat4 GetViewMatrix() const noexcept;
  glm::mat4 GetProjectionMatrix() const noexcept;
  glm::mat4 GetViewProjectionMatrix() const noexcept;

private:
  void UpdateViewMatrix() const noexcept;
  void UpdateProjectionMatrix() const noexcept;

private:
  // 基础变换数据
  glm::vec3 m_position{0.0F, 0.0F, 5.0F};
  glm::vec3 m_target{0.0F, 0.0F, 0.0F};
  glm::vec3 m_up{0.0F, 1.0F, 0.0F};

  // 投影参数
  ProjectionType m_proj_type{ProjectionType::Perspective};
  float m_fov_y{45.0F};      // 透视视角 (角度)
  float m_ortho_size{5.0F};  // 正交视口半高
  float m_aspect_ratio{16.0F / 9.0F};
  float m_near_plane{0.1F};
  float m_far_plane{100.0F};

  // 矩阵缓存与脏标记 (使用 mutable 允许在 const Get 函数中按需延迟计算)
  mutable glm::mat4 m_view_matrix{1.0F};
  mutable glm::mat4 m_projection_matrix{1.0F};
  mutable bool m_is_view_dirty{true};
  mutable bool m_is_proj_dirty{true};
};
}  // namespace fw
