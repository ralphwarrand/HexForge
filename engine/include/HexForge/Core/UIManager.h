#pragma once

#include "HexForge/Core/Console.h"
#include "HexForge/Physics/PhysicsSystem.h"

// Forward declaration
struct GLFWwindow;

namespace Hex
{


    class UIManager
    {
    public:
        UIManager(GLFWwindow* window, const std::shared_ptr<Console>& console,
                  PhysicsSystem& physicsSystem, Renderer& renderer);
        ~UIManager();

        void BeginFrame();
        void EndFrame();
        void RenderUI(float deltaTime);

    private:
        void SetStyle();

        // UI Panel Methods
        void ShowMenuBar();
        void ShowPhysicsControls();
        void ShowMetrics(float deltaTime);
        void ShowSceneInfo();
        void ShowLightingTool();
        void ShowViewport();

        // Member variables
        GLFWwindow* m_window;
        PhysicsSystem& m_physicsSystem;
        Renderer& m_renderer;
        std::shared_ptr<Console> m_console;

        // UI State
        bool m_showMetrics = true;
        bool m_showSceneInfo = true;
        bool m_showLightingTool = true;
        bool m_showPhysicsControls = true;


        ImVec2 m_viewportSize = { 0, 0 };

        glm::vec3 m_light_dir = glm::vec3(0.5f, -1.f, 0.5f);
    };
}

