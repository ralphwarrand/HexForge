//Hex
#include "Renderer/Renderer.h"
#include "Renderer/Primitives.h"
#include "Engine/Logger.h"
#include "Engine/Application.h"
#include "Renderer/Shader.h"
#include "Renderer/Camera.h"

//Lib
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Hex
{
	Renderer::Renderer(const AppSpecification& application_spec)
	{
		Init(application_spec);

		auto line_batch = LineBatch();
		line_batch.SetShaderProgram(new Shader(
			"resources/shaders/line.vert",
			"resources/shaders/line.frag"
		));
		DrawOrigin(line_batch);
		AddPrimitive<LineBatch>(line_batch);

		auto sphere = UVSphere(2.f, 80, 40);
		sphere.SetShaderProgram(new Shader(
			"resources/shaders/debug.vert",
			"resources/shaders/debug.frag"
		));
		AddPrimitive<UVSphere>(sphere);
	}

	Renderer::~Renderer()
	{
		glfwDestroyWindow(m_window);
		delete m_camera;
	}

	void Renderer::Tick() const
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		for (const auto& primitive : m_primitives) {
			//Set culling
			if(primitive->ShouldCullBackFaces())
			{
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);
			}
			
			// Retrieve shader from primitive and bind
			const auto shader = primitive->GetShaderProgram();
			shader->Bind();
			
			// Set uniforms
			shader->SetUniformMat4("view", m_camera->GetViewMatrix());
			shader->SetUniformMat4("projection", m_camera->GetProjectionMatrix());
			shader->SetUniformVec3("viewPosition", m_camera->GetPosition());
			shader->SetUniform1i("shade", primitive->ShouldShade());
			
			primitive->Draw();

			Hex::Shader::Unbind();

			glDisable(GL_CULL_FACE);
		}
		
		glfwSwapBuffers(m_window);
	}

	::GLFWwindow* Renderer::GetWindow() const
	{
		return m_window;
	}

	Camera* Renderer::GetCamera() const
	{
		return m_camera;
	}

	void Renderer::Init(const AppSpecification& app_spec)
	{
		InitOpenGLContext(app_spec);
		LogRendererInfo();
		
		glEnable(GL_DEBUG_OUTPUT);
		
		m_camera = new Camera({0.f, 0.f, 10.f}, -90.0f, 0.f);

		SetupCallBacks();
		
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
		glewExperimental = GL_TRUE;

		if(app_spec.fullscreen)
		{
			m_window = glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), glfwGetPrimaryMonitor(), nullptr);
		}
		else
		{
			m_window = glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), nullptr, nullptr);
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

		glfwMakeContextCurrent(m_window);
		glViewport(0, 0, app_spec.width, app_spec.height);

		if (const GLenum err = glewInit(); GLEW_OK != err)
		{
			Log(LogLevel::Error, "GLEW failed to initialise");
			Log(LogLevel::Error, reinterpret_cast<const char*>(glewGetErrorString(err)));
		}
		else
		{
			Log(LogLevel::Info, "GLEW initialised");
		}
	}

	void Renderer::SetupCallBacks()
	{
		glfwSetWindowUserPointer(m_window, this);
		
		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
			[[maybe_unused]] const void* user_param) {
			std::cerr << "OpenGL Debug Message: " << message << '\n';
			}, nullptr
		);

		glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, const double x_delta, const double y_delta)
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

		glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, const double yoffset)
		{
			const auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
			self->m_camera->ProcessMouseScroll(static_cast<float>(yoffset));
		});

		glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, const int width, const int height)
		{
			glViewport(0, 0, width, height);
		});
	}

	void Renderer::LogRendererInfo()
	{
		//log glfw version
		Log(LogLevel::Info, std::format("Running GLFW {}", reinterpret_cast<const char*>(glfwGetVersionString())));
		
		//log glew version
		Log(LogLevel::Info, std::format("Running GLEW {}", reinterpret_cast<const char*>(glewGetString(GLEW_VERSION))));

		//log opengl version
		Log(LogLevel::Info, std::format("Running OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION))));
		Log(LogLevel::Info, std::format("Running GLSL {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
		Log(LogLevel::Info, std::format("Using GPU: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER))));

		Log(LogLevel::Info, "Renderer Initialised\n");
	}

	void Renderer::DrawOrigin(LineBatch& line_batch)
	{
		line_batch.AddLine({0, -50, 0}, {0, 50, 0}, {0.f, 1.f, 0.f});
		line_batch.AddLine({-50, 0, 0}, {50, 0, 0}, {1.f, 0.f, 0.f});
		line_batch.AddLine({0, 0, -50}, {0, 0, 50}, {0.f, 0.f, 1.f});
	}
}