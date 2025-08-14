#version 460 core

layout(location = 0) out vec4 out_fragment_color;

// Mesh shader 将原始 meshlet ID 写入内建图元 ID，无需自定义输入或 NV 扩展声明。
// 颜色只依赖原始 ID，视锥剔除不会改变剩余 meshlet 的颜色。
vec3 GetMeshletColor(uint meshlet_id)
{
    uint hash = meshlet_id * 1664525u + 1013904223u;
    hash ^= hash >> 16u;
    hash *= 2246822519u;
    hash ^= hash >> 13u;

    vec3 color = vec3(
        float(hash & 255u),
        float((hash >> 8u) & 255u),
        float((hash >> 16u) & 255u)
    ) / 255.0;
    return color * 0.7 + 0.3;
}

void main()
{
    out_fragment_color = vec4(GetMeshletColor(uint(gl_PrimitiveID)), 1.0);
}
