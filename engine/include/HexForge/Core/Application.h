#pragma once

//STL
#include <string>
#include <cstdint>
#include <memory>
#include <functional>

#include <glm/fwd.hpp>


struct GLFWwindow;

namespace Hex
{
	// Forward declarations
	class InputManager;
	class PhysicsSystem;
	class Console;
	class Renderer;
	class UIManager;
	class EntityManager;

	using SceneBuilder = std::function<void(EntityManager&, PhysicsSystem&)>;

	// Struct containing parameters to set application settings
	struct AppSpecification 
	{
		std::string name = "Hexahedron App";
		uint16_t width = 1600;
		uint16_t height = 900;
		bool fullscreen = false;
		bool vsync = true;
	};

	class Application
	{
	public:
		Application() = delete;
		explicit Application(const AppSpecification& application_spec = AppSpecification(), const SceneBuilder& scene_builder = SceneBuilder());
		~Application();

		std::unique_ptr<InputManager>& GetInputManager();


		Application(const Application&) = delete;
		Application(Application&&) = delete;

		Application& operator = (const Application&) = delete;
		Application& operator = (Application&&) = delete;
		
		void Run();
		void Close();

		// Gamemode
		void SetGameplayMode(bool enabled);
		bool IsInGameplayMode() const;

		void ProcessMousePicking();
		std::pair<glm::vec3, glm::vec3> GetMouseRay();

	
	private:

		// Application init functions
		void Init(const AppSpecification& application_spec);
		// Application guts

		// Renderer declared FIRST, so it is destroyed LAST.
		std::unique_ptr<Renderer> m_renderer{nullptr};

		std::unique_ptr<InputManager> m_input_manager{nullptr};
		std::unique_ptr<EntityManager> m_entity_manager{nullptr};
		std::unique_ptr<PhysicsSystem> m_physics_system{nullptr};

		std::unique_ptr<UIManager> m_ui_manager{nullptr};
		std::shared_ptr<Console> m_console{nullptr};
		SceneBuilder m_sceneBuilder;

		AppSpecification m_specification{};
		bool m_running{false};

		bool m_gameplayMode = false; // Start in UI mode
	};
}
