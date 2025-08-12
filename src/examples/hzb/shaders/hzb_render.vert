#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

struct GpuInstanceData {
    mat4 world_matrix;
    vec4 bounding_sphere;
    vec4 bounding_box_min;
    vec4 bounding_box_max;
    uint material_id;
    uint padding[3];
};

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    GpuInstanceData instances[];
};

layout(std430, binding = 2) readonly buffer VisibleInstanceBuffer {
    uint visible_instance_ids[];
};

uniform mat4 u_ViewProjection;

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) flat out uint v_MaterialId;

void main() {
    uint instance_index = visible_instance_ids[gl_BaseInstance + gl_InstanceID];
    GpuInstanceData instance = instances[instance_index];

    gl_Position = u_ViewProjection * instance.world_matrix * vec4(a_Position, 1.0);
    v_TexCoord = a_TexCoord;
    v_MaterialId = instance.material_id;
}
