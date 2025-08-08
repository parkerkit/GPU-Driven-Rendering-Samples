// 1. Texture Bindless
// 2. Buffer Bindless

#define TEST2

#ifdef TEST1

#include <framework.h>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    // 检查显卡是否支持 Bindless Texture 扩展
    if (!GLAD_GL_ARB_bindless_texture) {
      throw std::runtime_error("GL_ARB_bindless_texture is NOT supported on this GPU!");
    }

    fw::ShaderPaths shader_paths{.vertex = "shaders/texture_bindless.vert",
                                 .fragment = "shaders/texture_bindless.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    struct Vertex {
      glm::vec2 pos{};
      glm::vec2 tex_coord{};
    };

    std::vector<Vertex> vertices{
      {{-0.5F, -0.5F}, {0.0F, 0.0F}},
      {{0.5F, -0.5F}, {1.0F, 0.0F}},
      {{0.5F, 0.5F}, {1.0F, 1.0F}},
      {{-0.5F, 0.5F}, {0.0F, 1.0F}},
    };

    m_drawable = std::make_unique<fw::Drawable>(GL_TRIANGLE_FAN);
    m_drawable->AddVertexBuffer(0, std::span(vertices), sizeof(Vertex));
    m_drawable->SetVertexCount(static_cast<GLsizei>(vertices.size()));

    auto& vao = m_drawable->GetVertexArray();
    vao.SetAttribute(0, 0, 2, GL_FLOAT, offsetof(Vertex, pos));
    vao.SetAttribute(1, 0, 2, GL_FLOAT, offsetof(Vertex, tex_coord));

    fw::Image image{ASSETS_PATH "textures/alpha_blend.png"};

    m_texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
    m_texture->Storage2D(1, image.GetGLInternalFormat(), image.GetWidth(), image.GetHeight());
    m_texture->SetSubImage2D(0, 0, 0, image.GetWidth(), image.GetHeight(), image.GetGLFormat(), GL_UNSIGNED_BYTE,
                             image.GetPixels());
    m_texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 获取 64 位纹理句柄 (GLuint64)
    m_texture_handle = glGetTextureHandleARB(m_texture->GetHandle());
    // 将句柄设置为“驻留(Resident)”显存状态（非常重要，不设置会导致崩溃）
    glMakeTextureHandleResidentARB(m_texture_handle);
  }

  void OnRender() override {
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->Bind();
    // 直接将 64 位句柄作为 Uniform 传给 Shader（无需调用 glBindTexture 绑定到槽位）
    GLint handle_location = glGetUniformLocation(m_shader->GetHandle(), "u_BindlessTexture");
    glUniformHandleui64ARB(handle_location, m_texture_handle);
    // 直接渲染即可，不需要 glActiveTexture 和 glBindTexture
    m_drawable->Draw();
  }

  void OnShutdown() override { glMakeTextureHandleNonResidentARB(m_texture_handle); }

private:
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture{};
  GLuint64 m_texture_handle{};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST1

#ifdef TEST2

#include <framework.h>
#include <imgui.h>

#include <array>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    // 检查 Bindless Buffer 扩展支持
    if (!GLAD_GL_NV_shader_buffer_load) {
      throw std::runtime_error("GL_NV_shader_buffer_load is NOT supported on this GPU!");
    }

    fw::ShaderPaths shader_paths{.vertex = "shaders/buffer_bindless.vert", .fragment = "shaders/buffer_bindless.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    struct Vertex {
      float x, y, z;
      float r, g, b;
    };

    std::vector<Vertex> vertices{
      {-0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F},
      {0.5F, -0.5F, 0.0F, 0.0F, 1.0F, 0.0F},
      {0.0F, 0.5F, 0.0F, 0.0F, 0.0F, 1.0F},
    };

    m_vbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(vertices)), 0);

    // 获取 Buffer 的 64 位 GPU 虚拟地址 (Bindless GPU Address)
    glGetNamedBufferParameterui64vNV(m_vbo->GetHandle(), GL_BUFFER_GPU_ADDRESS_NV, &m_buffer_gpu_addr);
    // 将该 Buffer 标记为 Resident（激活该地址，允许 Shader 驻留读取）
    glMakeNamedBufferResidentNV(m_vbo->GetHandle(), GL_READ_ONLY);

    // VAO 是 Core Profile 必须绑定的，但不需要用 glVertexAttribPointer 配置属性布局
    glGenVertexArrays(1, &m_vao);
  }

  void OnImGui() override {
    ImGui::Begin("ImGui");

    if (ImGui::Button("Reset camera")) {
      GetCameraController()->ResetCamera(m_bounds);
    }

    int camera_type_index{static_cast<int>(GetCamera()->GetProjectionType())};
    constexpr int camera_type_count{2};
    constexpr std::array<const char*, camera_type_count> camera_type_names{"Perspective", "Orthographic"};
    if (ImGui::Combo("Camera type", &camera_type_index, camera_type_names.data(), camera_type_count)) {
      GetCameraController()->SetProjectionType(static_cast<fw::Camera::ProjectionType>(camera_type_index));
    }

    static int s_view_mode_index{};
    constexpr int view_mode_count{7};
    constexpr std::array<const char*, view_mode_count> view_mode_names = {"Custom", "Top",  "Bottom", "Front",
                                                                          "Back",   "Left", "Right"};
    if (ImGui::Combo("View mode", &s_view_mode_index, view_mode_names.data(), view_mode_count)) {
      GetCameraController()->SetViewMode(static_cast<fw::CameraController::ViewMode>(s_view_mode_index), m_bounds);
    }

    ImGui::End();
  }

  void OnRender() override {
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader->Bind();
    GLint addr_location = glGetUniformLocation(m_shader->GetHandle(), "u_VertexBufferAddr");
    // 不需要 glBindBufferBase，直接将 64 位 GPU 地址作为 Uniform 传给 Shader
    glUniformui64NV(addr_location, m_buffer_gpu_addr);

    glBindVertexArray(m_vao);
    // 使用 glDrawArrays 触发 3 个顶点的绘制，Shader 内部通过地址和 gl_VertexID 自行 Fetch 数据
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  void OnShutdown() override { glMakeNamedBufferNonResidentNV(m_vbo->GetHandle()); }

private:
  std::unique_ptr<fw::Drawable> m_drawable1{};
  std::unique_ptr<fw::Drawable> m_drawable2{};
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  std::unique_ptr<fw::Buffer> m_vbo{};
  fw::AABB m_bounds{};
  GLuint m_vao{};
  GLuint64EXT m_buffer_gpu_addr{0};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST2
