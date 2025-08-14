#include <framework.h>

#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace examples::bindless {
namespace {
class BindlessBufferApp final : public fw::Application {
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

  void OnShutdown() override {
    if (m_vbo) {
      glMakeNamedBufferNonResidentNV(m_vbo->GetHandle());
    }
    glDeleteVertexArrays(1, &m_vao);
    m_vbo.reset();
    m_shader.reset();
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  std::unique_ptr<fw::Buffer> m_vbo{};
  GLuint m_vao{};
  GLuint64EXT m_buffer_gpu_addr{0};
};

}  // namespace
}  // namespace examples::bindless

APPLICATION_MAIN(examples::bindless::BindlessBufferApp)
