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
    };

    class Mesh {
    public:
        Mesh(std::vector<Vertex>&& verts, std::vector<uint32_t>&& idx);

        void Draw() const;

    private:
        GLuint VAO=0, VBO=0, EBO=0;
        GLsizei indexCount=0;
    };
}
