#version 460

layout(location = 0) in vec3 inPosition;

uniform mat4 u_ViewProjection;
flat out uint vMeshletID;

void main()
{
    // gl_DrawID 表示：这一批 multi-draw 的第几个 draw
    // 做了 compact 之后 meshlet_id != gl_DrawID，所以必须使用 comp 生成的 gl_BaseInstance
    // vMeshletID = gl_DrawID;
    vMeshletID = gl_BaseInstance;
    gl_Position = u_ViewProjection * vec4(inPosition, 1.0);
}
