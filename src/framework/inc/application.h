#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <exception>
#include <memory>
#include <string_view>

#include "camera.h"
#include "camera_controller.h"

namespace fw {
struct AppConfig {
  std::string_view window_name{"Application"};
  int window_width{800};
  int window_height{600};
  int gl_major_version{4};
  int gl_minor_version{6};
  bool is_vsync_enabled{true};
};

class Application {
public:
  Application() = default;
  virtual ~Application() noexcept;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void SetConfig(const AppConfig& config);
  /// @brief 每个实例只运行一次；窗口和 Context 在对象析构时释放。
  /// @details 派生类成员析构期间仍保留 Context，供 OpenGL 资源清理使用。
  void Run();

protected:
  virtual void OnInit();
  virtual void OnUpdate(double delta_time);
  virtual void OnRender();
  virtual void OnImGui();
  // OnInit 开始后，成功或失败都会调用；实现需允许资源仅部分创建。
  virtual void OnShutdown();
  virtual void OnFramebufferResized(int width, int height);
  virtual void OnCursorMoved(double delta_x, double delta_y);
  virtual void OnMouseButton(MouseButton button, InputAction action, Modifiers mods);
  virtual void OnScrolled(double offset, Modifiers mods);
  virtual void OnKey(Key key, InputAction action, Modifiers mods);

  Camera* GetCamera() noexcept;
  const Camera* GetCamera() const noexcept;
  CameraController* GetCameraController() noexcept;
  const CameraController* GetCameraController() const noexcept;

private:
  void InitializeWindow();
  void ShutdownWindow() noexcept;
  void InitializeImGui();
  void ShutdownImGui() noexcept;
  void EnableDebugOutput() const noexcept;
  void PrintGpuInfo() const noexcept;
  bool IsMouseCapturedByImGui() const noexcept;

  static Application& GetApplication(GLFWwindow* window);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
  static void WindowFocusCallback(GLFWwindow* window, int is_focused);
  static void CursorPositionCallback(GLFWwindow* window, double cursor_x, double cursor_y);
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
  static void ScrollCallback(GLFWwindow* window, double offset_x, double offset_y);
  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:
  AppConfig m_config{};
  GLFWwindow* m_window{};
  bool m_is_glfw_initialized{false};
  bool m_has_imgui_context{false};
  std::exception_ptr m_callback_exception{};
  double m_last_cursor_x{};
  double m_last_cursor_y{};
  bool m_has_cursor_position{false};
  std::unique_ptr<Camera> m_camera{std::make_unique<Camera>()};
  std::unique_ptr<CameraController> m_camera_controller{std::make_unique<OrbitController>(*m_camera)};
};
}  // namespace fw
