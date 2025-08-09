
#include <framework.h>

#include <memory>

class TestApp : public fw::Application {
protected:
  void OnInit() override {
    fw::ShaderPaths shader_paths{.vertex = "shaders/base.vert", .fragment = "shaders/base.frag"};
    m_graphics_shader = std::make_unique<fw::GraphicsShader>(shader_paths);

    auto cube = fw::Shapes::Cube();

    m_drawable = std::make_unique<fw::Drawable>(cube.topology);
    m_drawable->AddVertexBuffer(0, std::span(cube.attributes.positions), sizeof(glm::vec3));
    m_drawable->AddVertexBuffer(1, std::span(cube.attributes.texcoord_sets[0]), sizeof(glm::vec2));
    m_drawable->SetIndexBuffer(std::span(cube.indices));
    m_drawable->GetVertexArray().SetAttribute(0, 0, 3, GL_FLOAT, 0);
    m_drawable->GetVertexArray().SetAttribute(1, 1, 2, GL_FLOAT, 0);

    fw::Image image{ASSETS_PATH "textures/alpha_blend.png"};

    m_texture = std::make_unique<fw::Texture>(GL_TEXTURE_2D);
    m_texture->Storage2D(1, image.GetGLInternalFormat(), image.GetWidth(), image.GetHeight());
    m_texture->SetSubImage2D(0, 0, 0, image.GetWidth(), image.GetHeight(), image.GetGLFormat(), GL_UNSIGNED_BYTE,
                             image.GetPixels());
    m_texture->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_texture->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GetCameraController()->SetSceneBounds(cube.bounds);
    GetCameraController()->ResetCamera(cube.bounds);
  }

  void OnRender() override {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1F, 0.2F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto proj_mat = GetCamera()->GetProjectionMatrix();
    auto view_mat = GetCamera()->GetViewMatrix();

    m_graphics_shader->Bind();
    m_graphics_shader->SetMat4("uView", view_mat);
    m_graphics_shader->SetMat4("uProj", proj_mat);
    m_graphics_shader->SetInt("uTexture", 0);
    m_texture->BindUnit(0);
    m_drawable->Draw();
  }

  void OnShutdown() override {
    m_texture.reset();
    m_drawable.reset();
    m_graphics_shader.reset();
  }

private:
  std::unique_ptr<fw::GraphicsShader> m_graphics_shader{};
  std::unique_ptr<fw::Drawable> m_drawable{};
  std::unique_ptr<fw::Texture> m_texture{};
};

APPLICATION_MAIN(TestApp)
