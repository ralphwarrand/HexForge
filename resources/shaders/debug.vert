#version 410

layout(location = 0) in vec3 position;  // Vertex position in world space
layout(location = 1) in vec3 color;     // Vertex color
layout(location = 2) in vec3 normal;    // Vertex normal in world space
layout(location = 3) in vec3 tangent;   // (Optional) Tangent vector

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
out vec3 fragColor;

uniform mat4 model;

void main()
{
    // Pass position, normal, and color to the fragment shader
    fragPosition = position;

    mat4 test = mat4(1.f);
    fragNormal = normalize(mat3(transpose(inverse(test))) * normal);
    fragColor = color; // Vertex color

    // Transform the position to clip space
    gl_Position = projection * view * vec4(position, 1.0);
}