//Hex::Engine
#include "Engine/Application.h"
#include "Engine/Logger.h"
#include "Engine/ResourceManagement/ResourceManager.h"
#include "Engine/ResourceManagement/TextureResource.h"
#include "Engine/Console.h"
//Hex::Renderer
#include "Renderer/Renderer.h"
//Hex::EntityManager
#include "Gameplay/EntityManager.h"
#include "Gameplay/EntityComponents.h"

//Lib
#include <GLFW/glfw3.h>

#define IMGUI_ENABLE_DOCKING
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

//STL
#include <cstdlib>
#include <Engine/Application.h>
#include <Engine/Application.h>

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

		m_console = std::make_shared<Console>();
		m_resource_manager = std::make_unique<ResourceManager>();
		m_renderer = std::make_unique<Renderer>(application_spec, m_console);
		m_entity_manager = std::make_unique<EntityManager>();

		InitImgui(m_renderer->GetWindow());

		m_specification = application_spec;
		m_running = true;

		// Load a texture
		{
			auto texture = m_resource_manager->loadResource<Hex::TextureResource>(RESOURCES_PATH "textures/debug/test.bmp");

			if (texture)
			{
				std::cout << "Texture loaded successfully!" << std::endl;
				std::cout << "Width: " << texture->width << ", Height: " << texture->height << std::endl;
			}
			else
			{
				std::cerr << "Failed to load texture." << std::endl;
			}
		}

		auto entity1 = m_entity_manager->CreateEntity("Player");
		m_entity_manager->AddComponent<Position>(entity1, 10.0f, 20.0f, 10.f);
		m_entity_manager->AddComponent<Velocity>(entity1, 5.0f, -3.0f, 1.f);



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
		SetImGuiStyle();

		// Initialize ImGui for GLFW and OpenGL
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 420");


		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		io.IniFilename = "imgui_layout.ini";
	}

	void Application::SetImGuiStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
   		ImVec4* colors = style.Colors;

   		// Base Colors
   		colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
   		colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
   		colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
   		colors[ImGuiCol_ChildBg]               = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
   		colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
   		colors[ImGuiCol_Border]                = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
   		colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   		colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
   		colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
   		colors[ImGuiCol_FrameBgActive]         = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
   		colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
   		colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
   		colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
   		colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
   		colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
   		colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
   		colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
   		colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
   		colors[ImGuiCol_CheckMark]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
   		colors[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
   		colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
   		colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
   		colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
   		colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
   		colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
   		colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
   		colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
   		colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
   		colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
   		colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
   		colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
   		colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
   		colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
   		colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
   		colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
   		colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
   		colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
   		colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
   		colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
   		colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

   		// Customize ImGui style
   		style.WindowRounding    = 5.3f;
   		style.FrameRounding     = 2.3f;
   		style.ScrollbarRounding = 1.5f;
   		style.GrabRounding      = 2.3f;
   		style.WindowBorderSize  = 1.0f;
   		style.FrameBorderSize   = 1.0f;
   		style.ItemSpacing       = ImVec2(10, 8);
	}

	void Application::Run() const
	{
		float delta_time = 0.0f;
		float last_frame = 0.0f;
		
		while (m_running && !glfwWindowShouldClose(m_renderer->GetWindow()))
		{
			const float current_frame = static_cast<float>(glfwGetTime());
			delta_time = current_frame - last_frame;
			last_frame = current_frame;

			m_entity_manager->TickComponents(delta_time);
			m_renderer->Tick(delta_time);

			glfwPollEvents();



			m_entity_manager->PrintEntitiesWithComponent<Position>();
		}
	}
}
