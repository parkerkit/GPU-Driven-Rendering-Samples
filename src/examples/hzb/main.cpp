#include <framework.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

struct GpuInstanceData {
  glm::mat4 world_matrix{};
  glm::vec4 bounding_sphere{};
  glm::vec4 bounding_box_min{};
  glm::vec4 bounding_box_max{};
  std::uint32_t material_id{};
  std::array<std::uint32_t, 3> padding{};
};
static_assert(sizeof(GpuInstanceData) == 128);

struct GpuMaterialData {
  glm::vec4 base_color_factor{};
  std::uint64_t base_color_texture_handle{};
  std::uint64_t normal_texture_handle{};
  std::uint64_t roughness_texture_handle{};
  std::uint64_t metallic_texture_handle{};
};
static_assert(sizeof(GpuMaterialData) == 48);

struct DrawElementsIndirectCommand {
  std::uint32_t count{};
  std::uint32_t instance_count{};
  std::uint32_t first_index{};
  std::int32_t base_vertex{};
  std::uint32_t base_instance{};
};
static_assert(sizeof(DrawElementsIndirectCommand) == 20);

struct alignas(16) FrustumPlane {
  glm::vec3 normal{};
  float distance{};
};
static_assert(sizeof(FrustumPlane) == 16);

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    if (GLAD_GL_ARB_bindless_texture == 0 || GLAD_GL_ARB_gpu_shader_int64 == 0) {
      throw std::runtime_error{"requires GL_ARB_bindless_texture and GL_ARB_gpu_shader_int64."};
    }

    m_render_shader = std::make_unique<fw::GraphicsShader>(
      fw::ShaderPaths{.vertex = "shaders/hzb_render.vert", .fragment = "shaders/hzb_render.frag"});
    m_wall_shader = std::make_unique<fw::GraphicsShader>(
      fw::ShaderPaths{.vertex = "shaders/hzb_wall.vert", .fragment = "shaders/hzb_wall.frag"});
    m_depth_shader = std::make_unique<fw::GraphicsShader>(
      fw::ShaderPaths{.vertex = "shaders/hzb_only_depth.vert", .fragment = "shaders/hzb_only_depth.frag"});
    m_cull_shader = std::make_unique<fw::ComputeShader>("shaders/hzb_cull.comp");
    m_copy_depth_shader = std::make_unique<fw::ComputeShader>("shaders/hzb_copy_depth.comp");
    m_generate_hiz_shader = std::make_unique<fw::ComputeShader>("shaders/hzb_generate_hiz.comp");

    InitializeDrawables();
    InitMaterials();
    InitInstances();

    m_instances_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_instances)), GL_DYNAMIC_STORAGE_BIT);
    m_instances_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);

    m_materials_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(m_materials)), GL_DYNAMIC_STORAGE_BIT);
    m_materials_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 1);

    const GLsizeiptr visible_buffer_size{static_cast<GLsizeiptr>(sizeof(std::uint32_t) * m_total_instances)};
    m_visible_instances_ssbo = std::make_unique<fw::Buffer>(visible_buffer_size, GL_DYNAMIC_STORAGE_BIT);
    m_visible_instances_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 2);

    const DrawElementsIndirectCommand command{.count = m_cube_index_count};
    m_indirect_buffer = std::make_unique<fw::Buffer>(std::as_bytes(std::span(&command, 1)), GL_DYNAMIC_STORAGE_BIT);
    m_indirect_buffer->BindBase(GL_SHADER_STORAGE_BUFFER, 3);

    m_frustum_ubo = std::make_unique<fw::Buffer>(sizeof(FrustumPlane) * 6, GL_DYNAMIC_STORAGE_BIT);
    m_frustum_ubo->BindBase(GL_UNIFORM_BUFFER, 0);

    glCreateFramebuffers(1, &m_depth_framebuffer);
    CreateDepthResources(m_framebuffer_width, m_framebuffer_height);

    GetCameraController()->SetSceneBounds(m_total_bounds);
    GetCameraController()->ResetCamera(m_total_bounds);

    m_is_initialized = true;
  }

  void OnRender() override {
    if (m_framebuffer_width <= 0 || m_framebuffer_height <= 0) {
      return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glClearDepth(1.0);

    const glm::mat4 view_projection{GetCamera()->GetViewProjectionMatrix()};
    const glm::mat4 wall_model{glm::translate(glm::mat4{1.0F}, m_wall_translation)};

    // 只把明确的不透明墙体作为遮挡体，避免透明几何导致错误剔除。
    if (m_enable_occlusion_culling) {
      RenderOccluderDepth(view_projection, wall_model);
      GenerateHiZPyramid();
    }

    // 先清空间接命令的实例计数，再由每个可见实例原子追加索引。
    ResetIndirectInstanceCount();
    m_cull_shader->Bind();
    const auto frustum_planes{ExtractFrustumPlanes(view_projection)};
    m_frustum_ubo->SetData(0, std::as_bytes(std::span(frustum_planes)));
    m_hiz_texture->BindUnit(0);
    m_cull_shader->SetMat4("u_ViewProjection", view_projection);
    m_cull_shader->SetUInt("u_TotalInstances", static_cast<std::uint32_t>(m_total_instances));
    m_cull_shader->SetInt("u_HiZMaxMip", m_hiz_mip_count - 1);
    m_cull_shader->SetFloat("u_DepthBias", m_depth_bias);
    m_cull_shader->SetInt("u_EnableFrustumCulling", static_cast<int>(m_enable_frustum_culling));
    m_cull_shader->SetInt("u_EnableOcclusionCulling", static_cast<int>(m_enable_occlusion_culling));

    const GLuint group_count{static_cast<GLuint>((m_total_instances + 63) / 64)};
    m_cull_shader->Dispatch(group_count);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    // 线框墙体不会用最终深度测试遮住背后物体，便于直接观察 Hi-Z 剔除结果。
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_framebuffer_width, m_framebuffer_height);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_wall_shader->Bind();
    m_wall_shader->SetMat4("u_ViewProjection", view_projection);
    m_wall_shader->SetMat4("u_Model", wall_model);
    m_wall_shader->SetVec3("u_Color", glm::vec3{1.0F, 1.0F, 0.0F});
    if (m_show_wall_as_wireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    m_wall_drawable->Draw();
    if (m_show_wall_as_wireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    m_render_shader->Bind();
    m_render_shader->SetMat4("u_ViewProjection", view_projection);
    m_cube_drawable->DrawIndirect(m_indirect_buffer->GetHandle(), 0);
  }

  void OnImGui() override {
    ImGui::Begin("Hi-Z Occlusion Culling");
    ImGui::Checkbox("Enable frustum culling", &m_enable_frustum_culling);
    ImGui::Checkbox("Enable Hi-Z Occlusion culling", &m_enable_occlusion_culling);
    ImGui::Checkbox("Wall wireframe display", &m_show_wall_as_wireframe);
    ImGui::SliderFloat("Depth offset", &m_depth_bias, 0.0F, 0.01F, "%.6f");
    ImGui::DragFloat3("Wall location", &m_wall_translation.x, 0.1F);
    if (ImGui::Button("Reset Wall")) {
      m_wall_translation = k_initial_wall_translation;
    }
    ImGui::Separator();
    ImGui::TextUnformatted(std::format("instance count: {}", m_total_instances).c_str());
    ImGui::End();
  }

  void OnFramebufferResized(int width, int height) override {
    fw::Application::OnFramebufferResized(width, height);
    m_framebuffer_width = width;
    m_framebuffer_height = height;

    if (m_is_initialized && width > 0 && height > 0) {
      CreateDepthResources(width, height);
    }
  }

  void OnShutdown() override {
    for (const GpuMaterialData& material : m_materials) {
      if (material.base_color_texture_handle != 0) {
        glMakeTextureHandleNonResidentARB(material.base_color_texture_handle);
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (m_depth_framebuffer != 0) {
      glDeleteFramebuffers(1, &m_depth_framebuffer);
      m_depth_framebuffer = 0;
    }

    m_hiz_texture.reset();
    m_depth_texture.reset();
    m_texture2.reset();
    m_texture1.reset();
    m_indirect_buffer.reset();
    m_visible_instances_ssbo.reset();
    m_materials_ssbo.reset();
    m_instances_ssbo.reset();
    m_frustum_ubo.reset();
    m_wall_drawable.reset();
    m_cube_drawable.reset();
    m_generate_hiz_shader.reset();
    m_copy_depth_shader.reset();
    m_cull_shader.reset();
    m_depth_shader.reset();
    m_wall_shader.reset();
    m_render_shader.reset();

    m_is_initialized = false;
  }

private:
  void InitializeDrawables() {
    const auto cube{fw::Shapes::Cube()};
    m_cube_bounds = cube.bounds;
    m_cube_index_count = static_cast<std::uint32_t>(cube.indices.size());

    m_cube_drawable = std::make_unique<fw::Drawable>(cube.topology);
    m_cube_drawable->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_cube_drawable->AddVertexBuffer(1, std::span(cube.attributes.texcoord_sets[0]), sizeof(glm::vec2));
    m_cube_drawable->SetIndexBuffer(std::span(cube.indices));
    m_cube_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_cube_drawable->GetVertexArray().SetAttribute(1, 1, 2, GL_FLOAT, 0);

    constexpr float wall_half_width{5.5F};
    constexpr float wall_half_height{4.0F};
    const std::array<glm::vec3, 4> wall_positions{{{-wall_half_width, -wall_half_height, 0.0F},
                                                   {wall_half_width, -wall_half_height, 0.0F},
                                                   {wall_half_width, wall_half_height, 0.0F},
                                                   {-wall_half_width, wall_half_height, 0.0F}}};
    const std::array<std::uint32_t, 6> wall_indices{{0, 1, 2, 0, 2, 3}};

    m_wall_drawable = std::make_unique<fw::Drawable>(GL_TRIANGLES);
    m_wall_drawable->AddVertexBuffer(0, std::span(wall_positions), sizeof(glm::vec3));
    m_wall_drawable->SetIndexBuffer(std::span(wall_indices));
    m_wall_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    m_total_bounds.Merge({glm::vec3{-wall_half_width, -wall_half_height, k_initial_wall_translation.z},
                          glm::vec3{wall_half_width, wall_half_height, k_initial_wall_translation.z}});
  }

  void InitMaterials() {
    const auto create_texture = [](const std::filesystem::path& file_path) {
      const fw::Image image{file_path};

      auto texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
      texture->Storage2D(1, image.GetGLInternalFormat(), image.GetWidth(), image.GetHeight());
      texture->SetSubImage2D(0, 0, 0, image.GetWidth(), image.GetHeight(), image.GetGLFormat(), GL_UNSIGNED_BYTE,
                             image.GetPixels());
      texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      texture->SetParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
      texture->SetParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);

      return texture;
    };

    m_texture1 = create_texture(ASSETS_PATH "textures/alpha_blend.png");
    m_texture2 = create_texture(ASSETS_PATH "textures/wood.png");

    m_materials.resize(2);

    const GLuint64 texture_handle1{glGetTextureHandleARB(m_texture1->GetHandle())};
    glMakeTextureHandleResidentARB(texture_handle1);
    m_materials[0].base_color_texture_handle = texture_handle1;
    m_materials[0].base_color_factor = glm::vec4{1.0F};

    const GLuint64 texture_handle2{glGetTextureHandleARB(m_texture2->GetHandle())};
    glMakeTextureHandleResidentARB(texture_handle2);
    m_materials[1].base_color_texture_handle = texture_handle2;
    m_materials[1].base_color_factor = glm::vec4{0.85F, 0.95F, 1.0F, 1.0F};
  }

  void InitInstances() {
    constexpr std::size_t x_count{12};
    constexpr std::size_t y_count{8};
    constexpr std::size_t z_count{4};
    constexpr float horizontal_spacing{1.5F};
    constexpr float depth_spacing{2.0F};
    m_instances.reserve(x_count * y_count * z_count + 3);

    const auto add_instance = [this](const glm::vec3& position, std::uint32_t material_id) {
      const glm::mat4 world_matrix{glm::translate(glm::mat4{1.0F}, position)};
      const fw::AABB instance_bounds{TransformAabb(m_cube_bounds, world_matrix)};

      GpuInstanceData instance{};
      instance.world_matrix = world_matrix;
      instance.bounding_sphere = glm::vec4{instance_bounds.GetCenter(), instance_bounds.GetBoundingRadius()};
      instance.bounding_box_min = glm::vec4{instance_bounds.GetMin(), 0.0F};
      instance.bounding_box_max = glm::vec4{instance_bounds.GetMax(), 0.0F};
      instance.material_id = material_id;

      m_total_bounds.Merge(instance_bounds);
      m_instances.emplace_back(instance);
    };

    for (std::size_t z{}; z < z_count; ++z) {
      for (std::size_t y{}; y < y_count; ++y) {
        for (std::size_t x{}; x < x_count; ++x) {
          const float position_x{(static_cast<float>(x) - static_cast<float>(x_count - 1) * 0.5F) * horizontal_spacing};
          const float position_y{(static_cast<float>(y) - static_cast<float>(y_count - 1) * 0.5F) * horizontal_spacing};
          const float position_z{-2.0F - static_cast<float>(z) * depth_spacing};
          const auto material_id{static_cast<std::uint32_t>((x + y + z) % m_materials.size())};
          add_instance(glm::vec3{position_x, position_y, position_z}, material_id);
        }
      }
    }

    // 墙体前的实例用来验证深度比较不会把前景误剔除。
    add_instance(glm::vec3{-2.0F, 0.0F, 2.0F}, 0);
    add_instance(glm::vec3{0.0F, 0.0F, 2.0F}, 1);
    add_instance(glm::vec3{2.0F, 0.0F, 2.0F}, 0);

    m_total_instances = m_instances.size();
  }

  [[nodiscard]] fw::AABB TransformAabb(const fw::AABB& local_bounds, const glm::mat4& model_matrix) const {
    const glm::vec3 center{local_bounds.GetCenter()};
    const glm::vec3 extent{local_bounds.GetExtent()};

    const glm::vec3 world_center{model_matrix * glm::vec4{center, 1.0F}};
    const glm::vec3 world_extent{std::abs(model_matrix[0][0]) * extent.x + std::abs(model_matrix[1][0]) * extent.y +
                                   std::abs(model_matrix[2][0]) * extent.z,
                                 std::abs(model_matrix[0][1]) * extent.x + std::abs(model_matrix[1][1]) * extent.y +
                                   std::abs(model_matrix[2][1]) * extent.z,
                                 std::abs(model_matrix[0][2]) * extent.x + std::abs(model_matrix[1][2]) * extent.y +
                                   std::abs(model_matrix[2][2]) * extent.z};

    return {world_center - world_extent, world_center + world_extent};
  }

  [[nodiscard]] std::array<FrustumPlane, 6> ExtractFrustumPlanes(const glm::mat4& view_projection) const {
    std::array<FrustumPlane, 6> planes{};

    planes[0] = {{view_projection[0][3] + view_projection[0][0], view_projection[1][3] + view_projection[1][0],
                  view_projection[2][3] + view_projection[2][0]},
                 view_projection[3][3] + view_projection[3][0]};
    planes[1] = {{view_projection[0][3] - view_projection[0][0], view_projection[1][3] - view_projection[1][0],
                  view_projection[2][3] - view_projection[2][0]},
                 view_projection[3][3] - view_projection[3][0]};
    planes[2] = {{view_projection[0][3] + view_projection[0][1], view_projection[1][3] + view_projection[1][1],
                  view_projection[2][3] + view_projection[2][1]},
                 view_projection[3][3] + view_projection[3][1]};
    planes[3] = {{view_projection[0][3] - view_projection[0][1], view_projection[1][3] - view_projection[1][1],
                  view_projection[2][3] - view_projection[2][1]},
                 view_projection[3][3] - view_projection[3][1]};
    planes[4] = {{view_projection[0][3] + view_projection[0][2], view_projection[1][3] + view_projection[1][2],
                  view_projection[2][3] + view_projection[2][2]},
                 view_projection[3][3] + view_projection[3][2]};
    planes[5] = {{view_projection[0][3] - view_projection[0][2], view_projection[1][3] - view_projection[1][2],
                  view_projection[2][3] - view_projection[2][2]},
                 view_projection[3][3] - view_projection[3][2]};

    for (FrustumPlane& plane : planes) {
      const float normal_length{glm::length(plane.normal)};
      plane.normal /= normal_length;
      plane.distance /= normal_length;
    }

    return planes;
  }

  void RenderOccluderDepth(const glm::mat4& view_projection, const glm::mat4& wall_model) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_depth_framebuffer);
    glViewport(0, 0, m_framebuffer_width, m_framebuffer_height);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_depth_shader->Bind();
    m_depth_shader->SetMat4("u_ViewProjection", view_projection);
    m_depth_shader->SetMat4("u_Model", wall_model);
    m_wall_drawable->Draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
  }

  void GenerateHiZPyramid() const {
    m_copy_depth_shader->Bind();
    m_depth_texture->BindUnit(0);
    glBindImageTexture(0, m_hiz_texture->GetHandle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    m_copy_depth_shader->Dispatch(CeilDivide(static_cast<GLuint>(m_framebuffer_width), 16),
                                  CeilDivide(static_cast<GLuint>(m_framebuffer_height), 16));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    m_generate_hiz_shader->Bind();
    m_hiz_texture->BindUnit(0);
    for (int destination_mip{1}; destination_mip < m_hiz_mip_count; ++destination_mip) {
      const int destination_width{std::max(1, m_framebuffer_width >> destination_mip)};
      const int destination_height{std::max(1, m_framebuffer_height >> destination_mip)};

      m_generate_hiz_shader->SetInt("u_SourceMip", destination_mip - 1);
      glBindImageTexture(0, m_hiz_texture->GetHandle(), destination_mip, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
      m_generate_hiz_shader->Dispatch(CeilDivide(static_cast<GLuint>(destination_width), 8),
                                      CeilDivide(static_cast<GLuint>(destination_height), 8));

      // 每一级都依赖前一级，必须在下一次 dispatch 前完成可见性同步。
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }
  }

  void ResetIndirectInstanceCount() const {
    const std::uint32_t zero{};
    glClearNamedBufferSubData(m_indirect_buffer->GetHandle(), GL_R32UI,
                              offsetof(DrawElementsIndirectCommand, instance_count), sizeof(std::uint32_t),
                              GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
  }

  void CreateDepthResources(int width, int height) {
    if (width <= 0 || height <= 0) {
      return;
    }

    const int mip_count{CalculateMipCount(width, height)};

    auto depth_texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
    depth_texture->Storage2D(1, GL_DEPTH_COMPONENT32F, width, height);
    depth_texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    depth_texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    depth_texture->SetParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    depth_texture->SetParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    auto hiz_texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
    hiz_texture->Storage2D(mip_count, GL_R32F, width, height);
    hiz_texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    hiz_texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    hiz_texture->SetParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    hiz_texture->SetParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    hiz_texture->SetParameter(GL_TEXTURE_BASE_LEVEL, 0);
    hiz_texture->SetParameter(GL_TEXTURE_MAX_LEVEL, mip_count - 1);

    glNamedFramebufferTexture(m_depth_framebuffer, GL_DEPTH_ATTACHMENT, depth_texture->GetHandle(), 0);
    glNamedFramebufferDrawBuffer(m_depth_framebuffer, GL_NONE);
    glNamedFramebufferReadBuffer(m_depth_framebuffer, GL_NONE);
    if (glCheckNamedFramebufferStatus(m_depth_framebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      throw std::runtime_error{"Hi-Z depth framebuffer is incomplete."};
    }

    m_depth_texture = std::move(depth_texture);
    m_hiz_texture = std::move(hiz_texture);
    m_hiz_mip_count = mip_count;
  }

  [[nodiscard]] static int CalculateMipCount(int width, int height) noexcept {
    int level_count{1};
    int largest_dimension{std::max(width, height)};
    while (largest_dimension > 1) {
      largest_dimension >>= 1;
      ++level_count;
    }
    return level_count;
  }

  [[nodiscard]] static GLuint CeilDivide(GLuint value, GLuint divisor) noexcept {
    return (value + divisor - 1) / divisor;
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_render_shader{};
  std::unique_ptr<fw::GraphicsShader> m_wall_shader{};
  std::unique_ptr<fw::GraphicsShader> m_depth_shader{};
  std::unique_ptr<fw::ComputeShader> m_cull_shader{};
  std::unique_ptr<fw::ComputeShader> m_copy_depth_shader{};
  std::unique_ptr<fw::ComputeShader> m_generate_hiz_shader{};

  std::unique_ptr<fw::Drawable> m_cube_drawable{};
  std::unique_ptr<fw::Drawable> m_wall_drawable{};
  std::unique_ptr<fw::Texture> m_texture1{};
  std::unique_ptr<fw::Texture> m_texture2{};
  std::unique_ptr<fw::Texture> m_depth_texture{};
  std::unique_ptr<fw::Texture> m_hiz_texture{};

  std::unique_ptr<fw::Buffer> m_instances_ssbo{};
  std::unique_ptr<fw::Buffer> m_materials_ssbo{};
  std::unique_ptr<fw::Buffer> m_visible_instances_ssbo{};
  std::unique_ptr<fw::Buffer> m_indirect_buffer{};
  std::unique_ptr<fw::Buffer> m_frustum_ubo{};

  GLuint m_depth_framebuffer{};
  int m_framebuffer_width{1280};
  int m_framebuffer_height{720};
  int m_hiz_mip_count{1};
  std::uint32_t m_cube_index_count{};
  std::size_t m_total_instances{};
  fw::AABB m_total_bounds{};
  fw::AABB m_cube_bounds{};

  std::vector<GpuInstanceData> m_instances{};
  std::vector<GpuMaterialData> m_materials{};

  inline static constexpr glm::vec3 k_initial_wall_translation{0.0F, 0.0F, 0.5F};
  glm::vec3 m_wall_translation{k_initial_wall_translation};
  float m_depth_bias{0.00001F};
  bool m_enable_frustum_culling{true};
  bool m_enable_occlusion_culling{true};
  bool m_show_wall_as_wireframe{true};
  bool m_is_initialized{false};
};

APPLICATION_MAIN(TestApp)
