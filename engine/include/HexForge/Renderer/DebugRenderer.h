#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Hex
{
    class Shader; // Forward declaration

    // A simple vertex structure for our debug primitives
    struct DebugVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    class DebugRenderer
    {
    public:
        // Initializes the renderer's resources (VAO, VBO, Shaders)
        static void Init();

        // Cleans up the resources
        static void Shutdown();

        // Should be called at the start of each frame to clear the old data
        static void BeginFrame();

        // The main render call, executed by the main Renderer class
        static void Flush();

        // --- Primitives API ---

        // Draw a single line segment
        static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

        // Draw an Axis-Aligned Bounding Box (AABB)
        static void DrawAABB(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color);

    private:
        // OpenGL handles
        static unsigned int m_LineVAO;
        static unsigned int m_LineVBO;

        // Shader for drawing
        static std::shared_ptr<Shader> m_Shader;

        // CPU-side buffer for line vertices
        static std::vector<DebugVertex> m_LineVertices;

        // Maximum number of lines we can draw per frame (pre-allocated buffer size)
        static const size_t MAX_LINES = 10000;
    };
}