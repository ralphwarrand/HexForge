#pragma once

//Lib
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Hex
{
	class Camera
	{
	public:
		Camera(const glm::vec3& position, const float& yaw, const float& pitch);
		~Camera();

		Camera(const Camera&) = delete;
		Camera(Camera&&) = delete;

		Camera& operator = (const Camera&) = delete;
		Camera& operator = (Camera&&) = delete;

		[[nodiscard]] const glm::mat4& GetViewMatrix();
		[[nodiscard]] const glm::mat4& GetProjectionMatrix();

		void ProcessKeyboardInput(GLFWwindow* window, const float& delta_time);
		void ProcessMouseInput(double x_offset, double y_offset, const bool constrain_pitch = true);
		void ProcessMouseScroll(const float& y_offset);

		void UpdateProjectionMatrix();
		void UpdateCameraVectors();
		void Tick(const float& delta_time);

		// Setters
		void SetAspectRatio(float aspect_ratio);

		//Getters
		[[nodiscard]] glm::vec3 GetForwardVector() const;
		[[nodiscard]] glm::vec3 GetUpVector() const;
		[[nodiscard]] glm::vec3 GetPosition() const;
	private:
		glm::mat4 m_view_matrix{};
		glm::mat4 m_projection_matrix{};
		
		glm::vec3 m_position{};
		glm::vec3 m_forward{};
		glm::vec3 m_up{};
		glm::vec3 m_right{};
	
		float m_yaw{};
		float m_pitch{};
		float m_zoom{};
		float m_aspect_ratio{};
	
		float m_movement_speed{20.f};
		float m_mouse_sensitivity{0.1f};

		bool m_mouse_shown{true};
		float m_mouse_shown_delay{};
	};
}
