#pragma once
//STL
#include <string>

namespace Hex
{
	// Forward declaration
	class Renderer;

	// Struct containing parameters to set application settings
	struct AppSpecification 
	{
		std::string name = "Hexahedron App";
		uint16_t width = 1600;
		uint16_t height = 900;
		bool fullscreen = false;
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
		void Init(const AppSpecification& application_spec);
		
		AppSpecification m_specification;
		bool m_running = false;
		Renderer* m_renderer = nullptr;
	};
}