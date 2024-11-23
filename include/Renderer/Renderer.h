#pragma once

//Lib
#include <glm/glm.hpp>
#include <glad/glad.h>

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

    // Primitives
    class Primitive;
    class LineBatch;
    class SphereBatch;
    class CubeBatch;
    struct ScreenQuad;

    struct Material
    {
        glm::vec3 ambient_color;
        glm::vec3 diffuse_color;
        glm::vec3 specular_color;
        float shininess;
    };

    struct alignas(16) RenderData
    {
        glm::mat4 view;         // 64 bytes (16-byte alignment)
        glm::mat4 projection;   // 64 bytes (16-byte alignment)
        glm::vec3 view_pos;     // 12 bytes
        float padding1;         // 4 bytes (to align to 16 bytes)
        glm::vec3 light_pos;    // 12 bytes
        float padding2;         // 4 bytes (to align to 16 bytes)
        bool wireframe;         // 4 bytes (std140 treats bool as 4-byte int)
        float padding3[3];      // 12 bytes (to align struct size to 16 bytes)
        bool operator==(const RenderData & render_data) const = default;
    };

    class Renderer
    {
    public:
        Renderer() = delete;
        explicit Renderer(const AppSpecification& application_spec);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;

        Renderer& operator=(const Renderer&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void Tick(const float& delta_time);

        void CreateTestScene();

        template <typename T, typename... Args>
        void AddPrimitive(Args&&... args);

        void RemovePrimitive(Primitive* primitive);

        template <typename T, typename... Args>
        T* AddAndGetPrimitive(Args&&... args);

        LineBatch* GetOrCreateLineBatch();
        SphereBatch* GetOrCreateSphereBatch();
        CubeBatch* GetOrCreateCubeBatch();

        // Getters
        [[nodiscard]] GLFWwindow* GetWindow() const;
        [[nodiscard]] Camera* GetCamera() const;

    private:
        void Init(const AppSpecification& app_spec);
        void InitOpenGLContext(const AppSpecification& app_spec);
        void SetupCallBacks();
        static void LogRendererInfo();

        void InitShadowMap();
        void RenderShadowMap();
        void RenderFullScreenQuad() const;
        void RenderScene() const;
        void UpdateRenderData();

        void SetLightPos(const glm::vec3& pos);
        static void DrawOrigin(LineBatch& line_batch);

        //ImGui
        static void StartImGuiFrame();
        void EndImGuiFrame(const float& delta_time);
        void ShowDebugUI(const float& delta_time);

        // Define a unique_ptr with a custom deleter type alias
        using GLFWwindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
        GLFWwindowPtr m_window;

        std::unique_ptr<Camera> m_camera{nullptr};
        RenderData m_render_data{};
        RenderData m_old_render_data{};

        std::vector<std::shared_ptr<Primitive>> m_primitives;
        LineBatch* m_cached_line_batch{nullptr};
        SphereBatch* m_cached_sphere_batch{nullptr};
        CubeBatch* m_cached_cube_batch{nullptr};
        std::unique_ptr<ScreenQuad> m_screen_quad{nullptr};

        glm::vec3 m_light_pos{};

        GLuint m_shadow_map_fbo;         // Framebuffer for shadow mapping
        GLuint m_shadow_map_texture;     // Texture to store the depth information
        glm::mat4 m_light_view;          // View matrix for the light
        glm::mat4 m_light_projection;    // Projection matrix for the light
        int shadow_width = 2048, shadow_height = 2048;

        float m_shadow_map_zoom{1.f};
        glm::vec2 m_shadow_map_pan{0.f, 0.f};

        bool m_wireframe_mode{false};

        // Debug output toggle
        bool m_enable_debug_output{true};

        bool m_show_metrics{false};
        bool m_show_scene_info{false};
        bool m_show_lighting_tool{false};
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