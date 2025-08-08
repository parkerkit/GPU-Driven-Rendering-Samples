#version 450 core
#extension GL_NV_shader_buffer_load : enable // 开启 Bindless Buffer/Pointer 支持

// 定义顶点结构体
struct Vertex {
    float x, y, z;
    float r, g, b;
};

// 声明一个 buffer 类型的指针（存储 64 位 GPU 显存地址）
uniform Vertex* u_VertexBufferAddr;

out vec3 vColor;

void main() {
    // 直接通过 GPU 显存地址指针像操作 C 语言数组一样读取顶点
    Vertex v = u_VertexBufferAddr[gl_VertexID];

    gl_Position = vec4(v.x, v.y, v.z, 1.0);
    vColor = vec3(v.r, v.g, v.b);
}
