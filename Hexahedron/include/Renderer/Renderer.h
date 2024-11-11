#pragma once

struct GLFWwindow;

namespace Hex
{
	// Forward declarations
	struct AppSpecification;
	class Camera;
	class Shader;
	class LineBatch;

	class Renderer
	{
	public:
		Renderer() = delete;
		explicit Renderer(const AppSpecification& application_spec);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;

		Renderer& operator = (const Renderer&) = delete;
		Renderer& operator = (Renderer&&) = delete;

		void Tick() const;

		// Getters
		[[nodiscard]] GLFWwindow* GetWindow() const;
		[[nodiscard]] Camera* GetCamera() const;

	private:
		void Init(const AppSpecification& app_spec);
		
		GLFWwindow* m_window;
		Camera* m_camera;

		// Primitives
		LineBatch* m_line_batch;
	};
}
