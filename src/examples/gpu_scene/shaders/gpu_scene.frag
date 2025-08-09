#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

in vec2 FragTexCoord;
flat in uint FragMaterialID;

struct GPUMaterialData {
    vec4 baseColorFactor;
    uint64_t baseColorTexHandle;
    uint64_t normalTexHandle;
    uint64_t roughnessTexHandle;
    uint64_t metallicTexHandle;
};

// 绑定 SSBO 1
layout(std430, binding = 1) readonly buffer MaterialBuffer {
    GPUMaterialData materials[];
};

out vec4 FragColor;

void main() {
    GPUMaterialData mat = materials[FragMaterialID];
    vec4 color = mat.baseColorFactor;

    // 检查 Handle 是否有效 (非零)
    if (mat.baseColorTexHandle != 0UL) {
        // 直接将 64位 Handle 转型为 sampler2D 进行采样！
        sampler2D baseColorSampler = sampler2D(mat.baseColorTexHandle);
        color *= texture(baseColorSampler, FragTexCoord);
    }

    FragColor = color;
}
