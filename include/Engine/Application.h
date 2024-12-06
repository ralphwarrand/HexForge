#pragma once
//STL
#include <string>
#include <cstdint>
#include <memory>

struct GLFWwindow;

namespace Hex
{
	// Forward declaration
	class Console;
	class Renderer;
	class ResourceManager;
	class EntityManager;

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
		explicit Application(const AppSpecification& application_spec = AppSpecification());
		~Application();
		
		Application(const Application&) = delete;
		Application(Application&&) = delete;

		Application& operator = (const Application&) = delete;
		Application& operator = (Application&&) = delete;
		
		void Run() const;
		void Close();
	
	private:
		// Application init functions
		void Init(const AppSpecification& application_spec);
		static void InitTimezone();

		static void InitImgui(GLFWwindow* window);

		static void SetImGuiStyle();

		// Application guts
		std::shared_ptr<Console> m_console{nullptr};
		std::unique_ptr<Renderer> m_renderer{nullptr};
		std::unique_ptr<ResourceManager> m_resource_manager{nullptr};
		std::unique_ptr<EntityManager> m_entity_manager{nullptr};

		AppSpecification m_specification{};
		bool m_running{false};
	};
}