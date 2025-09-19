#pragma once

// Third-party
#include <glm/glm.hpp>

namespace Hex
{
	class InputManager;

	class Camera
	{
	public:
		explicit Camera(const glm::vec3& position = {0.0f, -3.0f, 3.0f}, float yaw = -90.0f, float pitch = 0.0f);
		~Camera();

		Camera(const Camera&) = delete;
		Camera(Camera&&) = delete;

		Camera& operator = (const Camera&) = delete;
		Camera& operator = (Camera&&) = delete;

		// Getters
		[[nodiscard]] glm::mat4 GetViewMatrix();
		[[nodiscard]] glm::mat4 GetProjectionMatrix();
		[[nodiscard]] glm::vec3 GetPosition() const { return m_position; }
		[[nodiscard]] glm::vec3 GetFront() const { return m_forward; }

		// Main update function that now handles all input
		void Update(float deltaTime, const InputManager& inputManager);

		// Setters
		void SetAspectRatio(float aspect_ratio);

		void ResetMouse();


	private:
		void UpdateCameraVectors();
		void UpdateProjectionMatrix();

		// Matrices
		glm::mat4 m_view_matrix{};
		glm::mat4 m_projection_matrix{};

		// Camera vectors
		glm::vec3 m_position{};
		glm::vec3 m_forward{};
		glm::vec3 m_up{};
		glm::vec3 m_right{};
		glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

		// Euler Angles & Zoom
		float m_yaw{};
		float m_pitch{};
		float m_zoom{};
		float m_aspect_ratio{};

		// Camera settings
		float m_movement_speed{5.f};
		float m_mouse_sensitivity{0.1f};

		// Mouse look state
		bool m_firstMouse{true};
		float m_lastMouseX{0.0f};
		float m_lastMouseY{0.0f};
	};
}

