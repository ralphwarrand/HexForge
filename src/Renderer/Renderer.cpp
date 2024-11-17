//Hex
#include "Renderer/Renderer.h"
#include "Renderer/Primitives/PrimitiveBase.h"
#include "Renderer/Primitives/LineBatch.h"
#include "Renderer/Primitives/SphereBatch.h"
#include "Renderer/Primitives/CubeBatch.h"
#include "Engine/Logger.h"
#include "Engine/Application.h"
#include "Renderer/Shader.h"
#include "Renderer/Camera.h"
#include "Renderer/ShaderManager.h"

//Lib
#define GLM_ENABLE_EXPERIMENTAL
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

namespace Hex
{
	// Custom deleter function for GLFWwindow
	static void GLFWwindowDeleter(GLFWwindow* window) {
		if (window) {
			glfwDestroyWindow(window);
		}
	}

	Renderer::Renderer(const AppSpecification& application_spec): m_window(nullptr, GLFWwindowDeleter)
	{
		Init(application_spec);
	}

	Renderer::~Renderer()
	{

	}

	void Renderer::Init(const AppSpecification& app_spec)
	{
		InitOpenGLContext(app_spec);
		LogRendererInfo();
		
		glEnable(GL_DEBUG_OUTPUT);
		
		m_camera.reset(new Camera({-20.f, 20.f, 20.f}, -45.0f, -20.f));

		CreateTestScene();

		SetupCallBacks();
		
		glfwSetInputMode(m_window.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

		if(app_spec.vsync)
		{
			glfwSwapInterval(1); // Enable VSync
		}
		else
		{
			glfwSwapInterval(0); // Disable VSync
		}

	}

	void Renderer::Tick(const float& delta_time)
	{
		// Start the ImGui frame
		StartImGuiFrame();

    	// Clear the screen at the start of the frame
    	int display_w, display_h;
    	glfwGetFramebufferSize(m_window.get(), &display_w, &display_h);
    	glViewport(0, 0, display_w, display_h);

    	// Clear color and depth buffers
    	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Set the background color
    	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    	// Enable depth testing
    	glEnable(GL_DEPTH_TEST);

		m_camera->ProcessKeyboardInput(m_window.get(), delta_time);
		m_camera->Tick(delta_time);

    	// Update and render 3D content
    	UpdateRenderData();
    	for (const auto primitive : m_primitives)
    	{
    	    primitive->SetRenderData(m_render_data);

    	    if (primitive->ShouldCullBackFaces())
    	    {
    	        glEnable(GL_CULL_FACE);
    	        glCullFace(GL_BACK);
    	    }

    	    const auto shader = primitive->GetShaderProgram();
    	    if (!shader)
    	    {
    	        Log(LogLevel::Error, "Failed to get shader program");
    	        continue;
    	    }
    	    shader->Bind();
    	    shader->SetUniformMat4("model", primitive->GetModelMatrix());
    	    shader->SetUniform1i("shade", primitive->ShouldShade());
    		primitive->Draw();
    	    Hex::Shader::Unbind();

    	    glDisable(GL_CULL_FACE);
    	}

		EndImGuiFrame(delta_time);

    	// Swap buffers at the end
    	glfwSwapBuffers(m_window.get());
	}

	void Renderer::CreateTestScene()
	{
		m_light_pos = glm::vec3(1.f, 1.f, 1.f);

		if (auto* line_batch = GetOrCreateLineBatch()) DrawOrigin(*line_batch);

		constexpr int rows = 40;
		constexpr int columns = 40;

		for (int i = 0; i < rows * columns; i++) {
			constexpr float spacing = 4.f;

			const int x = i % columns;      // Calculate x based on the remainder
			const int z = i / columns;      // Calculate z based on the quotient

			glm::vec3 random_colour = glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f));

			GetOrCreateLineBatch()->AddLine(
				glm::vec3(1 + x * spacing, 0.f, 1 + z * spacing),
				glm::vec3(1 + x * spacing, 10.f, 1 + z * spacing),
				random_colour
			);

			//TODO: Sphere hashing
			GetOrCreateSphereBatch()->AddSphere(
				glm::vec3(2 + x * spacing, 0.f, -2 + -z * spacing),
				0.2f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (2.f - 0.2f),
				random_colour
			);

			GetOrCreateCubeBatch()->AddCube(
				glm::vec3(-2 -1.f * x * spacing, 0.f,-2 -1.f * z * spacing), // Position
				0.2f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (2.f - 0.2f),														// Size
				random_colour                                     //	 Color
			);
		}
	}

	void Renderer::RemovePrimitive(Primitive *primitive)
	{
		std::erase_if(m_primitives, [primitive](const auto& p) { return p.get() == primitive; });

		// If the removed primitive was the cached LineBatch, reset the cache
		if (primitive == m_cached_line_batch) {
			m_cached_line_batch = nullptr;
		}
	}

	void Renderer::InitOpenGLContext(const AppSpecification& app_spec)
	{
		if (!glfwInit()) {
			Log(LogLevel::Fatal, "GLFW failed to initialise");
			exit(EXIT_FAILURE);
		}
		else
		{
			Log(LogLevel::Info, "GLFW initialised");
		}

		//using OpenGL 4.6
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		//TODO: Remove from release build?
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

		if(app_spec.fullscreen)
		{
			m_window.reset(glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), glfwGetPrimaryMonitor(), nullptr));
		}
		else
		{
			m_window.reset(glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), nullptr, nullptr));
		}
		
		if (!m_window)
		{
			Log(LogLevel::Fatal, "GLFW failed to create window");
			glfwTerminate();
			exit(EXIT_FAILURE);
		}
		Log(LogLevel::Info, "GLFW window created");
		
		//set callbacks
		glfwSetErrorCallback([](const int error, const char* description)
		{
			const std::string error_string = std::to_string(error) + ":" + description;
			Log(LogLevel::Error, error_string);
		});


		glfwMakeContextCurrent(m_window.get());

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			std::cout << "Failed to initialize GLAD" << std::endl;
		}

		glViewport(0, 0, app_spec.width, app_spec.height);

		int flags;
		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
			Log(LogLevel::Info, "OpenGL debug context enabled");
		}

	}

	void Renderer::SetupCallBacks()
	{
		glfwSetWindowUserPointer(m_window.get(), this);
		
		//glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
		//	[[maybe_unused]] const void* user_param) {
		//	std::cerr << "OpenGL Debug Message: " << message << '\n';
		//	}, nullptr
		//);

		glfwSetCursorPosCallback(m_window.get(), [](GLFWwindow* window, const double x_delta, const double y_delta)
		{
			const auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
					
			static double last_x = x_delta, last_y = y_delta;
			static bool first_mouse = true;
					
			if (first_mouse) {
				last_x = x_delta;
				last_y = y_delta;
				first_mouse = false;
			}
		
			const double x_offset = x_delta - last_x;
			const double y_offset = last_y - y_delta; // Reversed since y-coordinates go from bottom to top
					
			last_x = x_delta;
			last_y = y_delta;
					
			self->m_camera->ProcessMouseInput(x_offset, y_offset);
		});

		glfwSetScrollCallback(m_window.get(), [](GLFWwindow* window, double xoffset, const double yoffset)
		{
			const auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
			self->m_camera->ProcessMouseScroll(static_cast<float>(yoffset));
		});

		glfwSetFramebufferSizeCallback(m_window.get(), [](GLFWwindow* window, const int width, const int height)
		{
			glViewport(0, 0, width, height);
		});
	}

	void Renderer::LogRendererInfo()
	{
		Log(LogLevel::Info, std::format("Running GLFW {}", reinterpret_cast<const char*>(glfwGetVersionString())));
		Log(LogLevel::Info, std::format("Running OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION))));
		Log(LogLevel::Info, std::format("Running GLSL {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
		Log(LogLevel::Info, std::format("Using GPU: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
		Log(LogLevel::Info, "Renderer Initialised\n");
	}

	LineBatch* Renderer::GetOrCreateLineBatch()
	{
		// If the LineBatch is already cached, return it
		if (m_cached_line_batch) {
			return m_cached_line_batch;
		}

		// If not found in the cache, create a new LineBatch
		m_cached_line_batch = AddAndGetPrimitive<LineBatch>();

		const auto line_shader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/line.vert",
			RESOURCES_PATH "shaders/line.frag"
		);

		m_cached_line_batch->SetShaderProgram(line_shader);

		return m_cached_line_batch;
	}

	SphereBatch * Renderer::GetOrCreateSphereBatch()
	{
		// If the LineBatch is already cached, return it
		if (m_cached_sphere_batch) {
			return m_cached_sphere_batch;
		}

		// If not found in the cache, create a new SphereBatch
		m_cached_sphere_batch = AddAndGetPrimitive<SphereBatch>();

		const auto sphere_shader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/debug.vert",
			RESOURCES_PATH "shaders/debug.frag"
		);

		m_cached_sphere_batch->SetShaderProgram(sphere_shader);

		return m_cached_sphere_batch;
	}

	CubeBatch* Renderer::GetOrCreateCubeBatch()
	{
		// If the CubeBatch is already cached, return it
		if (m_cached_cube_batch) {
			return m_cached_cube_batch;
		}

		// If not found in the cache, create a new CubeBatch
		m_cached_cube_batch = AddAndGetPrimitive<CubeBatch>();

		// Assign a shader program to the CubeBatch
		const auto cube_shader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/debug.vert",
			RESOURCES_PATH "shaders/debug.frag"
		);

		m_cached_cube_batch->SetShaderProgram(cube_shader);

		return m_cached_cube_batch;
	}

	GLFWwindow* Renderer::GetWindow() const
	{
		return m_window.get();
	}

	Camera* Renderer::GetCamera() const
	{
		return m_camera.get();
	}

	void Renderer::UpdateRenderData()
	{
		// Update view and projection matrices from the camera
		m_render_data.view = m_camera->GetViewMatrix();          // View matrix from the camera
		m_render_data.projection = m_camera->GetProjectionMatrix(); // Projection matrix from the camera

		// Update the camera's position
		m_render_data.view_pos = m_camera->GetPosition();

		// Padding explicitly set to 0 (required for std140 uniform alignment)
		m_render_data.padding1 = 0.0f;

		// Update wireframe mode (bool is treated as 4-byte int in std140)
		m_render_data.wireframe = m_wireframe_mode;

		// Update light position
		m_render_data.light_pos = m_light_pos;

		// Ensure any other padding or alignment-specific fields are properly set (if applicable)
		// Example: If additional padding is needed for future fields, initialize them here.
	}

	void Renderer::SetLightPos(const glm::vec3 &pos)
	{
		m_light_pos = pos;
		m_render_data.light_pos = pos;
	}

	void Renderer::StartImGuiFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Create a dock space directly over the viewport
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	}

	void Renderer::EndImGuiFrame(const float& delta_time)
	{
		ShowDebugUI(delta_time);

		// Render ImGui
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Handle multi-viewports
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// Save the current OpenGL context
			GLFWwindow* backup_context = glfwGetCurrentContext();

			// Update and render all platform windows
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();

			// Restore the saved context
			glfwMakeContextCurrent(backup_context);
		}
	}

	void Renderer::ShowDebugUI(const float& delta_time)
	{

		ImGui::Begin("Main Window");

		if (ImGui::CollapsingHeader("Rendering Metrics"))
		{
			ImGui::Text("FPS: %.1f", 1.0f / delta_time);
			ImGui::Text("Frame-time: %.6f ms", delta_time * 1000.0f, delta_time);
		}

		//if (ImGui::CollapsingHeader("Rendering Settings"))
		//{
		//	ImGui::SliderFloat("Ambient Light", nullptr, 0.0f, 1.0f);
		//	ImGui::SliderFloat("Diffuse Intensity", nullptr, 0.0f, 1.0f);
		//}


		if (ImGui::CollapsingHeader("Scene Information"))
		{
			ImGui::Text("Active Primitives: %d", static_cast<int>(m_primitives.size()));
			ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
						m_camera->GetPosition().x,
						m_camera->GetPosition().y,
						m_camera->GetPosition().z);
			ImGui::Separator();
			ImGui::Text("Camera View: %s", glm::to_string(m_camera->GetViewMatrix()).c_str());
			ImGui::Separator();
			ImGui::Text("Camera Proj: %s", glm::to_string(m_camera->GetProjectionMatrix()).c_str());
		}

		if (ImGui::CollapsingHeader("Lighting Information"))
		{
			constexpr int active_lights_count = 1; //TODO: update to reflect actual light count
			constexpr int selected_light_index = 1; //TODO: update to reflect actual selected light index

			// Display static lighting information
			ImGui::Text("Active Lights: %d", active_lights_count); // Assume active_lights_count is defined
			ImGui::Text("Selected Light: %d", selected_light_index); // Assume selected_light_index is defined
			ImGui::Text("Light Position:");

			// Add interactive controls for editing the light position
			if (ImGui::DragFloat3("LightPosition", &m_light_pos.x, 0.1f, -10000.0f, 10000.0f))
			{
				// Update light position
				SetLightPos(m_light_pos);
			}

			ImGui::Separator();
		}



		if (ImGui::Button("Toggle Wireframe"))
		{
			m_wireframe_mode = !m_wireframe_mode; // Toggle the state

			if (m_wireframe_mode)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Enable wireframe
			}
			else
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Enable default fill mode
			}
		}

		ImGui::End();
	}

	void Renderer::DrawOrigin(LineBatch& line_batch)
	{
		line_batch.AddLine({0, -50, 0}, {0, 50, 0}, {0.f, 1.f, 0.f});
		line_batch.AddLine({-50, 0, 0}, {50, 0, 0}, {1.f, 0.f, 0.f});
		line_batch.AddLine({0, 0, -50}, {0, 0, 50}, {0.f, 0.f, 1.f});
	}
}