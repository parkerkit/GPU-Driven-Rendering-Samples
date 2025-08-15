#include <framework.h>
#include <gpu_timer.h>
#include <imgui.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::string FormatGpuTiming(const char* label, fw::GpuTimer& timer) {
  const auto milliseconds{timer.TakeAverageMilliseconds()};
  if (milliseconds) {
    return std::format("{} avg: {:.3f} ms\n", label, *milliseconds);
  }
  return std::format("{}: no new samples\n", label);
}
}  // namespace

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

struct Plane {
  glm::vec3 normal;
  float d;
};

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    if (GLAD_GL_ARB_bindless_texture == 0 || GLAD_GL_ARB_gpu_shader_int64 == 0) {
      throw std::runtime_error{"Requires GL_ARB_bindless_texture and GL_ARB_gpu_shader_int64."};
    }

    fw::ShaderPaths shader_paths{.vertex = "shaders/compute_culling.vert", .fragment = "shaders/compute_culling.frag"};
    m_render_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    m_cull_shader = std::make_unique<fw::ComputeShader>("shaders/compute_culling.comp");

    InitDrawable();
    InitMaterials();
    InitInstances();

    m_instances_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_instances)), GL_DYNAMIC_STORAGE_BIT);
    m_instances_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);

    m_materials_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_materials)), GL_DYNAMIC_STORAGE_BIT);
    m_materials_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 1);

    GLsizeiptr size{static_cast<GLsizeiptr>(sizeof(uint32_t) * m_total_instances)};
    m_visible_instances_ssbo = std::make_unique<fw::Buffer>(size, GL_DYNAMIC_STORAGE_BIT);
    m_visible_instances_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 2);

    GLuint zero{0};
    m_count_buffer_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(&zero, 1)), GL_DYNAMIC_STORAGE_BIT);
    m_count_buffer_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 3);

    m_frustums_ubo = std::make_unique<fw::Buffer>(sizeof(Plane) * 6, GL_DYNAMIC_STORAGE_BIT);
    m_frustums_ubo->BindBase(GL_UNIFORM_BUFFER, 0);

    GetCameraController()->SetSceneBounds(m_total_bounds);
    GetCameraController()->ResetCamera(m_total_bounds);

    m_total_timer = std::make_unique<fw::GpuTimer>();
    m_cull_timer = std::make_unique<fw::GpuTimer>();
    m_draw_timer = std::make_unique<fw::GpuTimer>();
    ResetTimingSamples();
  }

  void OnRender() override {
    const auto cpu_begin{std::chrono::steady_clock::now()};
    m_total_timer->Begin();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto view_proj = GetCamera()->GetViewProjectionMatrix();

    // 复用可见列表前，确保上一帧顶点着色器的 SSBO 读取先完成。
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_cull_timer->Begin();
    GLuint zero{0};
    glClearNamedBufferSubData(m_count_buffer_ssbo->GetHandle(), GL_R32UI, 0, sizeof(GLuint), GL_RED_INTEGER,
                              GL_UNSIGNED_INT, &zero);
    m_cull_shader->Bind();

    auto frustum_planes = ExtractFrustumPlanes(view_proj);
    m_frustums_ubo->SetData(0, std::as_bytes(std::span(frustum_planes)));

    m_cull_shader->SetUInt("u_TotalInstances", static_cast<uint32_t>(m_total_instances));

    GLuint group_count = static_cast<GLuint>((m_total_instances + 63) / 64);
    m_cull_shader->Dispatch(group_count);

    // 分别保证后续顶点着色器读取可见列表、CPU 回读计数器时能看到 Compute 的写入。
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    m_cull_timer->End();

    GLuint visible_count{};
    // 此示例保留同步读回，indirect_draw 示例再展示无需 CPU 回读的绘制路径。
    glGetNamedBufferSubData(m_count_buffer_ssbo->GetHandle(), 0, sizeof(visible_count), &visible_count);
    std::cout << "visible count: " << visible_count << '\n';

    m_draw_timer->Begin();
    m_render_shader->Bind();
    m_render_shader->SetMat4("u_ViewProjection", view_proj);
    m_drawable->DrawInstanced(static_cast<GLsizei>(visible_count));
    m_draw_timer->End();
    m_total_timer->End();
    m_cpu_render_sum_ms +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_begin).count();
    ++m_cpu_sample_count;
  }

  void OnImGui() override {
    ImGui::Begin("Material");

    static int s_material_index1{0};
    constexpr int material_count1{2};
    constexpr std::array<const char*, material_count1> material_names1{"alpha_blend", "wood"};
    if (ImGui::Combo("instance 1", &s_material_index1, material_names1.data(), material_count1)) {
      m_instances[0].materialID = static_cast<uint32_t>(s_material_index1);
      m_instances_ssbo->SetData(0, std::as_bytes(std::span(m_instances)));
      ResetTimingSamples();
    }

    static int s_material_index2{1};
    constexpr int material_count2{2};
    constexpr std::array<const char*, material_count2> material_names2{"alpha_blend", "wood"};
    if (ImGui::Combo("instance 2", &s_material_index2, material_names2.data(), material_count2)) {
      m_instances[1].materialID = static_cast<uint32_t>(s_material_index2);
      m_instances_ssbo->SetData(0, std::as_bytes(std::span(m_instances)));
      ResetTimingSamples();
    }

    ImGui::Separator();
    m_frame_time_sum_seconds += static_cast<double>(ImGui::GetIO().DeltaTime);
    ++m_frame_sample_count;
    const auto timing_now{std::chrono::steady_clock::now()};
    if (timing_now - m_timing_update_time >= std::chrono::seconds{1}) {
      const double frame_ms{m_frame_time_sum_seconds * 1000.0 / static_cast<double>(m_frame_sample_count)};
      const double fps{frame_ms > 0.0 ? 1000.0 / frame_ms : 0.0};
      m_timing_text = std::format("Frame avg: {:.2f} ms | {:.1f} FPS (includes UI / VSync)\n", frame_ms, fps);
      if (m_cpu_sample_count > 0) {
        m_timing_text +=
          std::format("CPU render avg: {:.3f} ms\n", m_cpu_render_sum_ms / static_cast<double>(m_cpu_sample_count));
      } else {
        m_timing_text += "CPU render avg: no samples\n";
      }
      m_timing_text += FormatGpuTiming("GPU total", *m_total_timer);
      m_timing_text += FormatGpuTiming("Cull", *m_cull_timer);
      m_timing_text += FormatGpuTiming("Draw", *m_draw_timer);
      m_cpu_render_sum_ms = 0.0;
      m_cpu_sample_count = 0;
      m_frame_time_sum_seconds = 0.0;
      m_frame_sample_count = 0;
      m_timing_update_time = timing_now;
    }
    ImGui::TextUnformatted(m_timing_text.empty() ? "Collecting 1-second timing samples..." : m_timing_text.c_str());
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(
      "Averages update once per second. CPU / GPU intervals exclude UI and swap; GPU samples are delayed.");
    ImGui::TextUnformatted(
      "CPU time includes synchronous readback and console output. GPU total can include submission gaps; "
      "Draw starts after readback.");
    ImGui::PopTextWrapPos();
    ImGui::End();
  }

  void OnShutdown() override {
    m_total_timer.reset();
    m_cull_timer.reset();
    m_draw_timer.reset();

    for (const GpuMaterialData& material : m_materials) {
      if (material.baseColorTexHandle != 0) {
        glMakeTextureHandleNonResidentARB(material.baseColorTexHandle);
      }
    }

    m_texture2.reset();
    m_texture1.reset();
    m_frustums_ubo.reset();
    m_count_buffer_ssbo.reset();
    m_visible_instances_ssbo.reset();
    m_materials_ssbo.reset();
    m_instances_ssbo.reset();
    m_drawable.reset();
    m_cull_shader.reset();
    m_render_shader.reset();
  }

private:
  void ResetTimingSamples() noexcept {
    // 更换测量条件时，丢弃旧平均值以及 GPU 中尚未返回的旧条件样本。
    m_total_timer->ResetSamples();
    m_cull_timer->ResetSamples();
    m_draw_timer->ResetSamples();
    m_cpu_render_sum_ms = 0.0;
    m_cpu_sample_count = 0;
    m_frame_time_sum_seconds = 0.0;
    m_frame_sample_count = 0;
    m_timing_text.clear();
    m_timing_update_time = std::chrono::steady_clock::now();
  }

  void InitDrawable() {
    auto cube = fw::Shapes::Cube();
    m_cube_bounds = cube.bounds;

    m_drawable = std::make_unique<fw::Drawable>(cube.topology);
    m_drawable->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_drawable->AddVertexBuffer(1, std::span(cube.attributes.texcoord_sets[0]), sizeof(glm::vec2));
    m_drawable->SetIndexBuffer(std::span(cube.indices));
    m_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_drawable->GetVertexArray().SetAttribute(1, 1, 2, GL_FLOAT, 0);
  }

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
    m_instances.reserve(m_total_instances);
    constexpr float spacing = 2.0F;
    for (std::size_t x = 0; x < m_grid_dim; ++x) {
      for (std::size_t y = 0; y < m_grid_dim; ++y) {
        for (std::size_t z = 0; z < m_grid_dim; ++z) {
          auto model_mat =
            glm::translate(glm::mat4(1.0F), glm::vec3(spacing * static_cast<float>(x), spacing * static_cast<float>(y),
                                                      spacing * static_cast<float>(z)));
          fw::AABB instance_bounds = TransformAABB(m_cube_bounds, model_mat);

          GpuInstanceData instance{};
          instance.worldMatrix = model_mat;
          instance.boundingSphere = glm::vec4(instance_bounds.GetCenter(), instance_bounds.GetBoundingRadius());
          instance.materialID = 0;

          m_total_bounds.Merge(instance_bounds);
          m_instances.emplace_back(instance);
        }
      }
    }

    m_instances[1].materialID = 1;
  }

  fw::AABB TransformAABB(fw::AABB local_bounds, const glm::mat4& model_mat) const {
    glm::vec3 center = local_bounds.GetCenter();
    glm::vec3 extent = local_bounds.GetExtent();

    glm::vec3 new_center = glm::vec3(model_mat * glm::vec4(center, 1.0F));
    glm::vec3 new_extent = glm::vec3(std::abs(model_mat[0][0]) * extent.x + std::abs(model_mat[1][0]) * extent.y +
                                       std::abs(model_mat[2][0]) * extent.z,
                                     std::abs(model_mat[0][1]) * extent.x + std::abs(model_mat[1][1]) * extent.y +
                                       std::abs(model_mat[2][1]) * extent.z,
                                     std::abs(model_mat[0][2]) * extent.x + std::abs(model_mat[1][2]) * extent.y +
                                       std::abs(model_mat[2][2]) * extent.z);

    return {new_center - new_extent, new_center + new_extent};
  }

  /**
   * @brief 从视图投影矩阵 (VP) 中提取视锥体的 6 个平面参数
   * @details 采用 Gribb-Hartmann 算法提取视锥体的 Left, Right, Bottom, Top, Near, Far 6 个边界平面。
   *          提取出的平面方程形式为 normal.x * x + normal.y * y + normal.z * z + d = 0，
   *          其中法线向量 (normal) 均归一化并指向视锥体【内部】。
   *          若点 P 满足 glm::dot(normal, P) + d >= 0，则表明点 P 在该平面的内侧（视锥体内）。
   *
   * @param vp 视图投影矩阵 (View-Projection Matrix)
   */
  std::array<Plane, 6> ExtractFrustumPlanes(const glm::mat4& vp) const {
    std::array<Plane, 6> planes{};

    // Left
    planes[0].normal = glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]);
    planes[0].d = vp[3][3] + vp[3][0];
    // Right
    planes[1].normal = glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]);
    planes[1].d = vp[3][3] - vp[3][0];
    // Bottom
    planes[2].normal = glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]);
    planes[2].d = vp[3][3] + vp[3][1];
    // Top
    planes[3].normal = glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]);
    planes[3].d = vp[3][3] - vp[3][1];
    // Near
    planes[4].normal = glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]);
    planes[4].d = vp[3][3] + vp[3][2];
    // Far
    planes[5].normal = glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]);
    planes[5].d = vp[3][3] - vp[3][2];

    // 归一化平面法线，使距离计算准确
    for (int i = 0; i < 6; ++i) {
      float length = glm::length(planes[i].normal);
      planes[i].normal /= length;
      planes[i].d /= length;
    }

    return planes;
  }

private:
  std::unique_ptr<fw::GpuTimer> m_total_timer{};
  std::unique_ptr<fw::GpuTimer> m_cull_timer{};
  std::unique_ptr<fw::GpuTimer> m_draw_timer{};
  double m_cpu_render_sum_ms{};
  std::size_t m_cpu_sample_count{};
  double m_frame_time_sum_seconds{};
  std::size_t m_frame_sample_count{};
  std::chrono::steady_clock::time_point m_timing_update_time{};
  std::string m_timing_text{};

  std::unique_ptr<fw::GraphicsShader> m_render_shader{};
  std::unique_ptr<fw::ComputeShader> m_cull_shader{};

  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture1{};
  std::unique_ptr<fw::Texture> m_texture2{};

  std::unique_ptr<fw::Buffer> m_instances_ssbo{};          // 实例
  std::unique_ptr<fw::Buffer> m_materials_ssbo{};          // 材质
  std::unique_ptr<fw::Buffer> m_visible_instances_ssbo{};  // 可见实例列表索引
  std::unique_ptr<fw::Buffer> m_count_buffer_ssbo{};       // 原子计数器

  std::unique_ptr<fw::Buffer> m_frustums_ubo{};

  std::size_t m_grid_dim{4};
  std::size_t m_total_instances{m_grid_dim * m_grid_dim * m_grid_dim};
  fw::AABB m_total_bounds{};
  fw::AABB m_cube_bounds{};

  std::vector<GpuInstanceData> m_instances{};
  std::vector<GpuMaterialData> m_materials{};
};

APPLICATION_MAIN(TestApp)
