#include "application.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>

using namespace fw;

namespace {
void CheckGLFWResult(int result, std::string_view message) {
  if (result == GLFW_FALSE) {
    throw std::runtime_error{std::string{message}};
  }
}

void CheckGLADResult(int result) {
  if (result == 0) {
    throw std::runtime_error{"Failed to initialize GLAD."};
  }
}

constexpr MouseButton GLFWButtonToMouseButton(int button) {
  // clang-format off
  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:        return MouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT:       return MouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE:      return MouseButton::Middle;
    default:                            return MouseButton::Unknown;
  }
  // clang-format on
}

constexpr Key GLFWKeyToKey(int key) {
  // clang-format off
  switch (key) {
    case GLFW_KEY_W:                    return Key::W;
    case GLFW_KEY_A:                    return Key::A;
    case GLFW_KEY_S:                    return Key::S;
    case GLFW_KEY_D:                    return Key::D;
    case GLFW_KEY_Q:                    return Key::Q;
    case GLFW_KEY_E:                    return Key::E;
    case GLFW_KEY_SPACE:                return Key::Space;
    case GLFW_KEY_LEFT_SHIFT:           return Key::LeftShift;
    case GLFW_KEY_RIGHT_SHIFT:          return Key::RightShift;
    case GLFW_KEY_LEFT_CONTROL:         return Key::LeftControl;
    case GLFW_KEY_RIGHT_CONTROL:        return Key::RightControl;
    case GLFW_KEY_LEFT:                 return Key::Left;
    case GLFW_KEY_RIGHT:                return Key::Right;
    case GLFW_KEY_UP:                   return Key::Up;
    case GLFW_KEY_DOWN:                 return Key::Down;
    case GLFW_KEY_ESCAPE:               return Key::Escape;
    default:                            return Key::Unknown;
  }
  // clang-format on
}

constexpr InputAction GLFWActionToInputAction(int action) {
  // clang-format off
  switch (action) {
    case GLFW_PRESS:        return InputAction::Press;
    case GLFW_RELEASE:      return InputAction::Release;
    case GLFW_REPEAT:       return InputAction::Repeat;
    default:                return InputAction::Press;  // 安全默认值
  }
  // clang-format on
}

constexpr Modifiers GLFWModToModifiers(int mods) {
  Modifiers result = Modifiers::None;

  if (mods & GLFW_MOD_SHIFT) {
    result |= Modifiers::Shift;
  }
  if (mods & GLFW_MOD_CONTROL) {
    result |= Modifiers::Control;
  }
  if (mods & GLFW_MOD_ALT) {
    result |= Modifiers::Alt;
  }
  if (mods & GLFW_MOD_SUPER) {
    result |= Modifiers::Super;
  }

  return result;
}

Modifiers GetCurrentModifiers(GLFWwindow* window) {
  Modifiers result = Modifiers::None;

  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
    result |= Modifiers::Shift;
  }

  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
    result |= Modifiers::Control;
  }

  if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
    result |= Modifiers::Alt;
  }

  return result;
}
}  // namespace

void Application::SetConfig(const AppConfig& config) { m_config = config; }

Camera* Application::GetCamera() noexcept { return m_camera.get(); }

const Camera* Application::GetCamera() const noexcept { return m_camera.get(); }

CameraController* Application::GetCameraController() noexcept { return m_camera_controller.get(); }

const CameraController* Application::GetCameraController() const noexcept { return m_camera_controller.get(); }

void Application::Run() {
  InitializeWindow();
  InitializeImGui();

  OnInit();

  double previous_time{glfwGetTime()};
  while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
    const double current_time{glfwGetTime()};
    const double delta_time{current_time - previous_time};
    previous_time = current_time;

    glfwPollEvents();
    OnUpdate(delta_time);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    OnImGui();
    ImGui::Render();

    OnRender();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
  }

  OnShutdown();
  ShutdownImGui();
  ShutdownWindow();
}

void Application::OnInit() {}
void Application::OnUpdate(double delta_time) { static_cast<void>(delta_time); }
void Application::OnRender() {}
void Application::OnImGui() {}
void Application::OnShutdown() {}

bool Application::IsMouseCapturedByImGui() const noexcept { return ImGui::GetIO().WantCaptureMouse; }

void Application::OnFramebufferResized(int width, int height) {
  if (!m_camera_controller) {
    return;
  }
  m_camera_controller->SetViewportSize(width, height);
}

void Application::OnCursorMoved(double cursor_x, double cursor_y) {
  if (!m_camera_controller) {
    return;
  }
  if (IsMouseCapturedByImGui()) {
    return;
  }

  m_camera_controller->OnMouseMove(cursor_x, cursor_y);
}

void Application::OnMouseButton(MouseButton button, InputAction action, Modifiers mods) {
  if (!m_camera_controller) {
    return;
  }
  if (IsMouseCapturedByImGui()) {
    return;
  }

  m_camera_controller->OnMouseButton(button, action, mods);
}

void Application::OnScrolled(double offset, Modifiers mods) {
  if (!m_camera_controller) {
    return;
  }
  if (IsMouseCapturedByImGui()) {
    return;
  }

  m_camera_controller->OnMouseScroll(offset, mods);
}

void Application::OnKey(Key key, InputAction action, Modifiers mods) {
  if (!m_camera_controller) {
    return;
  }
  if (IsMouseCapturedByImGui()) {
    return;
  }

  m_camera_controller->OnKeyEvent(key, action, mods);
}

void Application::InitializeWindow() {
  glfwSetErrorCallback(
    [](int error, const char* description) { std::cerr << "GLFW error [" << error << "]: " << description << '\n'; });

  CheckGLFWResult(glfwInit(), "Failed to initialize GLFW.");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_config.gl_major_version);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_config.gl_minor_version);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_window =
    glfwCreateWindow(m_config.window_width, m_config.window_height, m_config.window_name.data(), nullptr, nullptr);
  if (m_window == nullptr) {
    glfwTerminate();
    throw std::runtime_error{"Failed to create GLFW window."};
  }

  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(static_cast<int>(m_config.is_vsync_enabled));
  CheckGLADResult(gladLoadGL((GLADloadfunc)glfwGetProcAddress));

  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
  glfwSetCursorPosCallback(m_window, CursorPositionCallback);
  glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  glfwSetScrollCallback(m_window, ScrollCallback);
  glfwSetKeyCallback(m_window, KeyCallback);

  int framebuffer_width{};
  int framebuffer_height{};
  glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
  glViewport(0, 0, framebuffer_width, framebuffer_height);
  OnFramebufferResized(framebuffer_width, framebuffer_height);
}

void Application::ShutdownWindow() noexcept {
  if (m_window != nullptr) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
  glfwTerminate();
}

void Application::InitializeImGui() const {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");
}

void Application::ShutdownImGui() const noexcept {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

Application& Application::GetApplication(GLFWwindow* window) {
  return *static_cast<Application*>(glfwGetWindowUserPointer(window));
}

void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
  GetApplication(window).OnFramebufferResized(width, height);
}

void Application::CursorPositionCallback(GLFWwindow* window, double cursor_x, double cursor_y) {
  static double s_last_x{};
  static double s_last_y{};
  GetApplication(window).OnCursorMoved(cursor_x - s_last_x, cursor_y - s_last_y);
  s_last_x = cursor_x;
  s_last_y = cursor_y;
}

void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  auto temp_btn = GLFWButtonToMouseButton(button);
  auto temp_act = GLFWActionToInputAction(action);
  auto temp_mod = GLFWModToModifiers(mods);

  GetApplication(window).OnMouseButton(temp_btn, temp_act, temp_mod);
}

void Application::ScrollCallback(GLFWwindow* window, double offset_x, double offset_y) {
  static_cast<void>(offset_x);
  auto temp_mod = GetCurrentModifiers(window);

  GetApplication(window).OnScrolled(offset_y, temp_mod);
}

void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  static_cast<void>(scancode);

  auto temp_key = GLFWKeyToKey(key);
  auto temp_act = GLFWActionToInputAction(action);
  auto temp_mod = GLFWModToModifiers(mods);

  GetApplication(window).OnKey(temp_key, temp_act, temp_mod);
}
