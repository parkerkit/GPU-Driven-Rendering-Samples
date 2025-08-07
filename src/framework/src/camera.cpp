#include "camera.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

using namespace fw;

Camera::Camera() {
  UpdateProjectionMatrix();
  UpdateViewMatrix();
}

glm::mat4 Camera::GetViewMatrix() const noexcept {
  if (m_is_view_dirty) {
    UpdateViewMatrix();
  }
  return m_view_matrix;
}

glm::mat4 Camera::GetProjectionMatrix() const noexcept {
  if (m_is_proj_dirty) {
    UpdateProjectionMatrix();
  }
  return m_projection_matrix;
}

glm::mat4 Camera::GetViewProjectionMatrix() const noexcept { return GetProjectionMatrix() * GetViewMatrix(); }

void Camera::SetPosition(const glm::vec3& pos) noexcept {
  m_position = pos;
  m_is_view_dirty = true;
}

const glm::vec3& Camera::GetPosition() const noexcept { return m_position; }

void Camera::SetTarget(const glm::vec3& target) noexcept {
  m_target = target;
  m_is_view_dirty = true;
}

const glm::vec3& Camera::GetTarget() const noexcept { return m_target; }

void Camera::SetUp(const glm::vec3& up) noexcept {
  m_up = up;
  m_is_view_dirty = true;
}

const glm::vec3& Camera::GetUp() const noexcept { return m_up; }

void Camera::LookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) noexcept {
  m_position = pos;
  m_target = target;
  m_up = up;
  m_is_view_dirty = true;
}

void Camera::SetProjectionType(ProjectionType type) noexcept {
  m_proj_type = type;
  m_is_proj_dirty = true;
}

Camera::ProjectionType Camera::GetProjectionType() const noexcept { return m_proj_type; }

void Camera::SetFovY(float fov_y) noexcept {
  m_fov_y = std::clamp(fov_y, 1.0F, 179.0F);
  m_is_proj_dirty = true;
}

float Camera::GetFovY() const noexcept { return m_fov_y; }

void Camera::SetOrthoSize(float size) noexcept {
  m_ortho_size = std::max(size, 0.001F);
  m_is_proj_dirty = true;
}

float Camera::GetOrthoSize() const noexcept { return m_ortho_size; }

void Camera::SetNearFar(float near_plane, float far_plane) noexcept {
  m_near_plane = near_plane;
  m_far_plane = far_plane;
  m_is_proj_dirty = true;
}

void Camera::SetAspectRatio(float aspect_ratio) noexcept {
  if (aspect_ratio > 0.0F) {
    m_aspect_ratio = aspect_ratio;
    m_is_proj_dirty = true;
  }
}

float Camera::GetAspectRatio() const noexcept { return m_aspect_ratio; }

void Camera::UpdateViewMatrix() const noexcept {
  m_view_matrix = glm::lookAt(m_position, m_target, m_up);
  m_is_view_dirty = false;
}

void Camera::UpdateProjectionMatrix() const noexcept {
  if (m_proj_type == ProjectionType::Perspective) {
    m_projection_matrix = glm::perspective(glm::radians(m_fov_y), m_aspect_ratio, m_near_plane, m_far_plane);
  } else {
    float half_w = m_ortho_size * m_aspect_ratio;
    float half_h = m_ortho_size;
    m_projection_matrix = glm::ortho(-half_w, half_w, -half_h, half_h, m_near_plane, m_far_plane);
  }
  m_is_proj_dirty = false;
}
