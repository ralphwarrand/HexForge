#pragma once
#include <memory>
#include <vector>



struct GLFWwindow;

namespace Hex
{
	// Forward declarations
	struct AppSpecification;
	class Camera;
	class Shader;
	class Mesh;
	class Primitive;
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
		
		template<typename T, typename... Args>
		void AddPrimitive(Args&&... args);

		// Getters
		[[nodiscard]] GLFWwindow* GetWindow() const;
		[[nodiscard]] Camera* GetCamera() const;

	private:
		void Init(const AppSpecification& app_spec);
		void InitOpenGLContext(const AppSpecification& app_spec);
		static void LogRendererInfo();
		static void DrawOrigin(LineBatch& line_batch);
		
		GLFWwindow* m_window;
		Camera* m_camera;

		// Primitives
		std::vector<std::shared_ptr<Primitive>> m_primitives;
	};

	template <typename T, typename ... Args>
	void Renderer::AddPrimitive(Args&&... args)
	{
		m_primitives.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
	}
}
