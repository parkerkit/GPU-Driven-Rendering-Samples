#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

struct GPUInstanceData {
    mat4 worldMatrix;
    vec4 boundingSphere;
    uint meshID;
    uint materialID;
    uint padding[2];
};

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    GPUInstanceData instances[];
};

layout(std430, binding = 2) readonly buffer VisibleInstanceBuffer {
    uint visibleInstanceIDs[];
};

uniform mat4 u_ViewProjection;
out vec2 FragTexCoord;
flat out uint FragMaterialID;

void main() {
    // 通过 gl_InstanceID 从压缩列表中取得真正的 Instance 索引
    uint realInstanceID = visibleInstanceIDs[gl_InstanceID];

    // 获取真正的物体属性
    GPUInstanceData instance = instances[realInstanceID];

    vec4 worldPos = instance.worldMatrix * vec4(aPos, 1.0);
    gl_Position = u_ViewProjection * worldPos;

    FragTexCoord = aTexCoord;
    FragMaterialID = instance.materialID;
}
