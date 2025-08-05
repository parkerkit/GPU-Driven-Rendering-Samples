#include "camera_controller.h"

using namespace fw;

CameraController::CameraController(Camera& cam) : m_camera(cam) {}

void CameraController::SetViewportSize(int width, int height) noexcept {
  const auto w = static_cast<float>(width);
  const auto h = static_cast<float>(height);

  m_camera.SetViewportSize(w, h);
}

void CameraController::ResetState() noexcept { m_state = State::None; }

void CameraController::ResetCamera(const AABB& aabb) noexcept {
  // 1. 新的观察目标点为包围盒中心
  glm::vec3 center = aabb.GetCenter();
  float radius = aabb.GetBoundingRadius();

  if (radius <= 0.0001F) {
    radius = 1.0F;  // 避免极端空包围盒
  }

  // 2. 根据视场角 FOV 计算最佳退后距离
  float fov_rad = glm::radians(m_camera.GetFov());
  // 使用 sin(fov / 2) 确保视锥体在任何角度都能完整容纳包围盒外接球
  float distance = radius / std::sin(fov_rad * 0.5F);

  // 额外给一圈安全边距 (Padding)
  distance *= 1.2F;

  // 3. 保持当前相机的观察方向，仅沿朝向后退
  glm::vec3 current_dir = m_camera.GetPosition() - m_camera.GetTarget();
  if (glm::length(current_dir) < 0.0001F) {
    current_dir = glm::vec3(0.0F, 0.0F, 1.0F);  // 默认观察方向
  } else {
    current_dir = glm::normalize(current_dir);
  }

  // 4. 计算新的 position 并应用到 camera
  glm::vec3 new_position = center + current_dir * distance;

  m_camera.LookAt(new_position, center, glm::vec3(0.0F, 1.0F, 0.0F));

  // 5. 如果是正交相机，还需要更新 OrthoSize
  if (m_camera.GetProjectionType() == Camera::ProjectionType::Orthographic) {
    m_camera.SetOrthoSize(radius * 1.2F);
  }

  // 6. 清空当前鼠标拖拽等交互状态
  ResetState();
}

void CameraController::SetViewMode(ViewMode view_mode, const AABB& aabb) noexcept {
  m_view_mode = view_mode;
  if (m_view_mode == ViewMode::Custom) {
    return;  // 自定义模式不强制重置位置
  }

  // 1. 获取包围盒中心与合适观察距离
  glm::vec3 center = aabb.GetCenter();
  float radius = aabb.GetBoundingRadius();
  if (radius <= 0.0001F) {
    radius = 1.0F;
  }

  float fov_rad = glm::radians(m_camera.GetFov());
  float distance = (radius / std::sin(fov_rad * 0.5F)) * 1.2F;

  // 2. 根据不同的标准视图计算视线方向 (dir) 和 Up 轴
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

  // 3. 应用新的 position 和 target
  glm::vec3 new_position = center + dir * distance;
  m_camera.LookAt(new_position, center, up_vec);

  // 4. 标准三视图通常自动切换为正交投影 (Orthographic)
  m_camera.SetProjectionType(Camera::ProjectionType::Orthographic);
  m_camera.SetOrthoSize(radius * 1.2F);

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
}

void OrbitController::OnMouseScroll(double offset, Modifiers mods) {
  const float scroll_sensitivity = 0.5F;  // 缩放灵敏度
  const float min_distance = 0.1F;        // 最小观察距离（防止穿透目标点）
  const float max_distance = 100.0F;      // 最大观察距离

  // 1. 计算从 target 指向 position 的方向向量与当前距离
  glm::vec3 dir = GetCamera().GetPosition() - GetCamera().GetTarget();
  float distance = glm::length(dir);

  if (distance < 0.0001F) {
    dir = glm::vec3(0.0F, 0.0F, 1.0F);
    distance = min_distance;
  } else {
    dir = glm::normalize(dir);
  }

  // 2. 根据滚轮 offset 修改距离（向前滚 offset > 0，距离变短）
  // 如果按下 Shift 键，可以缩放得更快（微调/快速缩放支持）
  float speed_multiplier = HasFlag(mods, Modifiers::Shift) ? 2.5F : 1.0F;
  distance -= static_cast<float>(offset) * scroll_sensitivity * speed_multiplier;

  // 3. 限制缩放范围
  distance = glm::clamp(distance, min_distance, max_distance);

  // 4. 更新相机位置（Target 保持不变，Position 沿着方向向量缩放）
  GetCamera().SetPosition(GetCamera().GetTarget() + dir * distance);
}

void OrbitController::OnMouseButton(MouseButton button, InputAction action, Modifiers mods) {
  static_cast<void>(mods);

  if (action == InputAction::Press) {
    if (button == MouseButton::Left) {
      SetState(State::Panning);
    } else if (button == MouseButton::Right) {
      SetState(State::Rotating);
    }
  } else if (action == InputAction::Release) {
    SetState(State::None);
  }
}

void OrbitController::OnKeyEvent(Key key, InputAction action, Modifiers mods) {
  static_cast<void>(key);
  static_cast<void>(action);
  static_cast<void>(mods);
}

void OrbitController::Update(double delta_time) { static_cast<void>(delta_time); }

void OrbitController::ProcessRotation(float delta_x, float delta_y) noexcept {
  // 1. 旋转灵敏度
  const float sensitivity = 0.005F;

  // 2. 获取当前的视角朝向向量（从 target 指向 position）
  glm::vec3 dir = GetCamera().GetPosition() - GetCamera().GetTarget();
  float radius = glm::length(dir);
  if (radius < 0.0001F) {
    return;
  }

  dir = glm::normalize(dir);

  // 3. 计算当前的 Yaw（偏航角）和 Pitch（俯仰角）
  float yaw = std::atan2(dir.z, dir.x);
  float pitch = std::asin(glm::clamp(dir.y, -1.0F, 1.0F));

  // 4. 根据鼠标移动应用偏移（dx 改变 yaw，dy 改变 pitch）
  yaw -= delta_x * sensitivity;
  pitch += delta_y * sensitivity;

  // 5. 限制 Pitch 范围，防止经过南北极点时发生万向锁（Gimbal Lock）或翻转
  const float max_pitch = glm::radians(89.0F);
  pitch = glm::clamp(pitch, -max_pitch, max_pitch);

  // 6. 根据新的角度重新推算方向向量
  glm::vec3 new_dir{};
  new_dir.x = std::cos(pitch) * std::cos(yaw);
  new_dir.y = std::sin(pitch);
  new_dir.z = std::cos(pitch) * std::sin(yaw);

  // 7. 更新相机位置（Target 保持不变，Position 绕其旋转）
  GetCamera().SetPosition(GetCamera().GetTarget() + new_dir * radius);

  // 8. 旋转时视角自动切换为 Custom
  SetViewMode(ViewMode::Custom);
}

void OrbitController::ProcessPanning(float delta_x, float delta_y) noexcept {
  // 1. 根据当前相机与目标点的距离动态调整平移灵敏度（离得越远，平移越快）
  float distance = glm::distance(GetCamera().GetPosition(), GetCamera().GetTarget());
  float sensitivity = distance * 0.001F;

  // 2. 计算当前相机的局部坐标系（前、右、上）
  glm::vec3 forward = glm::normalize(GetCamera().GetTarget() - GetCamera().GetPosition());
  glm::vec3 right = glm::normalize(glm::cross(forward, GetCamera().GetUp()));
  glm::vec3 up = glm::normalize(glm::cross(right, forward));

  // 3. 计算视平面上的平移向量
  // dx 沿 Right 向量反向，dy 沿 Up 向量正向（根据鼠标拖拽习惯可自行反转 +/- 号）
  glm::vec3 delta = (-delta_x * right + delta_y * up) * sensitivity;

  // 4. Panning 模式下，Position 和 Target 必须同步平移
  GetCamera().SetPosition(GetCamera().GetPosition() + delta);
  GetCamera().SetTarget(GetCamera().GetTarget() + delta);
}
