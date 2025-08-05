#pragma once

#include <cstdint>
#include <type_traits>

#include "aabb.h"
#include "camera.h"

namespace fw {
// ==========================================
// 鼠标按键定义
// ==========================================
enum class MouseButton : uint8_t {
  Left = 0,
  Right,
  Middle,
  // 如有需要可扩展侧键
  Button4,
  Button5,
  Unknown
};

// ==========================================
// 按键动作定义 (按压/释放/连续触发)
// ==========================================
enum class InputAction : uint8_t {
  Press = 0,  // 按下瞬间
  Release,    // 释放瞬间
  Repeat      // 长按连续触发 (主要用于键盘)
};

// ==========================================
// 常用键盘按键定义 (按需扩展)
// ==========================================
enum class Key : uint16_t {
  Unknown = 0,

  // 基础移动键
  W,
  A,
  S,
  D,
  Q,
  E,

  // 方向键
  Up,
  Down,
  Left,
  Right,

  // 控制与功能键
  Space,
  Escape,
  LeftShift,
  RightShift,
  LeftControl,
  RightControl,
  LeftAlt,
  RightAlt,

  // 数字与功能键按需添加...
};

// ==========================================
// 修饰键 (位掩码，使用 enum class)
// ==========================================
enum class Modifiers : uint8_t {
  None = 0,
  Shift = 1 << 0,    // 0001
  Control = 1 << 1,  // 0002
  Alt = 1 << 2,      // 0004
  Super = 1 << 3     // 0008 (Win 键 / Command 键)
};

// ==========================================
// 为 Modifiers 重载按位运算符 (核心支持)
// ==========================================
inline Modifiers operator|(Modifiers lhs, Modifiers rhs) {
  using Underlying = std::underlying_type_t<Modifiers>;
  return static_cast<Modifiers>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
}

inline Modifiers operator&(Modifiers lhs, Modifiers rhs) {
  using Underlying = std::underlying_type_t<Modifiers>;
  return static_cast<Modifiers>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));
}

inline Modifiers& operator|=(Modifiers& lhs, Modifiers rhs) {
  lhs = lhs | rhs;
  return lhs;
}

// 辅助工具方法：判断当前掩码中是否包含指定的修饰键
inline bool HasFlag(Modifiers flags, Modifiers contains) { return (flags & contains) == contains; }

class CameraController {
public:
  enum class State : uint8_t {
    None,      // 无操作 / 游离状态
    Rotating,  // 旋转/视角控制中
    Panning,   // 平移中
    Zooming    // 缩放/推进中（可选）
  };

  enum class ViewMode : uint8_t { Custom, Top, Bottom, Front, Back, Left, Right };

public:
  // Camera& 强制要求必须依附于一个有效的 Camera
  explicit CameraController(Camera& cam);

  virtual ~CameraController() noexcept = default;

  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;

  CameraController(CameraController&&) = delete;
  CameraController& operator=(CameraController&&) = delete;

  // 鼠标移动与滚轮
  virtual void OnMouseMove(double delta_x, double delta_y) = 0;
  virtual void OnMouseScroll(double offset, Modifiers mods) = 0;

  // 鼠标点击（包含修饰键，方便实现 Shift+左键 这种平移操作）
  virtual void OnMouseButton(MouseButton button, InputAction action, Modifiers mods) = 0;

  // 键盘事件（包含修饰键）
  virtual void OnKeyEvent(Key key, InputAction action, Modifiers mods) = 0;

  // 每帧逻辑更新
  virtual void Update(double delta_time) = 0;

  void SetViewportSize(int width, int height) noexcept;

  void ResetState() noexcept;

  void ResetCamera(const AABB& aabb) noexcept;

  void SetViewMode(ViewMode view_mode, const AABB& aabb) noexcept;

protected:
  Camera& GetCamera() noexcept;
  const Camera& GetCamera() const noexcept;

  State GetState() const noexcept;
  void SetState(State state) noexcept;

  ViewMode GetViewMode() const noexcept;
  void SetViewMode(ViewMode view_mode) noexcept;

private:
  Camera& m_camera;
  State m_state{State::None};
  ViewMode m_view_mode{ViewMode::Custom};
};

class OrbitController : public CameraController {
public:
  using CameraController::CameraController;

  void OnMouseMove(double delta_x, double delta_y) override;
  void OnMouseScroll(double offset, Modifiers mods) override;
  void OnMouseButton(MouseButton button, InputAction action, Modifiers mods) override;
  void OnKeyEvent(Key key, InputAction action, Modifiers mods) override;
  void Update(double delta_time) override;

  void ProcessPanning(float delta_x, float delta_y) noexcept;
  void ProcessRotation(float delta_x, float delta_y) noexcept;
};

class FreeFlyController : public CameraController {
public:
  using CameraController::CameraController;
};
}  // namespace fw
