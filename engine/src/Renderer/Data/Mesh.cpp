// Hex
#include "HexForge/pch.h"
#include "HexForge/Renderer/Data/Mesh.h"

namespace Hex
{
    Mesh::Mesh(std::vector<Vertex> &&verts,
               std::vector<uint32_t> &&idx)
        : indexCount(static_cast<GLsizei>(idx.size()))
    {
        // Regular VAO/VBO/EBO setup
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     verts.size() * sizeof(Vertex),
                     verts.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     idx.size() * sizeof(uint32_t),
                     idx.data(),
                     GL_STATIC_DRAW);

        // vertex attribs: pos(0), normal(1), uv(2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void *) offsetof(Vertex, pos));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void *) offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void *) offsetof(Vertex, uv));

        glEnableVertexAttribArray(7);
        glVertexAttribPointer(
            7,                     // must match layout(location=7) in your VS
            4,                     // vec4
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, tangent)
        );
        glVertexAttribDivisor(7, 0); // per-vertex

        // Per-instance mat4 buffer ---
        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        // reserve some space (you can resize later via BufferData or SubData)
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(glm::mat4) * 100, // e.g. space for 100 instances
                     nullptr,
                     GL_DYNAMIC_DRAW);

        // Each mat4 = 4 vec4s, attrib locations 3,4,5,6
        for (int i = 0; i < 4; ++i)
        {
            GLuint loc = 3 + i;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc,
                                  4, GL_FLOAT, GL_FALSE,
                                  sizeof(glm::mat4),
                                  (void *) (sizeof(glm::vec4) * i));
            // advance this attribute once per instance (not per-vertex)
            glVertexAttribDivisor(loc, 1);
        }
        // unbind instance VBO from ARRAY_BUFFER slot
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);
    }

    Mesh::~Mesh()
    {
        glDeleteBuffers(1, &instanceVBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }

    void Mesh::Draw() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void Mesh::DrawInstanced(GLsizei instanceCount) const
    {
        glBindVertexArray(VAO);
        glDrawElementsInstanced(
            GL_TRIANGLES,
            indexCount,
            GL_UNSIGNED_INT,
            nullptr,
            instanceCount
        );
        glBindVertexArray(0);
    }
} // namespace Hex
