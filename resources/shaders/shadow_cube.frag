#version 420

in vec3 frag_pos_world;

uniform vec3 light_pos;        // Position of the light in world space
uniform float far_plane;       // Far plane distance of the light's frustum

out float frag_depth;          // Depth value to store in the cube map

void main() {
    float light_distance = length(frag_pos_world - light_pos); // Distance from light to fragment
    frag_depth = light_distance / far_plane;                  // Normalize depth to [0, 1]
}