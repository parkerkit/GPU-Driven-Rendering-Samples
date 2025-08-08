#version 450 core
// 必须启用 Bindless Texture 扩展
#extension GL_ARB_bindless_texture : require

in vec2 TexCoord;
out vec4 FragColor;

// 直接将 Bindless 句柄声明为 sampler2D 的 uniform
uniform sampler2D u_BindlessTexture;

void main() {
    FragColor = texture(u_BindlessTexture, TexCoord);
}
