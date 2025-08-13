#version 460 core

layout(std430, binding = 3) readonly buffer MeshletColors
{
    vec4 colors[];
};

flat in uint vMeshletID;
out vec4 FragColor;

void main()
{
    FragColor = colors[vMeshletID];
}
