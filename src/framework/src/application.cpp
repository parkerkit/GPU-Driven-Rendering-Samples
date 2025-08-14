#include "application.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace fw;

namespace {
// 异常不能跨越 GLFW 的 C 回调边界，记录后交给主循环处理。
template <typename Callback>
void InvokeCallback(std::exception_ptr& pending_exception, Callback callback) noexcept {
  if (pending_exception) {
    return;
  }
  try {
    callback();
  } catch (...) {
    pending_exception = std::current_exception();
  }
}

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

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                const GLchar* message, const void* user_param) {
  static_cast<void>(source);
  static_cast<void>(id);
  static_cast<void>(length);
  static_cast<void>(user_param);

  // 忽略一些不重要的通知类消息
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
    return;
  }

  std::cerr << "------------------------------------------\n";
  std::cerr << "OpenGL Debug Output\n";
  std::cerr << "Message: " << message << '\n';
  std::cerr << "Type: ";
  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      std::cerr << "ERROR";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      std::cerr << "DEPRECATED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      std::cerr << "UNDEFINED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      std::cerr << "PORTABILITY";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      std::cerr << "PERFORMANCE";
      break;
    case GL_DEBUG_TYPE_OTHER:
      std::cerr << "OTHER";
      break;
  }

  std::cerr << "\nSeverity: ";
  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      std::cerr << "HIGH (Fatal/Critical)";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      std::cerr << "MEDIUM";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      std::cerr << "LOW";
      break;
  }
  std::cout << "\n";
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

Application::~Application() noexcept {
  // 派生类成员已完成析构，最后再销毁 ImGui 和窗口中的 Context。
  ShutdownImGui();
  ShutdownWindow();
}

void Application::Run() {
  if (m_is_glfw_initialized) {
    throw std::logic_error{"Application::Run() can only be called once."};
  }
  InitializeWindow();
  InitializeImGui();

  try {
    OnInit();

    double previous_time{glfwGetTime()};
    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
      const double current_time{glfwGetTime()};
      const double delta_time{current_time - previous_time};
      previous_time = current_time;

      glfwPollEvents();
      if (m_callback_exception) {
        std::rethrow_exception(m_callback_exception);
      }
      OnUpdate(delta_time);

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      OnImGui();
      ImGui::Render();

      if (m_camera_controller) {
        if (IsMouseCapturedByImGui()) {
          m_camera_controller->ResetState();
        }
        // UI 和业务逻辑也可能改变相机，渲染前统一更新裁剪范围。
        m_camera_controller->Update(delta_time);
      }

      OnRender();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(m_window);
      if (m_callback_exception) {
        std::rethrow_exception(m_callback_exception);
      }
    }

  } catch (...) {
    try {
      OnShutdown();
    } catch (const std::exception& error) {
      std::cerr << "Application cleanup failed: " << error.what() << '\n';
    } catch (...) {
      std::cerr << "Unknown application cleanup failure\n";
    }
    // 清理失败也保留最初的初始化或运行异常。
    throw;
  }
  OnShutdown();
}

void Application::OnInit() {}
void Application::OnUpdate(double delta_time) { static_cast<void>(delta_time); }
void Application::OnRender() {}
void Application::OnImGui() {}
void Application::OnShutdown() {}

bool Application::IsMouseCapturedByImGui() const noexcept {
  return m_has_imgui_context && ImGui::GetIO().WantCaptureMouse;
}

void Application::OnFramebufferResized(int width, int height) {
  if (!m_camera_controller) {
    return;
  }
  m_camera_controller->SetViewportSize(width, height);
}

void Application::OnCursorMoved(double delta_x, double delta_y) {
  if (!m_camera_controller) {
    return;
  }
  if (IsMouseCapturedByImGui()) {
    m_camera_controller->ResetState();
    return;
  }

  int window_width{};
  int window_height{};
  int framebuffer_width{};
  int framebuffer_height{};
  glfwGetWindowSize(m_window, &window_width, &window_height);
  glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
  if (window_width <= 0 || window_height <= 0 || framebuffer_width <= 0 || framebuffer_height <= 0) {
    m_camera_controller->ResetState();
    return;
  }

  // GLFW 光标使用窗口坐标，控制器视口使用 framebuffer 像素。
  const double pixel_delta_x{delta_x * static_cast<double>(framebuffer_width) / window_width};
  const double pixel_delta_y{delta_y * static_cast<double>(framebuffer_height) / window_height};
  m_camera_controller->OnMouseMove(pixel_delta_x, pixel_delta_y);
}

void Application::OnMouseButton(MouseButton button, InputAction action, Modifiers mods) {
  if (!m_camera_controller) {
    return;
  }
  // 已开始的拖拽必须收到释放事件，即使鼠标此时由 UI 捕获。
  if (action != InputAction::Release && IsMouseCapturedByImGui()) {
    m_camera_controller->ResetState();
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
  m_is_glfw_initialized = true;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_config.gl_major_version);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_config.gl_minor_version);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

  m_window =
    glfwCreateWindow(m_config.window_width, m_config.window_height, m_config.window_name.data(), nullptr, nullptr);
  if (m_window == nullptr) {
    throw std::runtime_error{"Failed to create GLFW window."};
  }

  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(static_cast<int>(m_config.is_vsync_enabled));
  CheckGLADResult(gladLoadGL((GLADloadfunc)glfwGetProcAddress));

  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
  glfwSetWindowFocusCallback(m_window, WindowFocusCallback);
  glfwSetCursorPosCallback(m_window, CursorPositionCallback);
  glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  glfwSetScrollCallback(m_window, ScrollCallback);
  glfwSetKeyCallback(m_window, KeyCallback);

  int framebuffer_width{};
  int framebuffer_height{};
  glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
  glViewport(0, 0, framebuffer_width, framebuffer_height);
  OnFramebufferResized(framebuffer_width, framebuffer_height);

  EnableDebugOutput();
  PrintGpuInfo();
}

void Application::EnableDebugOutput() const noexcept {
  GLint flags{};
  glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
  if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  }
}

void Application::PrintGpuInfo() const noexcept {
  const GLubyte* vendor = glGetString(GL_VENDOR);                  // 显卡厂商 (Vendor)
  const GLubyte* renderer = glGetString(GL_RENDERER);              // 显卡型号 / 渲染器名称 (Renderer)
  const GLubyte* version = glGetString(GL_VERSION);                // 当前支持的 OpenGL 版本 (Version)
  const GLubyte* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);  // 着色语言 (GLSL) 版本

  auto safe_gl_str = [](const GLubyte* str) { return str ? reinterpret_cast<const char*>(str) : "Unknown"; };

  std::cout << "Vendor: " << safe_gl_str(vendor) << "\n";
  std::cout << "Renderer: " << safe_gl_str(renderer) << "\n";
  std::cout << "Version: " << safe_gl_str(version) << "\n";
  std::cout << "GLSL: " << safe_gl_str(glsl) << "\n";
}

void Application::ShutdownWindow() noexcept {
  if (m_window != nullptr) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
  if (m_is_glfw_initialized) {
    glfwTerminate();
    m_is_glfw_initialized = false;
  }
}

void Application::InitializeImGui() {
  IMGUI_CHECKVERSION();
  if (ImGui::CreateContext() == nullptr) {
    throw std::runtime_error{"Failed to create ImGui context."};
  }
  m_has_imgui_context = true;
  ImGui::StyleColorsDark();
  if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) {
    throw std::runtime_error{"Failed to initialize ImGui GLFW backend."};
  }
  if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
    throw std::runtime_error{"Failed to initialize ImGui OpenGL backend."};
  }
}

void Application::ShutdownImGui() noexcept {
  if (!m_has_imgui_context) {
    return;
  }
  // 后端可能只初始化了一部分，只清理已创建的后端数据。
  if (ImGui::GetIO().BackendRendererUserData != nullptr) {
    ImGui_ImplOpenGL3_Shutdown();
  }
  if (ImGui::GetIO().BackendPlatformUserData != nullptr) {
    ImGui_ImplGlfw_Shutdown();
  }
  ImGui::DestroyContext();
  m_has_imgui_context = false;
}

Application& Application::GetApplication(GLFWwindow* window) {
  return *static_cast<Application*>(glfwGetWindowUserPointer(window));
}

void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
  Application& application{GetApplication(window)};
  InvokeCallback(application.m_callback_exception, [&] { application.OnFramebufferResized(width, height); });
}

void Application::WindowFocusCallback(GLFWwindow* window, int is_focused) {
  Application& application{GetApplication(window)};
  application.m_has_cursor_position = false;
  if (is_focused == GLFW_FALSE && application.m_camera_controller) {
    application.m_camera_controller->ResetState();
  }
}

void Application::CursorPositionCallback(GLFWwindow* window, double cursor_x, double cursor_y) {
  Application& application{GetApplication(window)};
  const double delta_x{application.m_has_cursor_position ? cursor_x - application.m_last_cursor_x : 0.0};
  const double delta_y{application.m_has_cursor_position ? cursor_y - application.m_last_cursor_y : 0.0};
  application.m_last_cursor_x = cursor_x;
  application.m_last_cursor_y = cursor_y;
  application.m_has_cursor_position = true;
  InvokeCallback(application.m_callback_exception, [&] { application.OnCursorMoved(delta_x, delta_y); });
}

void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  Application& application{GetApplication(window)};
  if (action == GLFW_PRESS) {
    glfwGetCursorPos(window, &application.m_last_cursor_x, &application.m_last_cursor_y);
    application.m_has_cursor_position = true;
  }

  auto temp_btn = GLFWButtonToMouseButton(button);
  auto temp_act = GLFWActionToInputAction(action);
  auto temp_mod = GLFWModToModifiers(mods);

  InvokeCallback(application.m_callback_exception, [&] { application.OnMouseButton(temp_btn, temp_act, temp_mod); });
}

void Application::ScrollCallback(GLFWwindow* window, double offset_x, double offset_y) {
  static_cast<void>(offset_x);
  auto temp_mod = GetCurrentModifiers(window);

  Application& application{GetApplication(window)};
  InvokeCallback(application.m_callback_exception, [&] { application.OnScrolled(offset_y, temp_mod); });
}

void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  static_cast<void>(scancode);

  auto temp_key = GLFWKeyToKey(key);
  auto temp_act = GLFWActionToInputAction(action);
  auto temp_mod = GLFWModToModifiers(mods);

  Application& application{GetApplication(window)};
  InvokeCallback(application.m_callback_exception, [&] { application.OnKey(temp_key, temp_act, temp_mod); });
}
