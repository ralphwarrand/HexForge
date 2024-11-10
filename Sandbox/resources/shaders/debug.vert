#version 410

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Calculate world position of the vertex
    fragPosition = vec3(model * vec4(position, 1.0));

    // Calculate the normal in world space
    fragNormal = mat3(transpose(inverse(model))) * normal;

    // Calculate final position of the vertex in screen space
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}