#version 330 core
in vec4 frag_pos;
in vec4 frag_normal;
out vec4 output;

void main() {
    vec4 ambient = vec4(0.0, 0.2, 0.46, 1.0);

    vec4 light_location = vec4(0.0, 10.0, 20.0, 0.0);
    vec4 light_color = vec4(0.1, 0.1, 0.2, 1.0);
    vec4 light_distance = frag_pos - light_location;
    float inverted_dot = -dot(frag_normal.xyz, light_distance.xyz);

    output = ambient * light_color * inverted_dot;
}