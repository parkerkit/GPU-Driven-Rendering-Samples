#include <framework.h>
#include <glad/gl.h>
#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
// 与 mesh_shader.mesh 的输出布局保持一致。
constexpr size_t k_max_meshlet_vertices{64};
constexpr size_t k_max_meshlet_triangles{64};
constexpr size_t k_frustum_plane_count{6};

// std430 布局：四个 uint 后紧接一个 vec4，总步长为 32 字节。
struct alignas(16) GpuMeshlet {
  uint32_t vertex_offset{};
  uint32_t triangle_offset{};
  uint32_t vertex_count{};
  uint32_t triangle_count{};
  glm::vec4 bounding_sphere{};
};

static_assert(sizeof(GpuMeshlet) == 32);
static_assert(offsetof(GpuMeshlet, bounding_sphere) == 16);
static_assert(sizeof(glm::vec4) == 16);

struct MeshletBuildResult {
  std::vector<GpuMeshlet> gpu_meshlets{};
  std::vector<uint32_t> meshlet_vertices{};
  std::vector<uint32_t> packed_triangles{};
};

class MeshShaderApp final : public fw::Application {
protected:
  void OnInit() override {
    if (GLAD_GL_NV_mesh_shader == 0) {
      throw std::runtime_error{"This example requires the OpenGL GL_NV_mesh_shader extension."};
    }

    const fw::MeshShaderPaths shader_paths{.mesh = "shaders/mesh_shader.mesh", .fragment = "shaders/mesh_shader.frag"};
    m_mesh_shader = std::make_unique<fw::MeshShader>(shader_paths);

    m_frustum_planes_ubo =
      std::make_unique<fw::Buffer>(sizeof(glm::vec4) * k_frustum_plane_count, GL_DYNAMIC_STORAGE_BIT);
    m_frustum_planes_ubo->BindBase(GL_UNIFORM_BUFFER, 0);

    InitDrawable();

    GetCameraController()->SetSceneBounds(m_scene_bounds);
    GetCameraController()->ResetCamera(m_scene_bounds);
  }

  void OnRender() override {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 view_projection{GetCamera()->GetViewProjectionMatrix()};
    const auto frustum_planes = ExtractFrustumPlanes(view_projection);

    m_frustum_planes_ubo->SetData(0, std::as_bytes(std::span(frustum_planes)));

    m_mesh_shader->Bind();
    m_mesh_shader->SetMat4("u_view_projection", view_projection);

    glDrawMeshTasksNV(0, m_meshlet_count);
  }

  void OnShutdown() override {
    m_frustum_planes_ubo.reset();
    m_mesh_shader.reset();

    m_vertex_buffer_ssbo.reset();
    m_meshlet_buffer_ssbo.reset();
    m_meshlet_vertex_buffer_ssbo.reset();
    m_meshlet_triangle_buffer_ssbo.reset();
  }

private:
  void InitDrawable() {
    const auto shape = fw::Shapes::Sphere();

    // GPU 端读取 vec4 数组，不能直接上传紧密排列的 vec3 位置。
    std::vector<glm::vec4> vertices(shape.attributes.positions.size());
    std::ranges::transform(shape.attributes.positions, vertices.begin(),
                           [](glm::vec3 position) { return glm::vec4(position, 1.0F); });

    const auto meshlet_result = BuildMeshlets(vertices, shape.indices);

    m_scene_bounds = shape.bounds;
    m_meshlet_count = static_cast<uint32_t>(meshlet_result.gpu_meshlets.size());

    // 网格数据只上传一次，只有视锥平面的 UBO 需要逐帧更新。
    m_vertex_buffer_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(vertices)), 0);
    m_vertex_buffer_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);

    m_meshlet_buffer_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(meshlet_result.gpu_meshlets)), 0);
    m_meshlet_buffer_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 1);

    m_meshlet_vertex_buffer_ssbo =
      std::make_unique<fw::Buffer>(std::as_bytes(std::span(meshlet_result.meshlet_vertices)), 0);
    m_meshlet_vertex_buffer_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 2);

    m_meshlet_triangle_buffer_ssbo =
      std::make_unique<fw::Buffer>(std::as_bytes(std::span(meshlet_result.packed_triangles)), 0);
    m_meshlet_triangle_buffer_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 3);
  }

  // Gribb-Hartmann 提取法，使用 OpenGL 默认的 [-w, w] 裁剪深度范围。
  static std::array<glm::vec4, k_frustum_plane_count> ExtractFrustumPlanes(const glm::mat4& vp) {
    std::array<glm::vec4, k_frustum_plane_count> planes{};

    // 顺序：左、右、下、上、近、远；xyz 为法线，w 为平面常数。
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    // 归一化后才能将有符号距离与包围球半径直接比较。
    for (auto& plane : planes) {
      plane /= glm::length(glm::vec3(plane));
    }

    return planes;
  }

  static MeshletBuildResult BuildMeshlets(std::span<const glm::vec4> source_vertices,
                                          std::span<const uint32_t> source_indices) {
    MeshletBuildResult result{};
    if (source_vertices.empty() || source_indices.empty()) {
      return result;
    }

    assert(source_indices.size() % 3 == 0);

    // 示例只做包围球视锥剔除，不使用法线锥剔除。
    constexpr float k_cone_weight{0.0F};

    const size_t max_meshlets{
      meshopt_buildMeshletsBound(source_indices.size(), k_max_meshlet_vertices, k_max_meshlet_triangles)};

    std::vector<meshopt_Meshlet> opt_meshlets(max_meshlets);

    // meshoptimizer 的输出缓冲区按最坏情况分配；顶点映射上限是索引数量，而非原始顶点数量。
    result.meshlet_vertices.resize(source_indices.size());
    std::vector<unsigned char> meshlet_triangles(source_indices.size());

    const size_t meshlet_count =
      meshopt_buildMeshlets(opt_meshlets.data(), result.meshlet_vertices.data(), meshlet_triangles.data(),
                            source_indices.data(), source_indices.size(), &source_vertices[0].x, source_vertices.size(),
                            sizeof(glm::vec4), k_max_meshlet_vertices, k_max_meshlet_triangles, k_cone_weight);

    opt_meshlets.resize(meshlet_count);

    // 每个三角形的三个局部索引打包到 uint32_t 的低三个字节，供 NV mesh shader 解码。
    result.gpu_meshlets.reserve(meshlet_count);
    result.packed_triangles.reserve(source_indices.size() / 3);

    for (const auto& meshlet : opt_meshlets) {
      meshopt_optimizeMeshlet(result.meshlet_vertices.data() + meshlet.vertex_offset,
                              meshlet_triangles.data() + meshlet.triangle_offset, meshlet.triangle_count,
                              meshlet.vertex_count);

      const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
        result.meshlet_vertices.data() + meshlet.vertex_offset, meshlet_triangles.data() + meshlet.triangle_offset,
        meshlet.triangle_count, &source_vertices[0].x, source_vertices.size(), sizeof(glm::vec4));

      const GpuMeshlet gpu_meshlet{
        .vertex_offset = meshlet.vertex_offset,
        // GPU 偏移以 uint32_t 三角形为单位，不沿用 meshoptimizer 的字节偏移。
        .triangle_offset = static_cast<uint32_t>(result.packed_triangles.size()),
        .vertex_count = meshlet.vertex_count,
        .triangle_count = meshlet.triangle_count,
        .bounding_sphere = glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius),
      };

      for (uint32_t triangle = 0; triangle < meshlet.triangle_count; ++triangle) {
        const size_t offset = meshlet.triangle_offset + static_cast<size_t>(triangle) * 3;

        const uint32_t i0{meshlet_triangles[offset]};
        const uint32_t i1{meshlet_triangles[offset + 1]};
        const uint32_t i2{meshlet_triangles[offset + 2]};

        const uint32_t packed{i0 | (i1 << 8U) | (i2 << 16U)};

        result.packed_triangles.push_back(packed);
      }

      result.gpu_meshlets.push_back(gpu_meshlet);
    }

    if (!opt_meshlets.empty()) {
      const meshopt_Meshlet& last = opt_meshlets.back();
      result.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    }

    return result;
  }

private:
  std::unique_ptr<fw::MeshShader> m_mesh_shader{};

  fw::AABB m_scene_bounds{};
  uint32_t m_meshlet_count{};

  std::unique_ptr<fw::Buffer> m_vertex_buffer_ssbo{};
  std::unique_ptr<fw::Buffer> m_meshlet_buffer_ssbo{};
  std::unique_ptr<fw::Buffer> m_meshlet_vertex_buffer_ssbo{};
  std::unique_ptr<fw::Buffer> m_meshlet_triangle_buffer_ssbo{};
  std::unique_ptr<fw::Buffer> m_frustum_planes_ubo{};
};
}  // namespace

APPLICATION_MAIN(MeshShaderApp)
