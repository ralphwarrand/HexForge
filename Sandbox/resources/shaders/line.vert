#version 410

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 dir;

layout(std140) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
};

out vec3 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 model;

void main()
{
    fragColor = color;
    fragPosition = vec3(view * model * vec4(position, 1.0));

    // Stabilize the normal by using a fixed axis
    vec3 up = vec3(0.0, 1.0, 0.0); // or another fixed axis based on line orientation
    fragNormal = normalize(mat3(view) * cross(dir, up)); // Transform normal by view

    fragNormal = normalize(mat3(view) * dir);

    //// Make sure normal faces the camera
    //vec3 viewDir = normalize(viewPosition - fragPosition);
    //if (dot(fragNormal, viewDir) < 0.0) {
    //    fragNormal = -fragNormal;
    //}

    gl_Position = projection * view * model * vec4(position, 1.0);
}