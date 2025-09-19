#version 420 core

layout(location = 0) in vec3 position;
// — per‐instance model matrix (locations 3,4,5,6) —
layout(location = 3) in mat4 instanceModel;

// per‐draw uniforms
uniform mat4 light_view;
uniform mat4 light_projection;

void main()
{
    gl_Position = light_projection
    * light_view
    * instanceModel
    * vec4(position, 1.0);
}