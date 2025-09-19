//Hex
#include "pch.h"
#include "HexForge/Renderer/Camera.h"
#include "HexForge/Gameplay/InputManager.h"

//Lib
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Hex
{
    Camera::Camera(const glm::vec3 &position, float yaw, float pitch)
    {
       m_position = position;
       m_yaw = yaw;
       m_pitch = pitch;
       m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
       m_aspect_ratio = 16.0f / 9.0f;
       m_zoom = 60.f;


       m_firstMouse = true;
       m_lastMouseX = 0.0f;
       m_lastMouseY = 0.0f;

       UpdateCameraVectors();
       UpdateProjectionMatrix();
    }

    Camera::~Camera() = default;

    glm::mat4 Camera::GetViewMatrix()
    {
       // Recalculate the view matrix each time it's requested
       m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);
       return m_view_matrix;
    }

    glm::mat4 Camera::GetProjectionMatrix()
    {
       return m_projection_matrix;
    }

    void Camera::Update(float deltaTime, const InputManager& inputManager)
    {
       // Get the ImGui IO state
       ImGuiIO& io = ImGui::GetIO();

       // --- Mouse Input for Look ---
       // ONLY process mouse look if ImGui does NOT want the mouse
       if (!io.WantCaptureMouse)
       {
          glm::vec2 cursorPos = inputManager.GetCursorPos();

          if (m_firstMouse)
          {
             m_lastMouseX = cursorPos.x;
             m_lastMouseY = cursorPos.y;
             m_firstMouse = false;
          }

          float x_offset = cursorPos.x - m_lastMouseX;
          float y_offset = m_lastMouseY - cursorPos.y;

          m_lastMouseX = cursorPos.x;
          m_lastMouseY = cursorPos.y;

          x_offset *= m_mouse_sensitivity;
          y_offset *= m_mouse_sensitivity;

          m_yaw   += x_offset;
          m_pitch += y_offset;

          // Constrain pitch
          if (m_pitch > 89.0f) m_pitch = 89.0f;
          if (m_pitch < -89.0f) m_pitch = -89.0f;
       }


       // --- Keyboard Input for Movement ---
       // ONLY process keyboard movement if ImGui does NOT want the keyboard
       if (!io.WantCaptureKeyboard)
       {
          const float velocity = (inputManager.IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) ?
                                  m_movement_speed * deltaTime * 5.f :
                                  m_movement_speed * deltaTime;

          if (inputManager.IsKeyPressed(GLFW_KEY_W))
             m_position += m_forward * velocity;
          if (inputManager.IsKeyPressed(GLFW_KEY_S))
             m_position -= m_forward * velocity;
          if (inputManager.IsKeyPressed(GLFW_KEY_A))
             m_position -= m_right * velocity;
          if (inputManager.IsKeyPressed(GLFW_KEY_D))
             m_position += m_right * velocity;
          if (inputManager.IsKeyPressed(GLFW_KEY_SPACE))
             m_position += m_worldUp * velocity;
          if (inputManager.IsKeyPressed(GLFW_KEY_LEFT_CONTROL))
             m_position -= m_worldUp * velocity;
       }


       // --- Update internal vectors from new position and orientation ---
       UpdateCameraVectors();
    }


    void Camera::SetAspectRatio(float aspect_ratio)
    {
       m_aspect_ratio = aspect_ratio;
       UpdateProjectionMatrix();
    }

    void Camera::ResetMouse()
    {
       m_firstMouse = true;
    }

    void Camera::UpdateProjectionMatrix()
    {
       m_projection_matrix = glm::perspective(glm::radians(m_zoom), m_aspect_ratio, 0.1f, 1000.0f);
    }

    void Camera::UpdateCameraVectors()
    {
       glm::vec3 front;
       front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
       front.y = sin(glm::radians(m_pitch));
       front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
       m_forward = glm::normalize(front);

       // Also re-calculate the Right and Up vector
       m_right = glm::normalize(glm::cross(m_forward, m_worldUp));
       m_up = glm::normalize(glm::cross(m_right, m_forward));
    }
}
