#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 translation;
uniform mat4 rotation;
uniform vec3 color;

out vec4 frag_pos;
out vec4 frag_normal;
out vec3 object_color;

void main() {
    gl_Position = projection * view * translation * vec4(pos.xyz, 1.0);
    frag_pos = gl_Position;
    frag_normal = translation * vec4(normal.xyz, 1.0);
    object_color = color;
}