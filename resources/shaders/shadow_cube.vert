#version 420

layout(location = 0) in vec3 position;

uniform mat4 shadow_proj;          // Projection matrix
uniform mat4 shadow_view;          // View matrix for the specific cube face

out vec3 frag_pos_world;           // World-space position of the fragment

void main() {
    frag_pos_world = position;
    gl_Position = shadow_proj * shadow_view * vec4(position, 1.0);
}