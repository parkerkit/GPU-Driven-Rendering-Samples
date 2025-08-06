// 1. NDC 绘制
// 2. 相机变换
// 3. 计算着色器 + SSBO
// 4. 纹理

#define TEST5

#ifdef TEST1

#include <framework.h>
#include <imgui.h>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_ndc.vert",
                                 .fragment = "shaders/framework_test_ndc.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    // Interleaved VBO + IBO
    {
      struct Vertex {
        glm::vec2 pos{};
        glm::vec3 color{};
      };

      std::vector<Vertex> vertices{
        {{-0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}},
        {{0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}},
        {{0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
        {{-0.5F, 0.5F}, {1.0F, 1.0F, 1.0F}},
      };
      std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};

      m_drawable1 = std::make_unique<fw::Drawable>(GL_TRIANGLES);
      m_drawable1->AddVertexBuffer(0, std::span(vertices), sizeof(Vertex));
      m_drawable1->SetIndexBuffer(std::span(indices));

      auto& vao = m_drawable1->GetVertexArray();
      vao.SetAttribute(0, 0, 2, GL_FLOAT, offsetof(Vertex, pos));
      vao.SetAttribute(1, 0, 3, GL_FLOAT, offsetof(Vertex, color));
    }

    // Separate VBOs
    {
      std::vector<glm::vec2> pos{{-0.8F, -0.8F}, {0.8F, -0.8F}, {0.8F, 0.8F}, {-0.8F, 0.8F}};
      std::vector<glm::vec3> colors{{1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F}};

      m_drawable2 = std::make_unique<fw::Drawable>(GL_LINES);
      m_drawable2->AddVertexBuffer(0, std::span(pos), sizeof(glm::vec2));
      m_drawable2->AddVertexBuffer(1, std::span(colors), sizeof(glm::vec3));
      m_drawable2->SetVertexCount(static_cast<GLsizei>(pos.size()));

      auto& vao = m_drawable2->GetVertexArray();
      vao.SetAttribute(0, 0, 2, GL_FLOAT, 0);
      vao.SetAttribute(1, 1, 3, GL_FLOAT, 0);
    }
  }

  void OnImGui() override {
    ImGui::Begin("Information");
    ImGui::TextUnformatted("Hello World");
    ImGui::End();
  }

  void OnRender() override {
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader->Bind();
    m_drawable1->Draw();
    m_drawable2->Draw();
  }

private:
  std::unique_ptr<fw::Drawable> m_drawable1{};
  std::unique_ptr<fw::Drawable> m_drawable2{};
  std::unique_ptr<fw::GraphicsShader> m_shader{};
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
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_transform.vert",
                                 .fragment = "shaders/framework_test_transform.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    {
      std::vector<glm::vec3> vertices{{-0.5F, -0.5F, 0.0F}, {0.5F, -0.5F, 0.0F}, {0.0F, 0.5F, 0.0F}};
      m_drawable1 = std::make_unique<fw::Drawable>(GL_TRIANGLES);
      m_drawable1->AddVertexBuffer(0, std::span(vertices), sizeof(glm::vec3));
      m_drawable1->SetVertexCount(static_cast<GLsizei>(vertices.size()));

      auto& vao = m_drawable1->GetVertexArray();
      vao.SetAttribute(0, 0, 3, GL_FLOAT, 0);
    }

    {
      std::vector<glm::vec3> vertices{
        {-0.5F, -0.5F, 0.1F},
        {0.5F, -0.5F, 0.1F},
        {0.5F, 0.5F, 0.1F},
        {-0.5F, 0.5F, 0.1F},
      };
      std::vector<uint16_t> indices{0, 1, 2, 3};

      m_drawable2 = std::make_unique<fw::Drawable>(GL_LINE_LOOP);
      m_drawable2->AddVertexBuffer(0, std::span(vertices), sizeof(glm::vec3));
      m_drawable2->SetIndexBuffer(std::span(indices));

      auto& vao = m_drawable2->GetVertexArray();
      vao.SetAttribute(0, 0, 3, GL_FLOAT, 0);
    }

    m_aabb = {glm::vec3(-0.5F), glm::vec3(0.5F)};
  }

  void OnImGui() override {
    ImGui::Begin("ImGui");

    if (ImGui::Button("Reset camera")) {
      GetCameraController()->ResetCamera(m_aabb);
    }

    static int s_view_mode_index{};
    constexpr int view_mode_count{7};
    constexpr std::array<const char*, view_mode_count> view_mode_names = {"Custom", "Top",  "Bottom", "Front",
                                                                          "Back",   "Left", "Right"};
    if (ImGui::Combo("View mode", &s_view_mode_index, view_mode_names.data(), view_mode_count)) {
      GetCameraController()->SetViewMode(static_cast<fw::CameraController::ViewMode>(s_view_mode_index), m_aabb);
    }

    ImGui::End();
  }

  void OnRender() override {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto view_mat = GetCamera()->GetViewMatrix();
    auto proj_mat = GetCamera()->GetProjectionMatrix();

    m_shader->Bind();
    m_shader->SetMat4("uView", view_mat);
    m_shader->SetMat4("uProj", proj_mat);

    m_shader->SetVec3("uColor", glm::vec3(1.0F, 0.0F, 0.0F));
    m_drawable1->Draw();
    m_shader->SetVec3("uColor", glm::vec3(0.0F, 1.0F, 0.0F));
    m_drawable2->Draw();
  }

private:
  std::unique_ptr<fw::Drawable> m_drawable1{};
  std::unique_ptr<fw::Drawable> m_drawable2{};
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  fw::AABB m_aabb{};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST2

#ifdef TEST3

#include <framework.h>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_ndc.vert",
                                 .fragment = "shaders/framework_test_ndc.frag"};
    m_graphics_shader = std::make_unique<fw::GraphicsShader>(shader_paths);
    m_compute_shader = std::make_unique<fw::ComputeShader>("shaders/framework_test_compute.comp");

    struct Vertex {
      glm::vec2 pos{};
      glm::vec2 padding{};  // Padding to align the next member to a 16-byte boundary
      glm::vec3 color{};
      float padding2{};  // Padding to align the struct size to a multiple of 16 bytes
    };

    m_ssbo = std::make_unique<fw::Buffer>(3 * sizeof(Vertex), GL_DYNAMIC_STORAGE_BIT);
    m_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);

    m_vao = std::make_unique<fw::VertexArray>();
    m_vao->SetVertexBuffer(0, *m_ssbo, 0, sizeof(Vertex));
    m_vao->SetAttribute(0, 0, 2, GL_FLOAT, offsetof(Vertex, pos));
    m_vao->SetAttribute(1, 0, 3, GL_FLOAT, offsetof(Vertex, color));
  }

  void OnRender() override {
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    m_compute_shader->Bind();
    m_compute_shader->SetFloat("u_angle", static_cast<float>(glfwGetTime()));
    m_compute_shader->Dispatch(1, 1, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    m_graphics_shader->Bind();
    m_vao->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_graphics_shader{};
  std::unique_ptr<fw::ComputeShader> m_compute_shader{};

  std::unique_ptr<fw::VertexArray> m_vao{};
  std::unique_ptr<fw::Buffer> m_ssbo{};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST3

#ifdef TEST4

#include <framework.h>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_texture.vert",
                                 .fragment = "shaders/framework_test_texture.frag"};
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
    m_texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  void OnRender() override {
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->Bind();
    m_texture->BindUnit(0);
    m_drawable->Draw();
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture{};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST4

#ifdef TEST5

#include <framework.h>
#include <imgui.h>

#include <array>
#include <glm/gtc/matrix_transform.hpp>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_transform.vert",
                                 .fragment = "shaders/framework_test_transform.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    auto cube = fw::Shapes::Cube(1.0F, {-0.5F, 0.0F, 0.0F});

    m_drawable1 = std::make_unique<fw::Drawable>(cube.topology);
    m_drawable1->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_drawable1->SetIndexBuffer(std::span(cube.indices));
    m_drawable1->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    auto sphere = fw::Shapes::Sphere(0.5F, 32, 32, {0.5F, 0.0F, 0.0F});

    m_drawable2 = std::make_unique<fw::Drawable>(sphere.topology);
    m_drawable2->AddVertexBuffer(0, std::span(sphere.attributes.positions), sizeof(glm::vec3));
    m_drawable2->SetIndexBuffer(std::span(sphere.indices));
    m_drawable2->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    m_bounds = fw::AABB::Merge({cube.bounds, sphere.bounds});
  }

  void OnImGui() override {
    ImGui::Begin("ImGui");

    if (ImGui::Button("Reset camera")) {
      GetCameraController()->ResetCamera(m_bounds);
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
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto view_mat = GetCamera()->GetViewMatrix();
    auto proj_mat = GetCamera()->GetProjectionMatrix();

    m_shader->Bind();
    m_shader->SetMat4("uView", view_mat);
    m_shader->SetMat4("uProj", proj_mat);

    m_shader->SetVec3("uColor", glm::vec3(1.0F, 0.0F, 0.0F));
    m_drawable1->Draw();
    m_shader->SetVec3("uColor", glm::vec3(0.0F, 1.0F, 0.0F));
    m_drawable2->Draw();
  }

private:
  std::unique_ptr<fw::Drawable> m_drawable1{};
  std::unique_ptr<fw::Drawable> m_drawable2{};
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  fw::AABB m_bounds{};
};

APPLICATION_MAIN(TestApp)

#endif  // TEST5
