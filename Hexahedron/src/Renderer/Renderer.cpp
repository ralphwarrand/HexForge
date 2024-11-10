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
	Renderer::Renderer(const ApplicationSpecification& application_spec)
	{
		Init(application_spec);
		
		m_line_batch = new LineBatch();
		
		m_line_batch->AddLine({0, -50, 0}, {0, 50, 0}, {0.f, 1.f, 0.f});
		m_line_batch->AddLine({-50, 0, 0}, {50, 0, 0}, {1.f, 0.f, 0.f});
		m_line_batch->AddLine({0, 0, -50}, {0, 0, 50}, {0.f, 0.f, 1.f});
	}

	Renderer::~Renderer()
	{
		glfwDestroyWindow(m_window);
		delete m_camera;
		delete m_line_batch;
	}

	void Renderer::Tick() const
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		m_line_batch->GetShaderProgram()->Bind();
		
		m_line_batch->GetShaderProgram()->SetUniformMat4("view", m_camera->GetViewMatrix());
		m_line_batch->GetShaderProgram()->SetUniformMat4("projection", m_camera->GetProjectionMatrix());
	
		m_line_batch->Draw();
		
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

	void Renderer::Init(const ApplicationSpecification& application_spec)
	{
		Log(LogLevel::Info, "Renderer Initialising...");
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

		m_window = glfwCreateWindow(application_spec.width, application_spec.height, application_spec.name.c_str(), nullptr, nullptr);
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
			const std::string errorString = std::to_string(error) + ":" + description;
			Log(LogLevel::Error, errorString);
		});

		glfwMakeContextCurrent(m_window);

		if (const GLenum err = glewInit(); GLEW_OK != err)
		{
			Log(LogLevel::Error, "GLEW failed to initialise");
			Log(LogLevel::Error, reinterpret_cast<const char*>(glewGetErrorString(err)));
		}
		else
		{
			Log(LogLevel::Info, "GLEW initialised");
		}

		//log glfw version
		{
			Log(LogLevel::Info, std::format("Running GLFW {}", reinterpret_cast<const char*>(glfwGetVersionString())));
		}

		//log glew version
		{
			Log(LogLevel::Info, std::format("Running GLEW {}", reinterpret_cast<const char*>(glewGetString(GLEW_VERSION))));
		}

		//log opengl version
		{
			Log(LogLevel::Info, std::format("Running OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION))));
			Log(LogLevel::Info, std::format("Running GLSL {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
			Log(LogLevel::Info, std::format("Using GPU: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
		}

		Log(LogLevel::Info, "Renderer Initialised\n");

		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
								  GLsizei length, const GLchar* message, const void* userParam) {
			std::cerr << "OpenGL Debug Message: " << message << '\n';
		}, nullptr);

		glfwSetWindowUserPointer(m_window, this);

		m_camera = new Camera({0.f, 0.f, 10.f}, -90.0f, 0.f);
		
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
		
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		
		
	}
}