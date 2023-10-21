#version 330 core
in vec4 frag_pos;
in vec4 frag_normal;
in vec3 object_color;

out vec4 output;

void main() {
    vec4 norm = normalize(frag_normal);
    // vec4 ambient = vec4(0.0, 0.2, 0.46, 1.0);
    vec4 light_color = vec4(0.7, 0.7, 0.7, 1.0);
    vec4 color = vec4(object_color.xyz, 1.0f);

    vec4 light_location = vec4(5.0, 5.0, -10.0, 0.0);
    vec4 light_direction = normalize(light_location - frag_pos);
    float diff = max(dot(norm.xyz, light_direction.xyz), 0.0);

    vec4 diffuse = diff * light_color;

    output = color + diffuse;
}