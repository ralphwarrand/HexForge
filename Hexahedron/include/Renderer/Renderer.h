#pragma once

struct GLFWwindow;

namespace Hex
{
	// Forward declarations
	struct ApplicationSpecification;
	class Camera;
	class Shader;
	class LineBatch;

	class Renderer
	{
	public:
		Renderer() = delete;
		explicit Renderer(const ApplicationSpecification& application_spec);
		~Renderer();

		void Tick() const;

		// Getters
		GLFWwindow* GetWindow() const;
		Camera* GetCamera() const;

	private:
		void Init(const ApplicationSpecification& application_spec);
		
		GLFWwindow* m_window;
		Camera* m_camera;

		LineBatch* m_line_batch;
	};
}
