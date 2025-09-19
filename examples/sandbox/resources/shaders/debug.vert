#version 420 core

// per-vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// per-instance model matrix (locations 3–6)
layout(location = 3) in mat4 instanceModel;
layout(location = 7) in vec4 aTangent;  // xyz = tangent, w = bitangent sign (+1 or –1)

// per‐frame camera + light data in a UBO
layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;

    vec3 light_dir;     // unit vector pointing *toward* scene
    float _pad2;
    vec3 light_color;   // RGB intensity
    float _pad3;

    int  wireframe;
    float _pad4[3];
};

uniform mat4 light_space_matrix;

// outputs to the fragment shader
out vec3  vWorldPos;
out vec3  vNormal;
out vec2  vTexCoord;
out vec4  vLightSpacePos;
out mat3 vTBN;

void main() {
    // apply per-instance model
    vec4 worldPos = instanceModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;

    // normal‐matrix (inverse-transpose of model)
    mat3 normalMat = mat3(transpose(inverse(instanceModel)));

    // N in world‐space
    vec3 N = normalize(normalMat * aNormal);
    vNormal = N;

    // T in world‐space, then orthonormalize it against N
    vec3 T = normalize(normalMat * aTangent.xyz);
    T = normalize(T - N * dot(N, T));  // Gram–Schmidt

    // B via cross + handedness
    vec3 B = cross(N, T) * aTangent.w;

    vTBN = mat3(T, B, N);

    // UVs and shadow coords
    vTexCoord     = aTexCoord;
    vLightSpacePos = light_space_matrix * worldPos;

    // clip
    gl_Position = projection * view * worldPos;
}
