#pragma once

// STL
#include <vector>

// Third-party
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace Hex {
    struct Vertex {
        glm::vec3 pos, normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

    class Mesh {
    public:
        Mesh(std::vector<Vertex>&& verts, std::vector<uint32_t>&& idx);
        ~Mesh();

        void Draw() const;

        // Instanced draw: draws 'instanceCount' copies, each using the
        // per-instance mat4 attributes
        void DrawInstanced(GLsizei instanceCount) const;

        GLuint VAO=0, VBO=0, EBO=0;
        GLsizei indexCount=0;
        GLuint instanceVBO = 0;
    private:

    };
}
