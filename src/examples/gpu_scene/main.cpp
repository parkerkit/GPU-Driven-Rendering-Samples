#include <framework.h>
#include <imgui.h>

#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <stdexcept>

// Instance/Object 数据 (存入 SSBO)
struct GpuInstanceData {
  glm::mat4 worldMatrix;              // 16 floats (64 bytes)
  glm::vec4 boundingSphere;           // x, y, z, radius (16 bytes)
  uint32_t meshID;                    // 4 bytes
  uint32_t materialID;                // 4 bytes
  std::array<uint32_t, 2> padding{};  // 8 bytes -> 对齐到 16 字节
};

// Material 数据 (存入 SSBO)
struct GpuMaterialData {
  glm::vec4 baseColorFactor;    // 16 bytes
  uint64_t baseColorTexHandle;  // 8 bytes: Bindless Texture 64-bit 句柄 (0 表示无)
  uint64_t normalTexHandle;     // 8 bytes: 法线贴图句柄
  uint64_t roughnessTexHandle;  // 8 bytes
  uint64_t metallicTexHandle;   // 8 bytes
};

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    if (GLAD_GL_ARB_bindless_texture == 0 || GLAD_GL_ARB_gpu_shader_int64 == 0) {
      throw std::runtime_error{"Requires GL_ARB_bindless_texture and GL_ARB_gpu_shader_int64."};
    }

    fw::ShaderPaths shader_paths{.vertex = "shaders/gpu_scene.vert", .fragment = "shaders/gpu_scene.frag"};
    m_graphics_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    auto cube = fw::Shapes::Cube();
    m_total_bounds = cube.bounds;

    m_drawable = std::make_unique<fw::Drawable>(cube.topology);
    m_drawable->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_drawable->AddVertexBuffer(1, std::span(cube.attributes.texcoord_sets[0]), sizeof(glm::vec2));
    m_drawable->SetIndexBuffer(std::span(cube.indices));
    m_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_drawable->GetVertexArray().SetAttribute(1, 1, 2, GL_FLOAT, 0);

    InitMaterials();
    InitInstances();

    m_instances_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_instances)), GL_DYNAMIC_STORAGE_BIT);
    m_materials_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_materials)), GL_DYNAMIC_STORAGE_BIT);

    m_instances_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
    m_materials_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 1);

    GetCameraController()->SetSceneBounds(m_total_bounds);
    GetCameraController()->ResetCamera(m_total_bounds);
  }

  void OnRender() override {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto view_proj = GetCamera()->GetViewProjectionMatrix();

    m_graphics_shader->Bind();
    m_graphics_shader->SetMat4("u_ViewProjection", view_proj);
    m_drawable->DrawInstanced(2);
  }

  void OnImGui() override {
    ImGui::Begin("Material");

    static int s_material_index1{0};
    constexpr int material_count1{2};
    constexpr std::array<const char*, material_count1> material_names1{"alpha_blend", "wood"};
    if (ImGui::Combo("instance 1", &s_material_index1, material_names1.data(), material_count1)) {
      m_instances[0].materialID = static_cast<uint32_t>(s_material_index1);
      m_instances_ssbo->SetData(0, std::as_bytes(std::span(m_instances)));
    }

    static int s_material_index2{1};
    constexpr int material_count2{2};
    constexpr std::array<const char*, material_count2> material_names2{"alpha_blend", "wood"};
    if (ImGui::Combo("instance 2", &s_material_index2, material_names2.data(), material_count2)) {
      m_instances[1].materialID = static_cast<uint32_t>(s_material_index2);
      m_instances_ssbo->SetData(0, std::as_bytes(std::span(m_instances)));
    }

    ImGui::End();
  }

  void OnShutdown() override {
    for (const GpuMaterialData& material : m_materials) {
      if (material.baseColorTexHandle != 0) {
        glMakeTextureHandleNonResidentARB(material.baseColorTexHandle);
      }
    }

    m_texture2.reset();
    m_texture1.reset();
    m_materials_ssbo.reset();
    m_instances_ssbo.reset();
    m_drawable.reset();
    m_graphics_shader.reset();
  }

private:
  void InitMaterials() {
    auto create_texture = [](const std::filesystem::path& file_path) {
      fw::Image image{file_path};

      auto texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
      texture->Storage2D(1, image.GetGLInternalFormat(), image.GetWidth(), image.GetHeight());
      texture->SetSubImage2D(0, 0, 0, image.GetWidth(), image.GetHeight(), image.GetGLFormat(), GL_UNSIGNED_BYTE,
                             image.GetPixels());
      texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      return texture;
    };

    m_texture1 = create_texture(ASSETS_PATH "textures/alpha_blend.png");
    m_texture2 = create_texture(ASSETS_PATH "textures/wood.png");

    m_materials.resize(2);

    GLuint64 texture_handle1 = glGetTextureHandleARB(m_texture1->GetHandle());
    glMakeTextureHandleResidentARB(texture_handle1);
    m_materials[0].baseColorTexHandle = texture_handle1;
    m_materials[0].baseColorFactor = glm::vec4(1.0F);

    GLuint64 texture_handle2 = glGetTextureHandleARB(m_texture2->GetHandle());
    glMakeTextureHandleResidentARB(texture_handle2);
    m_materials[1].baseColorTexHandle = texture_handle2;
    m_materials[1].baseColorFactor = glm::vec4(1.0F);
  }

  void InitInstances() {
    m_instances.resize(2);

    m_instances[0].materialID = 0;
    m_instances[0].worldMatrix = glm::translate(glm::mat4(1.0F), glm::vec3(-0.6F, 0.0F, 0.0F));
    m_instances[1].materialID = 1;
    m_instances[1].worldMatrix = glm::translate(glm::mat4(1.0F), glm::vec3(0.6F, 0.0F, 0.0F));

    const auto& min = m_total_bounds.GetMin();
    const auto& max = m_total_bounds.GetMax();

    m_total_bounds.Expand({min.x - 0.6F, min.y, min.z});
    m_total_bounds.Expand({max.x + 0.6F, max.y, max.z});
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_graphics_shader{};
  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture1{};
  std::unique_ptr<fw::Texture> m_texture2{};

  std::unique_ptr<fw::Buffer> m_instances_ssbo{};
  std::unique_ptr<fw::Buffer> m_materials_ssbo{};

  std::vector<GpuInstanceData> m_instances{};
  std::vector<GpuMaterialData> m_materials{};
  fw::AABB m_total_bounds{};
};

APPLICATION_MAIN(TestApp)
