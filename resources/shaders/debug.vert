#version 420 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// per‐frame camera + light data in a UBO
layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;
    vec3 light_pos;
    float _pad2;
    int  wireframe;
    float _pad3[3];
};

// per‐draw uniforms
uniform mat4 model;
uniform mat4 light_space_matrix;

// outputs to the fragment shader
out vec3  vWorldPos;
out vec3  vNormal;
out vec2  vTexCoord;
out vec4  vLightSpacePos;

void main() {
    // world position
    vec4 worldPos = model * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;

    // normal (use the inverse‐transpose of the model)
    mat3 normalMat = transpose(inverse(mat3(model)));
    vNormal = normalize(normalMat * aNormal);

    vTexCoord = aTexCoord;

    // for shadow lookup
    vLightSpacePos = light_space_matrix * worldPos ;

    // final position
    gl_Position = projection * view * worldPos;
}
