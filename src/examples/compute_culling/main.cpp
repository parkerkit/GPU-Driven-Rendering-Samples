#include <framework.h>
#include <gpu_timer.h>
#include <imgui.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>

namespace {
std::string FormatGpuTiming(const char* label, fw::GpuTimer& timer) {
  const auto milliseconds{timer.TakeAverageMilliseconds()};
  if (milliseconds) {
    return std::format("{}: {:.3f} ms\n", label, *milliseconds);
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
    InitDebugDrawables();

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

    const glm::mat4 view_proj{GetCamera()->GetViewProjectionMatrix()};
    if (!m_enable_frozen_culling || !m_has_culling_view) {
      // 在渲染阶段捕获，使用相机控制器更新后的视角；观察相机仍可继续移动。
      m_culling_view_projection = view_proj;
      m_culling_planes = ExtractFrustumPlanes(m_culling_view_projection);
      m_has_culling_view = true;
    }

    // 复用可见列表前，确保上一帧顶点着色器的 SSBO 读取先完成。
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_cull_timer->Begin();
    GLuint zero{0};
    glClearNamedBufferSubData(m_count_buffer_ssbo->GetHandle(), GL_R32UI, 0, sizeof(GLuint), GL_RED_INTEGER,
                              GL_UNSIGNED_INT, &zero);
    m_cull_shader->Bind();

    m_frustums_ubo->SetData(0, std::as_bytes(std::span(m_culling_planes)));

    m_cull_shader->SetUInt("u_TotalInstances", static_cast<uint32_t>(m_total_instances));

    GLuint group_count = static_cast<GLuint>((m_total_instances + 63) / 64);
    m_cull_shader->Dispatch(group_count);

    // 分别保证后续顶点着色器读取可见列表、CPU 回读计数器时能看到 Compute 的写入。
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    m_cull_timer->End();

    GLuint visible_count{};
    // 此示例保留同步读回，indirect_draw 示例再展示无需 CPU 回读的绘制路径。
    glGetNamedBufferSubData(m_count_buffer_ssbo->GetHandle(), 0, sizeof(visible_count), &visible_count);
    m_visible_count = visible_count;

    m_draw_timer->Begin();
    m_render_shader->Bind();
    m_render_shader->SetMat4("u_ViewProjection", view_proj);
    m_drawable->DrawInstanced(static_cast<GLsizei>(visible_count));
    m_draw_timer->End();
    m_total_timer->End();
    m_cpu_render_sum_ms +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_begin).count();
    ++m_cpu_sample_count;

    // 调试线框独立绘制，避免剔除结果把包围体一起隐藏，也不计入上述场景耗时。
    DrawCullingDebug(view_proj);
  }

  void OnImGui() override {
    ImGui::Begin("Compute Culling");

    DrawCullingControls();

    ImGui::SliderInt("Instance (0-based)", &m_selected_instance, 0, static_cast<int>(m_instances.size()) - 1);
    GpuInstanceData& instance{m_instances[static_cast<std::size_t>(m_selected_instance)]};
    int material_index{static_cast<int>(instance.materialID)};
    constexpr std::array<const char*, 2> k_material_names{"alpha_blend", "wood"};
    if (ImGui::Combo("Material", &material_index, k_material_names.data(), static_cast<int>(k_material_names.size()))) {
      instance.materialID = static_cast<uint32_t>(material_index);
      const auto byte_offset{
        static_cast<GLintptr>(static_cast<std::size_t>(m_selected_instance) * sizeof(GpuInstanceData))};
      m_instances_ssbo->SetData(byte_offset, std::as_bytes(std::span(&instance, 1)));
      ResetTimingSamples();
    }

    ImGui::Separator();
    ImGui::TextUnformatted(std::format("Visible instances: {} / {}", m_visible_count, m_total_instances).c_str());
    ImGui::TextUnformatted("Timing (1 s average)");
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(
        "Frame includes UI / VSync. Scene intervals exclude debug lines and UI.\n"
        "CPU includes synchronous readback; GPU total may include submission gaps.\n"
        "GPU samples are delayed. Draw starts after readback.");
      ImGui::EndTooltip();
    }
    m_frame_time_sum_seconds += static_cast<double>(ImGui::GetIO().DeltaTime);
    ++m_frame_sample_count;
    const auto timing_now{std::chrono::steady_clock::now()};
    if (timing_now - m_timing_update_time >= std::chrono::seconds{1}) {
      const double frame_ms{m_frame_time_sum_seconds * 1000.0 / static_cast<double>(m_frame_sample_count)};
      const double fps{frame_ms > 0.0 ? 1000.0 / frame_ms : 0.0};
      m_timing_text = std::format("Frame: {:.2f} ms | {:.1f} FPS\n", frame_ms, fps);
      if (m_cpu_sample_count > 0) {
        m_timing_text +=
          std::format("CPU render: {:.3f} ms\n", m_cpu_render_sum_ms / static_cast<double>(m_cpu_sample_count));
      } else {
        m_timing_text += "CPU render: no samples\n";
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
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(m_timing_text.empty() ? "Collecting samples..." : m_timing_text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::End();
  }

  void OnShutdown() override {
    m_total_timer.reset();
    m_cull_timer.reset();
    m_draw_timer.reset();

    m_debug_shader.reset();
    m_frustum_drawable.reset();
    m_frustum_vertex_buffer.reset();
    m_sphere_drawable.reset();

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
  void DrawCullingControls() {
    if (ImGui::Checkbox("Freeze culling view", &m_enable_frozen_culling)) {
      m_has_culling_view = false;
      ResetTimingSamples();
    }
    bool has_debug_change{ImGui::Checkbox("Show culling frustum", &m_enable_frustum_display)};
    has_debug_change |= ImGui::Checkbox("Show bounding spheres", &m_enable_bounds_display);
    if (has_debug_change) {
      ResetTimingSamples();
    }
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted("Freeze, then move the camera to inspect.");
    ImGui::TextUnformatted("Yellow: frustum; green/red: pass/reject (CPU).");
    ImGui::PopTextWrapPos();
    ImGui::Separator();
  }

  void InitDebugDrawables() {
    m_debug_shader = std::make_unique<fw::GraphicsShader>(fw::ShaderPaths{
      .vertex = "shaders/compute_culling_debug.vert", .fragment = "shaders/compute_culling_debug.frag"});

    constexpr std::array<std::uint32_t, 24> k_frustum_indices{0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3,
                                                              2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7};
    m_frustum_vertex_buffer =
      std::make_unique<fw::Buffer>(static_cast<GLsizeiptr>(8 * sizeof(glm::vec3)), GL_DYNAMIC_STORAGE_BIT);
    m_frustum_drawable = std::make_unique<fw::Drawable>(GL_LINES);
    m_frustum_drawable->GetVertexArray().SetVertexBuffer(0, *m_frustum_vertex_buffer, 0, sizeof(glm::vec3));
    m_frustum_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_frustum_drawable->SetIndexBuffer(std::span(k_frustum_indices));

    // 三个互相垂直的大圆复用同一份单位球线框，绘制时再按实例包围球平移和缩放。
    constexpr std::size_t k_circle_segments{32};
    std::array<glm::vec3, k_circle_segments * 6> sphere_vertices{};
    for (std::size_t circle{}; circle < 3; ++circle) {
      const auto first_axis{static_cast<glm::length_t>((circle + 1) % 3)};
      const auto second_axis{static_cast<glm::length_t>((circle + 2) % 3)};
      for (std::size_t segment{}; segment < k_circle_segments; ++segment) {
        for (std::size_t endpoint{}; endpoint < 2; ++endpoint) {
          const float angle{2.0F * std::numbers::pi_v<float> * static_cast<float>(segment + endpoint) /
                            static_cast<float>(k_circle_segments)};
          glm::vec3& vertex{sphere_vertices[(circle * k_circle_segments + segment) * 2 + endpoint]};
          vertex[first_axis] = std::cos(angle);
          vertex[second_axis] = std::sin(angle);
        }
      }
    }
    m_sphere_drawable = std::make_unique<fw::Drawable>(GL_LINES);
    m_sphere_drawable->AddVertexBuffer(0, std::span(sphere_vertices), sizeof(glm::vec3));
    m_sphere_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_sphere_drawable->SetVertexCount(static_cast<GLsizei>(sphere_vertices.size()));
  }

  void DrawCullingFrustum() {
    const glm::mat4 inverse_view_projection{glm::inverse(m_culling_view_projection)};
    std::array<glm::vec3, 8> corners{};
    for (std::size_t corner{}; corner < corners.size(); ++corner) {
      // 当前项目使用 OpenGL 默认的 [-1, 1] 裁剪深度范围。
      const glm::vec4 position{inverse_view_projection * glm::vec4{(corner & 1U) != 0 ? 1.0F : -1.0F,
                                                                   (corner & 2U) != 0 ? 1.0F : -1.0F,
                                                                   (corner & 4U) != 0 ? 1.0F : -1.0F, 1.0F}};
      if (!std::isfinite(position.w) || std::abs(position.w) < 1e-6F) {
        return;
      }
      corners[corner] = glm::vec3{position} / position.w;
    }
    m_frustum_vertex_buffer->SetData(0, std::as_bytes(std::span(corners)));
    m_debug_shader->SetMat4("u_Model", glm::mat4{1.0F});
    m_debug_shader->SetVec3("u_Color", glm::vec3{1.0F, 0.85F, 0.05F});
    m_frustum_drawable->Draw();
  }

  bool IsSphereInCullingFrustum(glm::vec4 sphere) const noexcept {
    for (const Plane& plane : m_culling_planes) {
      if (glm::dot(plane.normal, glm::vec3{sphere}) + plane.d < -sphere.w) {
        return false;
      }
    }
    return true;
  }

  void DrawCullingDebug(const glm::mat4& observer_view_projection) {
    if (!m_enable_frustum_display && !m_enable_bounds_display) {
      return;
    }
    const GLboolean depth_enabled{glIsEnabled(GL_DEPTH_TEST)};
    const GLboolean depth_clamp_enabled{glIsEnabled(GL_DEPTH_CLAMP)};
    // 线框作为叠加显示，确保被场景遮挡或剔除的包围体也能被观察。
    glDisable(GL_DEPTH_TEST);
    // 关闭深度测试仍会裁剪线框，仅在调试绘制时取消近、远平面的裁剪。
    glEnable(GL_DEPTH_CLAMP);
    m_debug_shader->Bind();
    m_debug_shader->SetMat4("u_ViewProjection", observer_view_projection);
    if (m_enable_bounds_display) {
      for (const GpuInstanceData& instance : m_instances) {
        const glm::vec4 sphere{instance.boundingSphere};
        const glm::mat4 model{glm::scale(glm::translate(glm::mat4{1.0F}, glm::vec3{sphere}), glm::vec3{sphere.w})};
        m_debug_shader->SetMat4("u_Model", model);
        m_debug_shader->SetVec3(
          "u_Color", IsSphereInCullingFrustum(sphere) ? glm::vec3{0.2F, 1.0F, 0.2F} : glm::vec3{1.0F, 0.2F, 0.2F});
        m_sphere_drawable->Draw();
      }
    }
    if (m_enable_frustum_display) {
      DrawCullingFrustum();
    }
    if (depth_enabled == GL_TRUE) {
      glEnable(GL_DEPTH_TEST);
    }
    if (depth_clamp_enabled == GL_FALSE) {
      glDisable(GL_DEPTH_CLAMP);
    }
  }

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

  std::unique_ptr<fw::GraphicsShader> m_debug_shader{};
  std::unique_ptr<fw::Drawable> m_frustum_drawable{};
  std::unique_ptr<fw::Buffer> m_frustum_vertex_buffer{};
  std::unique_ptr<fw::Drawable> m_sphere_drawable{};
  glm::mat4 m_culling_view_projection{1.0F};
  std::array<Plane, 6> m_culling_planes{};
  bool m_enable_frozen_culling{false};
  bool m_has_culling_view{false};
  bool m_enable_frustum_display{false};
  bool m_enable_bounds_display{false};
  int m_selected_instance{};
  GLuint m_visible_count{};

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
