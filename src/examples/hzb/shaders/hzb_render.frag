#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

struct GpuMaterialData {
    vec4 base_color_factor;
    uint64_t base_color_texture_handle;
    uint64_t normal_texture_handle;
    uint64_t roughness_texture_handle;
    uint64_t metallic_texture_handle;
};

layout(std430, binding = 1) readonly buffer MaterialBuffer {
    GpuMaterialData materials[];
};

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) flat in uint v_MaterialId;
layout(location = 0) out vec4 o_Color;

void main() {
    GpuMaterialData material = materials[v_MaterialId];
    vec4 color = material.base_color_factor;
    if (material.base_color_texture_handle != 0UL) {
        color *= texture(sampler2D(material.base_color_texture_handle), v_TexCoord);
    }
    o_Color = color;
}
