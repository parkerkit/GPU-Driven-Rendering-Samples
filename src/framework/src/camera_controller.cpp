#include "camera_controller.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>  // glm::length2

#include "camera.h"

using namespace fw;

namespace {
float ScaleZoom(float value, float factor, float min_value, float max_value) noexcept {
  // 外部设置或窗口尺寸变化可能使当前值超出限制，仍须保持缩放连续且方向正确。
  const float lower_bound{std::min(value, min_value)};
  const float upper_bound{std::max(value, max_value)};
  return std::clamp(value * factor, lower_bound, upper_bound);
}
}  // namespace

CameraController::CameraController(Camera& cam) : m_camera(cam) {}

void CameraController::SetViewportSize(int width, int height) noexcept {
  if (width <= 0 || height <= 0) {
    m_viewport_size = glm::vec2{};
    ResetState();
    return;
  }

  const auto w = static_cast<float>(width);
  const auto h = static_cast<float>(height);

  m_viewport_size = glm::vec2{w, h};
  m_camera.SetAspectRatio(w / h);
}

void CameraController::ResetState() noexcept { m_state = State::None; }

void CameraController::SetSceneBounds(const AABB& bounds) noexcept {
  m_scene_bounds = bounds;
  UpdateClipRange();
}

glm::vec2 CameraController::GetViewportSize() const noexcept { return m_viewport_size; }

float CameraController::GetSceneRadius() const noexcept {
  constexpr float k_min_radius{0.0001F};
  constexpr float k_fallback_radius{0.5F};
  const float radius{m_scene_bounds.GetBoundingRadius()};
  // 极小场景仍需为相机方向和裁剪面留出可表示的数值范围。
  return radius > 0.0F ? std::max(radius, k_min_radius) : k_fallback_radius;
}

float CameraController::GetFramingDistance() const noexcept {
  const float half_fov_y{glm::radians(m_camera.GetFovY()) * 0.5F};
  const float half_fov_x{std::atan(std::tan(half_fov_y) * m_camera.GetAspectRatio())};
  const float limiting_half_fov{std::min(half_fov_x, half_fov_y)};
  constexpr float k_framing_margin{1.05F};
  return k_framing_margin * GetSceneRadius() / std::sin(limiting_half_fov);
}

float CameraController::GetFramingOrthoSize() const noexcept {
  constexpr float k_framing_margin{1.2F};
  // 正交尺寸是半高；窄窗口还需保证半宽能够容纳包围球。
  return k_framing_margin * GetSceneRadius() / std::min(m_camera.GetAspectRatio(), 1.0F);
}

void CameraController::ResetCamera(const AABB& bounds) noexcept {
  constexpr float k_epsilon{0.0001F};
  m_scene_bounds = bounds;

  const glm::vec3 center{bounds.GetCenter()};
  const float distance{GetFramingDistance()};
  Camera& camera{GetCamera()};
  glm::vec3 view_offset{camera.GetPosition() - camera.GetTarget()};

  if (glm::length2(view_offset) < k_epsilon * k_epsilon) {
    view_offset = glm::vec3(0.0F, 0.0F, 1.0F);
  } else {
    view_offset = glm::normalize(view_offset);
  }

  const glm::vec3 new_eye_pos = center + distance * view_offset;

  camera.SetTarget(center);
  camera.SetPosition(new_eye_pos);

  camera.SetOrthoSize(GetFramingOrthoSize());
  UpdateClipRange();

  // 清空当前鼠标拖拽等交互状态
  ResetState();
}

void CameraController::UpdateClipRange(const AABB& bounds) noexcept { SetSceneBounds(bounds); }

void CameraController::UpdateClipRange() noexcept {
  if (!m_scene_bounds.IsValid()) {
    return;
  }

  const glm::vec3 center{m_scene_bounds.GetCenter()};
  const float radius{GetSceneRadius()};

  auto& camera = GetCamera();

  const glm::vec3 eye = camera.GetPosition();
  const glm::vec3 target = camera.GetTarget();

  constexpr float eps = 1e-6F;

  glm::vec3 forward = target - eye;
  const float length = glm::length(forward);

  if (length < eps) {
    return;
  }

  forward /= length;

  // bounds center 在相机 forward 方向上的深度
  const float depth = glm::dot(center - eye, forward);

  constexpr float expand_ratio = 1.1F;

  const float min_near = std::max(0.000001F, radius * 0.0001F);
  const float near_plane = std::max(min_near, depth - radius * expand_ratio);
  const float far_plane = depth + radius * expand_ratio;

  // 整个 bounds 都在相机后面
  if (far_plane <= min_near) {
    return;
  }

  camera.SetNearFar(near_plane, std::max(far_plane, near_plane + min_near));
}

void CameraController::SetProjectionType(Camera::ProjectionType type) noexcept {
  if (type == m_camera.GetProjectionType()) {
    return;
  }

  constexpr float k_epsilon{1e-6F};
  const glm::vec3 target{m_camera.GetTarget()};
  const glm::vec3 view{target - m_camera.GetPosition()};
  float distance{glm::length(view)};
  if (distance < k_epsilon) {
    return;
  }

  const glm::vec3 forward{view / distance};
  const glm::vec3 right_raw{glm::cross(forward, m_camera.GetUp())};
  const float right_length{glm::length(right_raw)};
  if (right_length < k_epsilon) {
    return;
  }

  const glm::vec3 right{right_raw / right_length};
  const glm::vec3 up{glm::normalize(glm::cross(right, forward))};
  const float aspect{m_camera.GetAspectRatio()};
  const float tan_half_fov_y{std::tan(glm::radians(m_camera.GetFovY()) * 0.5F)};
  const float tan_half_fov_x{tan_half_fov_y * aspect};
  float ortho_size{m_camera.GetOrthoSize()};
  if (type == Camera::ProjectionType::Orthographic) {
    ortho_size = distance * tan_half_fov_y;
  } else {
    distance = ortho_size / tan_half_fov_y;
  }

  if (m_scene_bounds.IsValid()) {
    constexpr float k_framing_margin{1.05F};
    // 确保所有角点都在相机前方，且深度余量大于 UpdateClipRange 的 near 下限。
    const float depth_margin{std::max(0.00001F, GetSceneRadius() * 0.001F)};
    const glm::vec3 bounds_min{m_scene_bounds.GetMin()};
    const glm::vec3 bounds_max{m_scene_bounds.GetMax()};
    for (int corner_index{}; corner_index < 8; ++corner_index) {
      const glm::vec3 corner{(corner_index & 1) != 0 ? bounds_max.x : bounds_min.x,
                             (corner_index & 2) != 0 ? bounds_max.y : bounds_min.y,
                             (corner_index & 4) != 0 ? bounds_max.z : bounds_min.z};
      const glm::vec3 offset{corner - target};
      const float horizontal_extent{std::abs(glm::dot(offset, right)) * k_framing_margin};
      const float vertical_extent{std::abs(glm::dot(offset, up)) * k_framing_margin};
      const float depth_offset{glm::dot(offset, forward)};

      // 保留平移后的 target，按角点相对当前视轴的偏移取景。
      distance = std::max(distance, depth_margin - depth_offset);
      if (type == Camera::ProjectionType::Orthographic) {
        ortho_size = std::max({ortho_size, horizontal_extent / aspect, vertical_extent});
      } else {
        distance = std::max({distance, horizontal_extent / tan_half_fov_x - depth_offset,
                             vertical_extent / tan_half_fov_y - depth_offset});
      }
    }
  }

  m_camera.LookAt(target - forward * distance, target, up);
  m_camera.SetOrthoSize(ortho_size);
  m_camera.SetProjectionType(type);
  UpdateClipRange();
  ResetState();
}

void CameraController::SetViewMode(ViewMode view_mode, const AABB& aabb) noexcept {
  SetSceneBounds(aabb);
  m_view_mode = view_mode;
  if (m_view_mode == ViewMode::Custom) {
    return;  // 自定义模式不强制重置位置
  }

  glm::vec3 center = aabb.GetCenter();
  const float distance{GetFramingDistance()};

  glm::vec3 dir{0.0F, 0.0F, 1.0F};
  glm::vec3 up_vec{0.0F, 1.0F, 0.0F};

  switch (m_view_mode) {
    case ViewMode::Front:
      dir = glm::vec3(0.0F, 0.0F, 1.0F);
      up_vec = glm::vec3(0.0F, 1.0F, 0.0F);
      break;
    case ViewMode::Back:
      dir = glm::vec3(0.0F, 0.0F, -1.0F);
      up_vec = glm::vec3(0.0F, 1.0F, 0.0F);
      break;
    case ViewMode::Left:
      dir = glm::vec3(-1.0F, 0.0F, 0.0F);
      up_vec = glm::vec3(0.0F, 1.0F, 0.0F);
      break;
    case ViewMode::Right:
      dir = glm::vec3(1.0F, 0.0F, 0.0F);
      up_vec = glm::vec3(0.0F, 1.0F, 0.0F);
      break;
    case ViewMode::Top:
      dir = glm::vec3(0.0F, 1.0F, 0.0F);
      up_vec = glm::vec3(0.0F, 0.0F, -1.0F);
      break;
    case ViewMode::Bottom:
      dir = glm::vec3(0.0F, -1.0F, 0.0F);
      up_vec = glm::vec3(0.0F, 0.0F, 1.0F);
      break;
    default:
      break;
  }

  glm::vec3 new_position = center + dir * distance;
  m_camera.LookAt(new_position, center, up_vec);

  // 标准三视图通常自动切换为正交投影 (Orthographic)
  m_camera.SetProjectionType(Camera::ProjectionType::Orthographic);
  m_camera.SetOrthoSize(GetFramingOrthoSize());
  UpdateClipRange();

  ResetState();
}

Camera& CameraController::GetCamera() noexcept { return m_camera; }

const Camera& CameraController::GetCamera() const noexcept { return m_camera; }

CameraController::State CameraController::GetState() const noexcept { return m_state; }
void CameraController::SetState(State state) noexcept { m_state = state; }

CameraController::ViewMode CameraController::GetViewMode() const noexcept { return m_view_mode; }
void CameraController::SetViewMode(ViewMode view_mode) noexcept { m_view_mode = view_mode; }

void OrbitController::OnMouseMove(double delta_x, double delta_y) {
  if (GetState() == State::None) {
    return;
  }

  const float dx = static_cast<float>(delta_x);
  const float dy = static_cast<float>(delta_y);

  if (GetState() == State::Rotating) {
    ProcessRotation(dx, dy);
  } else if (GetState() == State::Panning) {
    ProcessPanning(dx, dy);
  }
  UpdateClipRange();
}

void OrbitController::OnMouseScroll(double offset, Modifiers mods) {
  if (offset == 0.0 || !std::isfinite(offset)) {
    return;
  }

  constexpr float k_min_zoom_ratio{0.001F};
  constexpr float k_max_zoom_ratio{1000.0F};
  const float min_distance{std::max(0.0001F, GetSceneRadius() * k_min_zoom_ratio)};
  const float max_distance{GetFramingDistance() * k_max_zoom_ratio};
  // 与 Camera::SetOrthoSize 的数值下限保持一致。
  const float min_ortho_size{std::max(0.001F, GetSceneRadius() * k_min_zoom_ratio)};
  const float max_ortho_size{std::max(min_ortho_size, GetFramingOrthoSize() * k_max_zoom_ratio)};

  auto& camera = GetCamera();

  const float speed_multiplier = HasFlag(mods, Modifiers::Shift) ? 2.5F : 1.0F;

  // 防止触控板 / 滚轮一次产生过大的输入
  const float scroll{static_cast<float>(std::clamp(offset, -10.0, 10.0))};

  // 指数缩放：
  // offset > 0 -> zoom_factor < 1 -> 放大
  // offset < 0 -> zoom_factor > 1 -> 缩小
  const float zoom_factor = std::exp(-scroll * 0.1F * speed_multiplier);

  if (camera.GetProjectionType() == Camera::ProjectionType::Orthographic) {
    // 正交相机：
    // 不需要改变相机到 Target 的距离，只改变正交视口大小
    const float ortho_size{ScaleZoom(camera.GetOrthoSize(), zoom_factor, min_ortho_size, max_ortho_size)};

    camera.SetOrthoSize(ortho_size);
  } else {
    // 透视相机：
    // 通过改变相机到 Target 的距离实现推进与后退。
    glm::vec3 dir = camera.GetPosition() - camera.GetTarget();

    float distance = glm::length(dir);

    if (distance < 0.0001F) {
      dir = glm::vec3(0.0F, 0.0F, 1.0F);
      distance = min_distance;
    } else {
      dir /= distance;
    }

    distance = ScaleZoom(distance, zoom_factor, min_distance, max_distance);

    camera.SetPosition(camera.GetTarget() + dir * distance);
  }
  UpdateClipRange();
}

void OrbitController::OnMouseButton(MouseButton button, InputAction action, Modifiers mods) {
  static_cast<void>(mods);

  if (action == InputAction::Press) {
    // 首个按键拥有本次拖拽；其他按键不会抢占或中断操作。
    if (GetState() != State::None) {
      return;
    }
    if (button == MouseButton::Left) {
      SetState(State::Panning);
    } else if (button == MouseButton::Right) {
      SetState(State::Rotating);
    }
  } else if (action == InputAction::Release) {
    const bool is_panning_released{button == MouseButton::Left && GetState() == State::Panning};
    const bool is_rotating_released{button == MouseButton::Right && GetState() == State::Rotating};
    if (is_panning_released || is_rotating_released) {
      ResetState();
    }
  }
}

void OrbitController::OnKeyEvent(Key key, InputAction action, Modifiers mods) {
  static_cast<void>(key);
  static_cast<void>(action);
  static_cast<void>(mods);
}

void OrbitController::Update(double delta_time) {
  static_cast<void>(delta_time);
  // 同时覆盖应用层直接调整 Camera 的情况。
  UpdateClipRange();
}

void OrbitController::ProcessRotation(float delta_x, float delta_y) noexcept {
  if (delta_x == 0.0F && delta_y == 0.0F) {
    return;
  }

  // 按视口高度归一化，避免高 DPI 像素换算改变旋转速度。
  constexpr float k_rotation_per_viewport{6.0F};
  const float viewport_height{GetViewportSize().y};
  const float sensitivity{viewport_height > 0.0F ? k_rotation_per_viewport / viewport_height : 0.01F};
  constexpr float eps = 1e-6F;

  const glm::vec3 pos = GetCamera().GetPosition();
  const glm::vec3 target = GetCamera().GetTarget();
  const glm::vec3 view_up = GetCamera().GetUp();

  glm::vec3 view = target - pos;
  float view_len = glm::length(view);

  if (view_len < eps) {
    return;
  }

  glm::vec3 forward = view / view_len;

  glm::vec3 right_raw = glm::cross(forward, view_up);
  float right_len = glm::length(right_raw);

  if (right_len < eps) {
    return;
  }

  glm::vec3 right = right_raw / right_len;

  // 重新构造严格正交的 up
  glm::vec3 up = glm::normalize(glm::cross(right, forward));

  glm::vec3 rotate_axis = glm::normalize(-delta_x * up - delta_y * right);

  float angle = glm::length(glm::vec2(delta_x, delta_y)) * sensitivity;

  glm::quat delta_quat = glm::angleAxis(angle, rotate_axis);
  glm::vec3 view_dir = pos - target;

  glm::vec3 new_pos = target + delta_quat * view_dir;
  glm::vec3 new_up = glm::normalize(delta_quat * up);

  GetCamera().SetPosition(new_pos);
  GetCamera().SetUp(new_up);

  // 旋转时视角自动切换为 Custom
  SetViewMode(ViewMode::Custom);
}

void OrbitController::ProcessPanning(float delta_x, float delta_y) noexcept {
  if (delta_x == 0.0F && delta_y == 0.0F) {
    return;
  }

  const glm::vec2 viewport_size{GetViewportSize()};
  if (viewport_size.x <= 0.0F || viewport_size.y <= 0.0F) {
    return;
  }

  constexpr float eps = 1e-6F;

  const glm::vec3 pos = GetCamera().GetPosition();
  const glm::vec3 target = GetCamera().GetTarget();
  const glm::vec3 view_up = GetCamera().GetUp();

  glm::vec3 view = target - pos;
  float distance = glm::length(view);

  if (distance < eps) {
    return;
  }

  glm::vec3 forward = view / distance;

  glm::vec3 right_raw = glm::cross(forward, view_up);
  float right_len = glm::length(right_raw);

  if (right_len < eps) {
    return;
  }

  glm::vec3 right = right_raw / right_len;

  // 重新构造严格正交的 up
  glm::vec3 up = glm::normalize(glm::cross(right, forward));

  const Camera& camera{GetCamera()};
  const float half_height{camera.GetProjectionType() == Camera::ProjectionType::Orthographic
                            ? camera.GetOrthoSize()
                            : distance * std::tan(glm::radians(camera.GetFovY()) * 0.5F)};
  const float world_per_pixel_y{2.0F * half_height / viewport_size.y};
  const float world_per_pixel_x{2.0F * half_height * camera.GetAspectRatio() / viewport_size.x};
  const glm::vec3 translation{-delta_x * world_per_pixel_x * right + delta_y * world_per_pixel_y * up};

  GetCamera().SetPosition(pos + translation);
  GetCamera().SetTarget(target + translation);
}
