// Hex
#include "HexForge/pch.h"
#include "HexForge/Core/Application.h"
#include "HexForge/Core/Logger.h"
#include "HexForge/Core/Console.h"
#include "HexForge/Core/UIManager.h"
#include "HexForge/Renderer/Renderer.h"
#include "HexForge/Gameplay/EntityManager.h"
#include "HexForge/Gameplay/InputManager.h"
#include "HexForge/Physics/PhysicsSystem.h"

namespace Hex
{
	Application::Application(const AppSpecification& application_spec, const SceneBuilder& scene_builder)
	: m_sceneBuilder(std::move(scene_builder)) {
		Init(application_spec);
	}

	Application::~Application()
	{

	}

	std::unique_ptr<InputManager>& Application::GetInputManager()
	{
		return m_input_manager;
	}

	void Application::Close()
	{
		m_running = false;
	}

	void Application::SetGameplayMode(bool enabled)
	{
		m_gameplayMode = enabled;
		if (m_gameplayMode)
		{
			// ENTER Gameplay Mode
			glfwSetInputMode(m_renderer->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			m_renderer->GetCamera()->ResetMouse(); // Prevents camera jump on mode switch

			m_renderer->RequestViewportFocus();
		}
		else
		{
			// ENTER UI Mode
			glfwSetInputMode(m_renderer->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	bool Application::IsInGameplayMode() const
	{
		return m_gameplayMode;
	}

	void Application::ProcessMousePicking()
	{
		// Get the ray from the current mouse position
		auto [ray_origin, ray_dir] = GetMouseRay();

		// Define a virtual plane to interact with.
		// Here, it's a horizontal plane at y=5. You can change this.
		glm::vec3 plane_origin = {0.0f, 5.0f, 0.0f};
		glm::vec3 plane_normal = {0.0f, 1.0f, 0.0f}; // Y-up

		// Calculate the intersection of the ray with the plane
		// using the formula: t = dot(plane_origin - ray_origin, plane_normal) / dot(ray_dir, plane_normal)
		float denominator = glm::dot(ray_dir, plane_normal);

		// Ensure the ray is not parallel to the plane
		if (glm::abs(denominator) > 1e-6) {
			float t = glm::dot(plane_origin - ray_origin, plane_normal) / denominator;

			// If t is positive, the intersection is in front of the camera
			if (t >= 0) {
				// Calculate the 3D intersection point
				glm::vec3 intersection_point = ray_origin + ray_dir * t;

				// Update the physics system with this 3D position
				m_physics_system->UpdateMousePickerPosition(*m_entity_manager, intersection_point);
			}
		}
	}

	std::pair<glm::vec3, glm::vec3> Application::GetMouseRay()
	{
		// Get mouse position and window dimensions
		glm::vec2 cursorPos = m_input_manager->GetCursorPos();
		float screenX = cursorPos.x;
		float screenY = cursorPos.y;
		int width = m_specification.width;
		int height = m_specification.height;

		// Convert screen coordinates to Normalized Device Coordinates (NDC)
		// x and y are now in the range [-1, 1]. z=-1 is the near plane.
		float ndcX = (2.0f * screenX) / width - 1.0f;
		float ndcY = 1.0f - (2.0f * screenY) / height; // Y is inverted
		float ndcZ = -1.0f;
		glm::vec4 ray_ndc_start = glm::vec4(ndcX, ndcY, ndcZ, 1.0f);

		// Get the inverse projection and view matrices
		glm::mat4 proj = m_renderer->GetCamera()->GetProjectionMatrix();
		glm::mat4 view = m_renderer->GetCamera()->GetViewMatrix();
		glm::mat4 invProj = glm::inverse(proj);
		glm::mat4 invView = glm::inverse(view);

		// Unproject the NDC point to get a point on the near plane in world space
		glm::vec4 ray_world_start_h = invView * invProj * ray_ndc_start;
		ray_world_start_h /= ray_world_start_h.w; // Perspective divide
		glm::vec3 ray_world_start = glm::vec3(ray_world_start_h);

		// The ray's origin is the camera's position
		glm::vec3 ray_origin = m_renderer->GetCamera()->GetPosition(); // Assumes you have a GetPosition() method

		// The ray's direction is from the camera towards the point on the near plane
		glm::vec3 ray_dir = glm::normalize(ray_world_start - ray_origin);

		return {ray_origin, ray_dir};
	}

	void Application::Init(const AppSpecification& application_spec)
	{
		m_specification = application_spec;

		// 1. Console is created
		m_console = std::make_shared<Console>();

		// 2. World Systems are created
		m_entity_manager = std::make_unique<EntityManager>();
		m_physics_system = std::make_unique<PhysicsSystem>();

		// 3. Input Manager is created
		m_input_manager = std::make_unique<InputManager>();

		// 4. Renderer creates the GLFW window
		m_renderer = std::make_unique<Renderer>(m_entity_manager->GetRegistry() ,application_spec, m_console);


		// 5. This tells GLFW to associate our 'Application' instance ('this') with the window.
		glfwSetWindowUserPointer(m_renderer->GetWindow(), this);

		// 6. InputManager sets the MASTER callbacks for the window
		Hex::InputManager::Init(m_renderer->GetWindow());

		// 7. UIManager initializes ImGui, which will now use the forwarded events
		m_ui_manager = std::make_unique<UIManager>(m_renderer->GetWindow(), m_console, *m_physics_system, *m_renderer);

		// 8. We build the scene
		m_sceneBuilder(*m_entity_manager, *m_physics_system);

		m_running = true;
	}

	void Application::Run()
	{
		float delta_time = 0.0f;
		float last_frame = 0.0f;

		while (m_running && !glfwWindowShouldClose(m_renderer->GetWindow()))
		{
			// Poll for any new window/input events FIRST
			glfwPollEvents();

			// Calculate delta time
			const float current_frame = static_cast<float>(glfwGetTime());
			delta_time = current_frame - last_frame;
			last_frame = current_frame;

			m_ui_manager->BeginFrame();
			m_ui_manager->RenderUI(delta_time);

			ImGuiIO& io = ImGui::GetIO();

			if (!io.WantCaptureKeyboard)
			{
				if (m_input_manager->IsKeyJustPressed(GLFW_KEY_TAB))
				{
					SetGameplayMode(!m_gameplayMode); // Toggle the mode
				}

				if (m_input_manager->IsKeyJustPressed(GLFW_KEY_ESCAPE))
				{
					m_running = false;
				}
			}

			if (m_gameplayMode)
			{
				m_renderer->GetCamera()->Update(delta_time, *m_input_manager);
			}


			m_physics_system->Tick(*m_entity_manager, delta_time, current_frame);

			// Render 3D world to framebuffer
			m_renderer->RenderWorld(delta_time);

			// Render UI

			m_ui_manager->EndFrame();

			glfwSwapBuffers(m_renderer->GetWindow());

			// This snapshots the input state for the NEXT frame.
			m_input_manager->Update();
		}
	}
}
