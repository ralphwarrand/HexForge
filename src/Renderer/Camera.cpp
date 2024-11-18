//Hex
#include "Renderer/Camera.h"
#include "Engine/Logger.h"

//Lib
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace Hex
{
	Camera::Camera(const glm::vec3& position, const float& yaw, const float& pitch)
	{
		m_position = position;
		m_yaw = yaw;
		m_pitch = pitch;
		m_aspect_ratio = 16.0f / 9.0f;
		m_projection_matrix = glm::mat4(1.0f);
		UpdateProjectionMatrix();
	
		m_forward = glm::vec3(0.f, 0.f, -1.f);
		m_right = glm::vec3(1.f, 0.f, 0.f);
		m_up = glm::vec3(0.f, 1.f, 0.f);
		m_zoom = 60.f;

		UpdateCameraVectors();
		m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);
	}

	Camera::~Camera()
	= default;

	const glm::mat4& Camera::GetViewMatrix()
	{
		m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);
		return m_view_matrix;
	}

	const glm::mat4& Camera::GetProjectionMatrix()
	{
		UpdateProjectionMatrix();
		return m_projection_matrix;
	}

	void Camera::ProcessKeyboardInput(GLFWwindow* window, const float& delta_time)
	{
		static bool was_tab_pressed = false; // Tracks the state of the TAB key

		if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
		{
			if (!was_tab_pressed && m_mouse_shown_delay <= 0.f) // Only toggle on key press, not hold
			{
				if (m_mouse_shown)
				{
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					m_mouse_shown = false;
				}
				else
				{
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					m_mouse_shown = true;
				}

				m_mouse_shown_delay = 1.f; // Add delay to prevent rapid toggling
			}

			was_tab_pressed = true;
		}
		else
		{
			was_tab_pressed = false; // Reset when TAB is released
		}

		if (m_mouse_shown) return; // Skip all movement processing if mouse is shown

		const float velocity = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ?
			                       m_movement_speed * delta_time * 5.f :
			                       m_movement_speed * delta_time;

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			m_position += m_forward * velocity;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			m_position -= m_forward * velocity;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			m_position -= m_right * velocity;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			m_position += m_right * velocity;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			m_position += m_up * velocity;
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			m_position -= m_up * velocity;
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		UpdateCameraVectors();
	}

	void Camera::ProcessMouseInput(double x_offset, double y_offset, const bool constrain_pitch)
	{
		if (m_mouse_shown) return; // Skip input if mouse is shown

		x_offset *= m_mouse_sensitivity;
		y_offset *= m_mouse_sensitivity;

		m_yaw += static_cast<float>(x_offset);
		m_pitch += static_cast<float>(y_offset);

		if (constrain_pitch)
		{
			if (m_pitch > 89.0f)
				m_pitch = 89.0f;
			if (m_pitch < -89.0f)
				m_pitch = -89.0f;
		}

		UpdateCameraVectors();
	}

	void Camera::ProcessMouseScroll(const float& y_offset)
	{
		m_zoom -= y_offset;
		if (m_zoom < 1.0f)
			m_zoom = 1.0f;
		if (m_zoom > 90.f)
			m_zoom = 90.f;
		UpdateProjectionMatrix();
	}

	void Camera::UpdateProjectionMatrix()
	{
		m_projection_matrix = glm::perspective(glm::radians(m_zoom), m_aspect_ratio, 0.1f, 10000.0f);
	}

	void Camera::UpdateCameraVectors()
	{
		glm::vec3 forward;
		forward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
		forward.y = sin(glm::radians(m_pitch));
		forward.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
		m_forward = glm::normalize(forward);

		m_right = glm::normalize(glm::cross(m_forward, {0.f, 1.f, 0.f}));
		m_up = glm::normalize(glm::cross(m_right, m_forward));
	}

	void Camera::Tick(const float& delta_time)
	{
		if(m_mouse_shown_delay > 0.f) m_mouse_shown_delay -= delta_time;
	}

	void Camera::SetAspectRatio(float aspect_ratio)
	{
		m_aspect_ratio = aspect_ratio;
		UpdateProjectionMatrix();
	}

	glm::vec3 Camera::GetForwardVector() const
	{
		return m_forward;
	}

	glm::vec3 Camera::GetUpVector() const
	{
		return m_up;
	}

	glm::vec3 Camera::GetPosition() const
	{
		return m_position;
	}
}
