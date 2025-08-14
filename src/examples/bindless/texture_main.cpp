#include <framework.h>

#include <cstddef>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace examples::bindless {
namespace {
class BindlessTextureApp final : public fw::Application {
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

  void OnShutdown() override {
    if (m_texture_handle != 0) {
      glMakeTextureHandleNonResidentARB(m_texture_handle);
      m_texture_handle = 0;
    }
    m_texture.reset();
    m_drawable.reset();
    m_shader.reset();
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_shader{};
  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture{};
  GLuint64 m_texture_handle{};
};

}  // namespace
}  // namespace examples::bindless

APPLICATION_MAIN(examples::bindless::BindlessTextureApp)
