//Hex
#include "Engine/Application.h"
#include "Engine/Logger.h"
#include "Renderer/Renderer.h"
#include "Renderer/Camera.h"

//Lib
#include <GLFW/glfw3.h>

#define IMGUI_ENABLE_DOCKING
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

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
		// Cleanup
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void Application::Close()
	{
		m_running = false;
	}

	void Application::Init(const AppSpecification& application_spec)
	{
		InitTimezone();

		m_renderer = std::make_unique<Renderer>(application_spec);
		InitImgui(m_renderer->GetWindow());


		m_specification = application_spec;
		m_running = true;
	}

	void Application::InitTimezone() {

#ifdef WIN32
		date::set_install(RESOURCES_PATH "tzdata");
#endif

		try
		{
			const auto tz = date::current_zone();
			std::cout << "Timezone initialized: " << tz->name() << '\n';
		}
		catch (const std::exception &e)
		{
			std::cerr << "Error initializing timezone: " << e.what() << '\n';
		}
	}

	void Application::InitImgui(GLFWwindow *window)
	{
		// Create ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

		// Set up ImGui style
		ImGui::StyleColorsDark();

		// Initialize ImGui for GLFW and OpenGL
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410");


		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		io.IniFilename = "imgui_layout.ini";
	}

	void Application::Run() const
	{
		float delta_time = 0.0f;
		float last_frame = 0.0f;

		//TODO: Move to renderer, maybe make m_specification a shared_ptr
		m_renderer->GetCamera()->SetAspectRatio(static_cast<float>(m_specification.width) / m_specification.height);
		
		while (m_running && !glfwWindowShouldClose(m_renderer->GetWindow()))
		{
			const float current_frame = static_cast<float>(glfwGetTime());
			delta_time = current_frame - last_frame;
			last_frame = current_frame;

			m_renderer->Tick(delta_time);

			glfwPollEvents();
		}
	}
}