#pragma once
//STL
#include <string>

namespace Hex
{
	class Renderer;

	struct ApplicationSpecification 
	{
		std::string name = "Hexahedron App";
		uint16_t width = 1600;
		uint16_t height = 900;
	};

	class Application
	{
	public:
		Application() = delete;
		explicit Application(const ApplicationSpecification& application_spec = ApplicationSpecification());
		~Application();
		void Close();
	
	private:
		void Init(const ApplicationSpecification& application_spec);
		void Run();
		
		ApplicationSpecification m_specification;
		bool m_running = false;
		Renderer* m_renderer = nullptr;
	};
}
