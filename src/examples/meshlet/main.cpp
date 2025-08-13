#include <framework.h>
#include <imgui.h>

#include <array>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <span>

namespace {
glm::vec4 GetMeshletColor(uint32_t meshlet_id) {
  uint32_t hash{meshlet_id * 1664525U + 1013904223U};
  hash ^= hash >> 16U;
  hash *= 2246822519U;
  hash ^= hash >> 13U;

  const glm::vec3 color{static_cast<float>(hash & 255U), static_cast<float>((hash >> 8U) & 255U),
                        static_cast<float>((hash >> 16U) & 255U)};
  return glm::vec4{color / 255.0F * 0.7F + 0.3F, 1.0F};
}
}  // namespace

struct DrawElementsIndirectCommand {
  uint32_t count;          // 单个 Instance 的索引数量 (Index Count)
  uint32_t instanceCount;  // 绘制的 Instance 数量 (GPU Culling 算出来的 visibleCount!)
  uint32_t firstIndex;     // 索引 Buffer 中的起始偏移 (Index Offset)
  int32_t baseVertex;      // 顶点 Buffer 中的起始偏移 (Vertex Offset)
  uint32_t baseInstance;   // 传递给 Shader 的 gl_BaseInstance / gl_InstanceID 起始值
};

struct Meshlet {
  uint32_t firstIndex;
  uint32_t indexCount;
  int32_t baseVertex;
  uint32_t padding;

  glm::vec4 boundingSphere;  // xyz=center, w=radius
  glm::vec4 cone;            // xyz=cone axis, w=cutoff
};
static_assert(sizeof(Meshlet) == 48);

struct MeshletBuildResult {
  std::vector<Meshlet> meshlets;
  std::vector<uint32_t> indices;
};

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/meshlet.vert", .fragment = "shaders/meshlet.frag"};
    m_render_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    m_cull_shader = std::make_unique<fw::ComputeShader>("shaders/meshlet.comp");

    auto meshlet_result = InitDrawable();

    std::vector<DrawElementsIndirectCommand> commands(m_meshlet_count);

    // 颜色仅由原始 Meshlet ID 决定，便于重复运行及对照不同绘制路径。
    std::vector<glm::vec4> meshlet_colors(m_meshlet_count);
    for (uint32_t meshlet_id{}; meshlet_id < m_meshlet_count; ++meshlet_id) {
      meshlet_colors[meshlet_id] = GetMeshletColor(meshlet_id);
    }

    m_meshlet_ssbo =
      std::make_unique<fw::Buffer>(std::as_bytes(std::span(meshlet_result.meshlets)), GL_DYNAMIC_STORAGE_BIT);
    m_meshlet_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 0);

    m_indirect_buffer = std::make_unique<fw::Buffer>(std::as_bytes(std::span(commands)), GL_DYNAMIC_STORAGE_BIT);
    m_indirect_buffer->BindBase(GL_SHADER_STORAGE_BUFFER, 1);

    uint32_t zero{};
    m_visible_count_ssbo = std::make_unique<fw::Buffer>(std::as_bytes(std::span(&zero, 1)), GL_DYNAMIC_STORAGE_BIT);
    m_visible_count_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 2);

    m_meshlet_colors_ssbo =
      std::make_unique<fw::Buffer>(std::as_bytes(std::span(meshlet_colors)), GL_DYNAMIC_STORAGE_BIT);
    m_meshlet_colors_ssbo->BindBase(GL_SHADER_STORAGE_BUFFER, 3);

    m_frustums_ubo = std::make_unique<fw::Buffer>(sizeof(glm::vec4) * 6, GL_DYNAMIC_STORAGE_BIT);
    m_frustums_ubo->BindBase(GL_UNIFORM_BUFFER, 0);

    GetCameraController()->SetSceneBounds(m_scene_bounds);
    GetCameraController()->ResetCamera(m_scene_bounds);
  }

  void OnRender() override {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto view_proj = GetCamera()->GetViewProjectionMatrix();
    auto frustum_planes = ExtractFrustumPlanes(view_proj);

    const GLuint zero{};
    glClearNamedBufferSubData(m_visible_count_ssbo->GetHandle(), GL_R32UI, 0, sizeof(GLuint), GL_RED_INTEGER,
                              GL_UNSIGNED_INT, &zero);

    // 因为 meshlet.comp 使用了 compact，所以每帧都需要把 IndirectBuffer 清空
    glClearNamedBufferSubData(m_indirect_buffer->GetHandle(), GL_R32UI, 0,
                              static_cast<GLsizeiptr>(m_meshlet_count * sizeof(DrawElementsIndirectCommand)),
                              GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    m_cull_shader->Bind();

    m_frustums_ubo->SetData(0, std::as_bytes(std::span(frustum_planes)));
    m_cull_shader->SetUInt("u_MeshletCount", static_cast<uint32_t>(m_meshlet_count));
    m_cull_shader->Dispatch(static_cast<GLuint>((m_meshlet_count + 63) / 64));

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    m_render_shader->Bind();
    m_render_shader->SetMat4("u_ViewProjection", view_proj);
    m_drawable->DrawMultiIndirect(m_indirect_buffer->GetHandle(), static_cast<GLsizei>(m_meshlet_count), 0,
                                  sizeof(DrawElementsIndirectCommand));
  }

  void OnShutdown() override {
    m_frustums_ubo.reset();
    m_indirect_buffer.reset();
    m_visible_count_ssbo.reset();
    m_meshlet_ssbo.reset();
    m_meshlet_colors_ssbo.reset();

    m_drawable.reset();

    m_cull_shader.reset();
    m_render_shader.reset();
  }

private:
  MeshletBuildResult InitDrawable() {
    auto shape = fw::Shapes::Sphere();
    m_scene_bounds = shape.bounds;

    auto result = BuildMeshlets(shape.attributes.positions, shape.indices);
    m_meshlet_count = static_cast<uint32_t>(result.meshlets.size());

    m_drawable = std::make_unique<fw::Drawable>(shape.topology);
    m_drawable->AddVertexBuffer(0, std::span(shape.attributes.positions), sizeof(glm::vec3));
    m_drawable->SetIndexBuffer(std::span(result.indices));
    m_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);

    return result;
  }

  // 使用 Gribb-Hartmann 从视图投影矩阵 (VP) 中提取视锥体的 6 个平面参数
  std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& vp) const {
    std::array<glm::vec4, 6> planes{};  // {glm::vec3 normal; float d}

    // Left Right Bottom Top Near Far
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    // 归一化平面法线，使距离计算准确
    for (int i = 0; i < 6; ++i) {
      float length = glm::length(glm::vec3(planes[i]));
      planes[i] /= length;
    }

    return planes;
  }

  MeshletBuildResult BuildMeshlets(const std::vector<glm::vec3>& vertices,
                                   const std::vector<uint32_t>& source_indices) {
    MeshletBuildResult result{};

    if (vertices.empty() || source_indices.empty()) {
      return result;
    }

    assert(source_indices.size() % 3 == 0);

    constexpr size_t max_vertices = 64;
    constexpr size_t max_triangles = 64;
    // 当前只做包围球视锥剔除，不让法线锥权重影响 Meshlet 划分。
    constexpr float cone_weight = 0.0F;

    const size_t max_meshlets = meshopt_buildMeshletsBound(source_indices.size(), max_vertices, max_triangles);

    std::vector<meshopt_Meshlet> opt_meshlets(max_meshlets);

    // local vertex -> 原 mesh vertex index
    std::vector<unsigned int> meshlet_vertices(source_indices.size());

    // 每个 triangle 用 3 个 uint8 local vertex index
    std::vector<unsigned char> meshlet_triangles(source_indices.size());

    const size_t meshlet_count =
      meshopt_buildMeshlets(opt_meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
                            source_indices.data(), source_indices.size(), &vertices[0].x, vertices.size(),
                            sizeof(glm::vec3), max_vertices, max_triangles, cone_weight);

    opt_meshlets.resize(meshlet_count);

    result.meshlets.reserve(meshlet_count);
    result.indices.reserve(source_indices.size());

    // 转换
    for (size_t meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
      meshopt_Meshlet& src = opt_meshlets[meshlet_index];

      // 可选，优化 GPU 顶点缓存
      meshopt_optimizeMeshlet(meshlet_vertices.data() + src.vertex_offset,
                              meshlet_triangles.data() + src.triangle_offset, src.triangle_count, src.vertex_count);

      Meshlet dst{};

      dst.firstIndex = static_cast<uint32_t>(result.indices.size());
      dst.indexCount = static_cast<uint32_t>(src.triangle_count * 3);
      dst.baseVertex = 0;
      dst.padding = 0;

      // meshlet local index -> global index
      for (size_t i = 0; i < src.triangle_count * 3; ++i) {
        const uint8_t local_vertex_index = meshlet_triangles[src.triangle_offset + i];
        const uint32_t global_vertex_index = meshlet_vertices[src.vertex_offset + local_vertex_index];

        result.indices.push_back(global_vertex_index);
      }

      // Bounds
      const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
        meshlet_vertices.data() + src.vertex_offset, meshlet_triangles.data() + src.triangle_offset, src.triangle_count,
        &vertices[0].x, vertices.size(), sizeof(glm::vec3));

      dst.boundingSphere = glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
      dst.cone = glm::vec4(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2], bounds.cone_cutoff);

      result.meshlets.push_back(dst);
    }

    return result;
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_render_shader{};
  std::unique_ptr<fw::ComputeShader> m_cull_shader{};

  std::unique_ptr<fw::Drawable> m_drawable{};
  fw::AABB m_scene_bounds{};
  uint32_t m_meshlet_count;

  std::unique_ptr<fw::Buffer> m_meshlet_ssbo{};
  std::unique_ptr<fw::Buffer> m_visible_count_ssbo{};
  std::unique_ptr<fw::Buffer> m_indirect_buffer{};
  std::unique_ptr<fw::Buffer> m_frustums_ubo{};
  std::unique_ptr<fw::Buffer> m_meshlet_colors_ssbo{};
};

APPLICATION_MAIN(TestApp)
