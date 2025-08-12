// 1. NDC 绘制
// 2. 相机变换
// 3. 计算着色器 + SSBO
// 4. 纹理
// 5. 模型创建 + 相机测试
// 6. meshoptimizer 创建 meshlet

#define TEST6

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
    GetCameraController()->SetSceneBounds(m_aabb);
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

    auto cube = fw::Shapes::Cube(1.0F, {0.0F, 0.0F, 10.0F});

    m_drawable1 = std::make_unique<fw::Drawable>(cube.topology);
    m_drawable1->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_drawable1->SetIndexBuffer(std::span(cube.indices));
    m_drawable1->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    auto sphere = fw::Shapes::Sphere(0.5F, 32, 32, {0.0F, 0.0F, 0.0F});

    m_drawable2 = std::make_unique<fw::Drawable>(sphere.topology);
    m_drawable2->AddVertexBuffer(0, std::span(sphere.attributes.positions), sizeof(glm::vec3));
    m_drawable2->SetIndexBuffer(std::span(sphere.indices));
    m_drawable2->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    m_bounds = fw::AABB::Merge({cube.bounds, sphere.bounds});
    GetCameraController()->SetSceneBounds(m_bounds);
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

#ifdef TEST6

#include <framework.h>
#include <imgui.h>

#include <array>
#include <cstdint>
#include <random>

struct MeshletDrawable {
  std::vector<glm::vec3> vertices{};
  std::vector<std::uint32_t> indices{};
  fw::AABB bounds{};
};

std::vector<MeshletDrawable> BuildMeshlets(const std::vector<glm::vec3>& vertices,
                                           const std::vector<std::uint32_t>& indices) {
  std::vector<MeshletDrawable> result{};

  if (vertices.empty() || indices.empty()) {
    return result;
  }

  if (indices.size() % 3 != 0) {
    return result;  // triangle mesh 必须是 3 的倍数
  }

  constexpr size_t k_max_vertices = 64;
  constexpr size_t k_max_triangles = 96;
  constexpr float k_cone_weight = 0.0;  // 如果不做 Normal Cone culling ，可以设为 0

  // 计算最坏情况下需要多少 Meshlet
  const std::size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), k_max_vertices, k_max_triangles);

  std::vector<meshopt_Meshlet> meshlets(max_meshlets);
  std::vector<std::uint32_t> meshlet_vertices(indices.size());  // local vertex -> original mesh vertex
  std::vector<std::uint8_t> meshlet_triangles(indices.size());  // triangle vertex -> meshlet local vertex

  const size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
                                                     indices.data(), indices.size(), &vertices[0].x, vertices.size(),
                                                     sizeof(glm::vec3), k_max_vertices, k_max_triangles, k_cone_weight);

  meshlets.resize(meshlet_count);
  result.reserve(meshlet_count);

  for (size_t meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
    const meshopt_Meshlet& meshlet = meshlets[meshlet_index];

    MeshletDrawable drawable{};

    drawable.vertices.reserve(meshlet.vertex_count);
    drawable.indices.reserve(meshlet.triangle_count * 3);

    // 生成这个 Drawable 自己的 compact vertex buffer
    for (size_t local_vertex = 0; local_vertex < meshlet.vertex_count; ++local_vertex) {
      // meshlet 内 local vertex -> 原始 Mesh 的 global vertex index
      const uint32_t global_vertex_index = meshlet_vertices[meshlet.vertex_offset + local_vertex];
      drawable.vertices.push_back(vertices[global_vertex_index]);
    }

    // 生成这个 Drawable 自己的 local index buffer
    for (size_t triangle = 0; triangle < meshlet.triangle_count; ++triangle) {
      const size_t offset = meshlet.triangle_offset + triangle * 3;

      // 注意：meshlet_triangles 里面本身已经是 Meshlet-local index。因此直接拿来索引 drawable.vertices 即可。
      const uint32_t i0 = meshlet_triangles[offset + 0];
      const uint32_t i1 = meshlet_triangles[offset + 1];
      const uint32_t i2 = meshlet_triangles[offset + 2];

      drawable.indices.push_back(i0);
      drawable.indices.push_back(i1);
      drawable.indices.push_back(i2);
    }

    // AABB
    for (const glm::vec3& p : drawable.vertices) {
      drawable.bounds.Expand(p);
    }

    result.emplace_back(std::move(drawable));
  }

  return result;
}

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/framework_test_transform.vert",
                                 .fragment = "shaders/framework_test_transform.frag"};
    m_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    auto sphere = fw::Shapes::Sphere(5.0F);

    m_aabb = sphere.bounds;
    GetCameraController()->SetSceneBounds(m_aabb);
    GetCameraController()->ResetCamera(m_aabb);

    m_mesh_drawable = std::make_unique<fw::Drawable>(GL_TRIANGLES);
    m_mesh_drawable->AddVertexBuffer(0, std::span(sphere.attributes.positions), sizeof(glm::vec3));
    m_mesh_drawable->SetIndexBuffer(std::span(sphere.indices));
    m_mesh_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    auto meshlets = BuildMeshlets(sphere.attributes.positions, sphere.indices);
    for (const auto& meshlet : meshlets) {
      auto drawable = std::make_unique<fw::Drawable>(GL_TRIANGLES);
      drawable->AddVertexBuffer(0, std::span(meshlet.vertices), sizeof(glm::vec3));
      drawable->SetIndexBuffer(std::span(meshlet.indices));
      drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
      m_meshlet_drawables.emplace_back(std::move(drawable));
    }

    // 给每个 meshlet 生成一个颜色
    std::random_device rd{};
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0F, 1.0F);

    m_meshlet_colors.resize(meshlets.size());
    std::ranges::generate(m_meshlet_colors, [&]() { return glm::vec3{dis(gen), dis(gen), dis(gen)}; });
  }

  void OnImGui() override {
    ImGui::Begin("ImGui");

    if (ImGui::Button("Reset camera")) {
      GetCameraController()->ResetCamera(m_aabb);
    }

    constexpr int drawable_count{2};
    constexpr std::array<const char*, drawable_count> drawable_names{"Mesh", "Meshlet"};
    ImGui::Combo("Drawable", &m_selected_drawble, drawable_names.data(), drawable_count);

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

    if (m_selected_drawble == 0) {
      m_shader->SetVec3("uColor", glm::vec3(1.0F, 0.0F, 0.0F));
      m_mesh_drawable->Draw();
    } else {
      for (std::size_t i = 0; i < m_meshlet_drawables.size(); ++i) {
        m_shader->SetVec3("uColor", m_meshlet_colors[i]);
        m_meshlet_drawables[i]->Draw();
      }
    }
  }

private:
  std::vector<std::unique_ptr<fw::Drawable>> m_meshlet_drawables{};
  std::vector<glm::vec3> m_meshlet_colors{};
  std::unique_ptr<fw::Drawable> m_mesh_drawable{};
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  fw::AABB m_aabb{};
  int m_selected_drawble = 0;  // 0: Mesh, 1: Meshlet
};

APPLICATION_MAIN(TestApp)

#endif  // TEST6
