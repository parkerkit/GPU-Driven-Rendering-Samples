#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

// 定义 SSBO 结构体
struct GPUInstanceData {
    mat4 worldMatrix;
    vec4 boundingSphere;
    uint meshID;
    uint materialID;
    uint padding[2];
};

// 绑定 SSBO 0
layout(std430, binding = 0) readonly buffer InstanceBuffer {
    GPUInstanceData instances[];
};

uniform mat4 u_ViewProjection;

out vec2 FragTexCoord;
flat out uint FragMaterialID; // 传递 MaterialID 给 Pixel Shader

void main() {
    // 直接读取内置变量 gl_InstanceID
    GPUInstanceData instance = instances[gl_InstanceID];

    vec4 worldPos = instance.worldMatrix * vec4(aPos, 1.0);
    gl_Position = u_ViewProjection * worldPos;

    FragTexCoord = aTexCoord;
    FragMaterialID = instance.materialID;
}
