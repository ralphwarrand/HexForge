#version 420 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 vsRayDir;
out vec3 vsLightDir;

uniform mat4 inverseProjection;
uniform mat4 invView;
uniform vec3 light_dir;

void main() {
    // reconstruct view‐space direction
    vec4 clip = vec4(aPos, 0.0, 1.0);
    vec4 view = inverseProjection * clip;
    view.xyz /= view.w;
    // since clip.w==1, dividing by w gives the correct view‐space position
    vsRayDir = normalize(view.xyz);
    vsLightDir = normalize((invView * vec4(light_dir, 0.0)).xyz);

    TexCoords = aTexCoords;
    gl_Position = clip;
}