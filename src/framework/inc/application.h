#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

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
  int gl_minor_version{5};
  bool is_vsync_enabled{true};
};

class Application {
public:
  Application() = default;
  virtual ~Application() = default;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void SetConfig(const AppConfig& config);
  void Run();

protected:
  virtual void OnInit();
  virtual void OnUpdate(double delta_time);
  virtual void OnRender();
  virtual void OnImGui();
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
  void InitializeImGui() const;
  void ShutdownImGui() const noexcept;
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
  double m_last_cursor_x{};
  double m_last_cursor_y{};
  bool m_has_cursor_position{false};
  std::unique_ptr<Camera> m_camera{std::make_unique<Camera>()};
  std::unique_ptr<CameraController> m_camera_controller{std::make_unique<OrbitController>(*m_camera)};
};
}  // namespace fw
