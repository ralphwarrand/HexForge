#include "HexForge/pch.h"
#include "HexForge/Renderer/DebugRenderer.h"
#include "HexForge/Renderer/Shader.h"
#include "HexForge/Renderer/ShaderManager.h" // Assuming you have this

namespace Hex
{
    // Initialize static members
    unsigned int DebugRenderer::m_LineVAO = 0;
    unsigned int DebugRenderer::m_LineVBO = 0;
    std::shared_ptr<Shader> DebugRenderer::m_Shader = nullptr;
    std::vector<DebugVertex> DebugRenderer::m_LineVertices;

    void DebugRenderer::Init()
    {
        // Create the shader for debug drawing
        m_Shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/line.vert",
            RESOURCES_PATH "shaders/line.frag"
        );

        // Reserve space in our CPU-side vector
        m_LineVertices.reserve(MAX_LINES * 2);

        // --- Set up Line Rendering ---
        glGenVertexArrays(1, &m_LineVAO);
        glBindVertexArray(m_LineVAO);

        glGenBuffers(1, &m_LineVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
        // Allocate a large buffer upfront with GL_DYNAMIC_DRAW hint
        glBufferData(GL_ARRAY_BUFFER, MAX_LINES * 2 * sizeof(DebugVertex), nullptr, GL_DYNAMIC_DRAW);

        // Vertex position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, position));

        // Vertex color attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, color));

        glBindVertexArray(0);
    }

    void DebugRenderer::Shutdown()
    {
        glDeleteBuffers(1, &m_LineVBO);
        glDeleteVertexArrays(1, &m_LineVAO);
    }

    void DebugRenderer::BeginFrame()
    {
        m_LineVertices.clear();
    }

    void DebugRenderer::Flush()
    {
        if (m_LineVertices.empty())
        {
            return;
        }

        m_Shader->Bind();

        // Bind the VAO for lines
        glBindVertexArray(m_LineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
        // Stream the new vertex data to the pre-allocated VBO
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_LineVertices.size() * sizeof(DebugVertex), m_LineVertices.data());

        // Set line width (optional)
        glLineWidth(2.0f);

        // Draw the lines
        glDrawArrays(GL_LINES, 0, m_LineVertices.size());

        glBindVertexArray(0);
        Shader::Unbind();
    }

    void DebugRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
    {
        if (m_LineVertices.size() >= MAX_LINES * 2) return;
        m_LineVertices.push_back({ start, color });
        m_LineVertices.push_back({ end, color });
    }

    void DebugRenderer::DrawAABB(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color)
    {
        glm::vec3 min = center - size / 2.0f;
        glm::vec3 max = center + size / 2.0f;

        // The 8 corners of the box
        glm::vec3 corners[8] = {
            {min.x, min.y, min.z}, {max.x, min.y, min.z},
            {max.x, max.y, min.z}, {min.x, max.y, min.z},
            {min.x, min.y, max.z}, {max.x, min.y, max.z},
            {max.x, max.y, max.z}, {min.x, max.y, max.z}
        };

        // Draw the 12 lines of the box
        DrawLine(corners[0], corners[1], color); DrawLine(corners[1], corners[2], color);
        DrawLine(corners[2], corners[3], color); DrawLine(corners[3], corners[0], color);
        DrawLine(corners[4], corners[5], color); DrawLine(corners[5], corners[6], color);
        DrawLine(corners[6], corners[7], color); DrawLine(corners[7], corners[4], color);
        DrawLine(corners[0], corners[4], color); DrawLine(corners[1], corners[5], color);
        DrawLine(corners[2], corners[6], color); DrawLine(corners[3], corners[7], color);
    }
}