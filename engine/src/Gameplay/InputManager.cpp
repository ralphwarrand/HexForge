// Hex
#include "HexForge/pch.h"
#include "HexForge/Gameplay/InputManager.h"
#include "HexForge/Core/Application.h"

// Third-party
#include <GLFW/glfw3.h>
#include <algorithm>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


namespace Hex {

    InputManager::InputManager() = default;
    InputManager::~InputManager() = default;

    void InputManager::Init(GLFWwindow* window) {
        // We assume the window's user pointer is already set to the Application instance.
        // These callbacks are now the single source of truth for input.
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, CursorPosCallback);
        // You would also set scroll and char callbacks here if you needed them.
    }

    void InputManager::Update() {
        // Copy the current input states into the "last frame" arrays.
        // This snapshots the state for the next frame's "JustPressed" checks.
        std::copy(std::begin(m_Keys), std::end(m_Keys), std::begin(m_LastFrameKeys));
        std::copy(std::begin(m_MouseButtons), std::end(m_MouseButtons), std::begin(m_LastFrameMouseButtons));
    }

    // --- Getters ---

    bool InputManager::IsKeyPressed(int key) const {
        if (key >= 0 && key < 1024) {
            return m_Keys[key];
        }
        return false;
    }

    bool InputManager::IsKeyJustPressed(int key) const {
        if (key >= 0 && key < 1024) {
            return m_Keys[key] && !m_LastFrameKeys[key];
        }
        return false;
    }

    bool InputManager::IsMouseButtonPressed(int button) const {
        if (button >= 0 && button < 32) {
            return m_MouseButtons[button];
        }
        return false;
    }

    bool InputManager::IsMouseButtonJustPressed(int button) const {
        if (button >= 0 && button < 32) {
            return m_MouseButtons[button] && !m_LastFrameMouseButtons[button];
        }
        return false;
    }

    glm::vec2 InputManager::GetCursorPos() const {
        return { static_cast<float>(m_MouseX), static_cast<float>(m_MouseY) };
    }

    // --- Private Callback Handlers ---

    void InputManager::OnKey(int key, int scancode, int action, int mods) {
        if (key >= 0 && key < 1024) {
            if (action == GLFW_PRESS) m_Keys[key] = true;
            else if (action == GLFW_RELEASE) m_Keys[key] = false;
        }
    }

    void InputManager::OnMouseButton(int button, int action, int mods) {
        if (button >= 0 && button < 32) {
            if (action == GLFW_PRESS) m_MouseButtons[button] = true;
            else if (action == GLFW_RELEASE) m_MouseButtons[button] = false;
        }
    }

    void InputManager::OnCursorPos(double xpos, double ypos) {
        m_MouseX = xpos;
        m_MouseY = ypos;
    }

    // --- Static GLFW Callbacks (with ImGui Forwarding) ---

    void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // 1. Forward the event to ImGui
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

        // 2. Forward to our application's input manager
        Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app && app->GetInputManager()) {
            app->GetInputManager()->OnKey(key, scancode, action, mods);
        }
    }

    void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        // 1. Forward the event to ImGui
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

        // 2. Forward to our application's input manager
        Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app && app->GetInputManager()) {
            app->GetInputManager()->OnMouseButton(button, action, mods);
        }
    }

    void InputManager::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        // 1. Forward the event to ImGui
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

        // 2. Forward to our application's input manager
        Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app && app->GetInputManager()) {
            app->GetInputManager()->OnCursorPos(xpos, ypos);
        }
    }
}