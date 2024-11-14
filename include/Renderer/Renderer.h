#pragma once

//Lib
#include <glm/glm.hpp>

//STL
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

	struct Material
	{
		glm::vec3 ambient_color;
		glm::vec3 diffuse_color;
		glm::vec3 specular_color;
		float shininess;
	};

	struct RenderData
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec3 view_pos;
	};

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

		void Tick();
		
		template <typename T, typename... Args>
		void AddPrimitive(Args&&... args);

		// Getters
		[[nodiscard]] GLFWwindow* GetWindow() const;
		[[nodiscard]] Camera* GetCamera() const;

	private:
		void Init(const AppSpecification& app_spec);
		void InitOpenGLContext(const AppSpecification& app_spec);
		void SetupCallBacks();
		static void LogRendererInfo();
		
		static void DrawOrigin(LineBatch& line_batch);

		RenderData m_render_data;
		GLFWwindow* m_window;
		Camera* m_camera;
		
		std::vector<std::shared_ptr<Primitive>> m_primitives;
	};

	template <typename T, typename... Args>
	void Renderer::AddPrimitive(Args&&... args)
	{
		// Emplace a new instance of T directly by forwarding the constructor arguments
		m_primitives.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
	}
}