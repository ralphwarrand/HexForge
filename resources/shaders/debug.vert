#version 420

layout(location = 0) in vec3 position;  // Vertex position in world space
layout(location = 1) in vec3 color;     // Vertex color
layout(location = 2) in vec3 normal;    // Vertex normal in world space
layout(location = 3) in vec3 tangent;   // (Optional) Tangent vector

layout(std140, binding = 0) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding1;    // 4 bytes (to align light_pos)
    vec3 light_pos;    // 12 bytes
    float padding2;    // 4 bytes (to align wireframe)
    bool wireframe;    // 4 bytes
    float padding3[3]; // 12 bytes (to align block to 16 bytes)
};

out vec3 fragPosition;
out vec3 fragNormal;
out vec3 fragColor;
out vec4 fragPosLightSpace;     // Position in light-space

uniform mat4 model;
uniform mat4 light_space_matrix;   // Light space matrix for shadow mapping

void main()
{
    // Pass position, normal, and color to the fragment shader
    fragPosition = position;

    mat3 test = mat3(1.f);
    fragNormal = normalize(mat3(transpose(inverse(test))) * normal);
    fragColor = color; // Vertex color

    // Transform the position to clip space
    gl_Position = projection * view * vec4(position, 1.0);

    // Project into light’s clip space for shadow lookups
    fragPosLightSpace = light_space_matrix * vec4(position, 1.0);
}