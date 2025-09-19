#pragma once

#include <glm/glm.hpp>

// Forward-declare GLFWwindow to avoid including the GLFW header here
struct GLFWwindow;

namespace Hex {

    // Forward-declare Application to avoid circular dependencies
    class Application;

    class InputManager {
    public:
        InputManager();
        ~InputManager();

        static void Init(GLFWwindow* window);
        void Update(); // Can be used for per-frame updates like mouse deltas

        // Public query methods for other systems to use
        bool IsKeyPressed(int key) const;
        bool IsKeyJustPressed(int key) const;
        bool IsMouseButtonJustPressed(int button) const;

        bool IsMouseButtonPressed(int button) const;
        glm::vec2 GetCursorPos() const;

    private:
        // Give Application access to our private callback handlers
        friend class Application;

        // Non-static member functions to handle the logic
        void OnKey(int key, int scancode, int action, int mods);
        void OnMouseButton(int button, int action, int mods);
        void OnCursorPos(double xpos, double ypos);

        // Static C-style functions that GLFW will call directly
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

        // State storage
        bool m_Keys[1024] = {false};
        bool m_LastFrameKeys[1024] = {false};

        bool m_MouseButtons[32] = {false};
        bool m_LastFrameMouseButtons[32] = {false};

        double m_MouseX = 0.0;
        double m_MouseY = 0.0;
    };
}