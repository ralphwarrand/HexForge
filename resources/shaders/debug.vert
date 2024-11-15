#version 410

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(std140) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding;     // 4 bytes
    bool wireframe;    // 4 bytes
    float padding2[3]; // 12 bytes (padding to align to 16 bytes)
};

out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 model;


void main()
{
    // Calculate world position of the vertex
    fragPosition = vec3(model * vec4(position, 1.0));

    // Calculate the normal in world space
    fragNormal = mat3(transpose(inverse(model))) * normal;

    // Calculate final position of the vertex in screen space
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}