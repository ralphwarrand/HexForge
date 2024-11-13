#version 410

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 dir;

out vec3 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPosition;

void main()
{
    fragColor = color;
    fragPosition = vec3(view * model * vec4(position, 1.0));

    // Calculate normal using dir and viewDir, ensure it points toward the camera
    vec3 viewDir = normalize(viewPosition - fragPosition);
    fragNormal = normalize(cross(dir, viewDir));

    // Ensure normal consistently faces the camera
    if (dot(fragNormal, viewDir) < 0.0) {
        fragNormal = -fragNormal;
    }
    //fragNormal = normalize(cross(dir, vec3(0.0, 1.0, 0.0))); // For testing purposes only

    // Transform position for rendering
    gl_Position = projection * view * model * vec4(position, 1.0);
}