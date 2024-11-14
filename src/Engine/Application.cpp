//Hex
#include "Engine/Application.h"
#include "Engine/Logger.h"
#include "Renderer/Renderer.h"
#include "Renderer/Camera.h"

//Lib
#include <GLFW/glfw3.h>

//STL
#include <cstdlib>

namespace Hex
{
	Application::Application(const AppSpecification& application_spec)
	{
		Init(application_spec);
	}

	Application::~Application()
	{
		m_renderer->~Renderer();
	}

	void Application::Close()
	{
		m_running = false;
	}

	void Application::Init(const AppSpecification& application_spec)
	{
		#ifdef WIN32
			date::set_install(RESOURCES_PATH "tzdata");
		#endif

		try {
			auto tz = date::current_zone();
			std::cout << "Timezone initialized: " << tz->name() << '\n';
		} catch (const std::exception &e) {
			std::cerr << "Error initializing timezone: " << e.what() << '\n';
		}

		m_renderer = new Renderer(application_spec);
		m_specification = application_spec;
		m_running = true;
	}

	void Application::Run() const
	{
		float delta_time = 0.0f;
		float last_frame = 0.0f;

		m_renderer->GetCamera()->SetAspectRatio(static_cast<float>(m_specification.width) / m_specification.height);
		
		while (m_running && !glfwWindowShouldClose(m_renderer->GetWindow()))
		{
			const float current_frame = static_cast<float>(glfwGetTime());
			delta_time = current_frame - last_frame;
			last_frame = current_frame;
			
			m_renderer->GetCamera()->ProcessKeyboardInput(m_renderer->GetWindow(), delta_time);
			m_renderer->Tick();

			glfwPollEvents();
		}
	}
}