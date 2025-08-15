#version 460 core

uniform vec3 u_Color;
out vec4 fragment_color;

void main() {
    fragment_color = vec4(u_Color, 1.0);
}
