#pragma once

//Lib
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Hex
{
	class Camera
	{
	public:
		Camera(glm::vec3 position, float yaw, float pitch);
		~Camera();

		Camera(const Camera&) = delete;
		Camera(Camera&&) = delete;

		Camera& operator = (const Camera&) = delete;
		Camera& operator = (Camera&&) = delete;

		[[nodiscard]] glm::mat4 GetViewMatrix() const;
		[[nodiscard]] glm::mat4 GetProjectionMatrix() const;

		void ProcessKeyboardInput(GLFWwindow* window, const float& delta_time);
		void ProcessMouseInput(float x_offset, float y_offset, const bool constrain_pitch = true);
		void ProcessMouseScroll(float yOffset);

		void UpdateProjectionMatrix();
		void UpdateCameraVectors();

		// Setters
		void SetAspectRatio(float aspect_ratio);
	private:
		glm::mat4 m_projection_matrix;
	
		glm::vec3 m_position;
		glm::vec3 m_forward;
		glm::vec3 m_up;
		glm::vec3 m_right;
	
		float m_yaw;
		float m_pitch;
		float m_zoom;
		float m_aspect_ratio;
	
		float m_movement_speed = 20.f;
		float m_mouse_sensitivity = 0.1f;
	};
}
