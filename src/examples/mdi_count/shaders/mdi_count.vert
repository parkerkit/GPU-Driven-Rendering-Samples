#version 460

layout(location = 0) in vec3 inPosition;

uniform mat4 u_ViewProjection;
flat out uint vMeshletID;

void main()
{
    vMeshletID = gl_BaseInstance;
    gl_Position = u_ViewProjection * vec4(inPosition, 1.0);
}
