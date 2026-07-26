#version 420

// per-vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// per-instance model matrix (locations 3–6)
layout(location = 3) in mat4 instanceModel;
layout(location = 7) in vec4 aTangent;  // xyz = tangent, w = bitangent sign (+1 or –1)

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

out vec3 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

void main()
{

    mat4 model = mat4(1.f);
    fragColor = vec3(1.f,0.f,1.f);
    fragPosition = vec3(view * model * vec4(aPosition, 1.0));


    gl_Position = projection * view * model * vec4(aPosition, 1.0);
}