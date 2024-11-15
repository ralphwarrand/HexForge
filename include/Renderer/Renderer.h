#pragma once

//Lib
#include <glm/glm.hpp>

//STL
#include <memory>
#include <vector>


struct GLFWwindow;

namespace Hex
{
	class UVSphere;
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
		glm::mat4 view;         // 64 bytes (16-byte alignment)
		glm::mat4 projection;   // 64 bytes (16-byte alignment)
		glm::vec3 view_pos;     // 12 bytes (aligned to 16 bytes)
		float padding;          // 4 bytes (to align to 16 bytes)
		bool wireframe;         // 4 bytes (aligned to 16 bytes due to vec3)
		float padding2[3];      // 12 bytes (to ensure struct size is a multiple of 16 bytes)
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

		void Tick(const float& delta_time);

		void CreateTestScene();
		
		template <typename T, typename... Args>
		void AddPrimitive(Args&&... args);

		void RemovePrimitive(Primitive* primitive);

		template <typename T, typename... Args>
		T* AddAndGetPrimitive(Args&&... args);

		LineBatch* GetOrCreateLineBatch();

		// Getters
		[[nodiscard]] GLFWwindow* GetWindow() const;
		[[nodiscard]] Camera* GetCamera() const;

	private:
		void Init(const AppSpecification& app_spec);
		void InitOpenGLContext(const AppSpecification& app_spec);
		void SetupCallBacks();
		static void LogRendererInfo();
		void UpdateRenderData();

		static void StartImGuiFrame();

		void ShowDebugUI(const float& delta_time);

		static void DrawOrigin(LineBatch& line_batch);

		// Define a unique_ptr with a custom deleter type alias
		using GLFWwindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
		GLFWwindowPtr m_window;

		std::unique_ptr<Camera> m_camera{nullptr};
		RenderData m_render_data{};

		std::vector<std::shared_ptr<Primitive>> m_primitives;
		LineBatch* m_cached_line_batch = nullptr;

		glm::vec3 m_light_pos{};

		UVSphere* Test{nullptr};

		bool m_wireframe_mode{false};
	};

	template <typename T, typename... Args>
	void Renderer::AddPrimitive(Args&&... args)
	{
		// Emplace a new instance of T directly by forwarding the constructor arguments
		m_primitives.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
	}

	template <typename T, typename... Args>
	T* Renderer::AddAndGetPrimitive(Args&&... args)
	{
		static_assert(std::is_base_of<Primitive, T>::value, "T must derive from Primitive");

		// Create and store the primitive
		auto primitive = std::make_shared<T>(std::forward<Args>(args)...);
		m_primitives.emplace_back(primitive);

		// Return a pointer to the newly created primitive
		return static_cast<T*>(primitive.get());
	}
}