#pragma once



// Third-party
#include <glm/glm.hpp>
#include <entt/entt.hpp>

//STL
#include <memory>

//Hex
#include "Data/RenderStructs.h"

struct GLFWwindow;

namespace Hex
{
    // Forward declarations
    class Console;

    class Renderer
    {
    public:
        Renderer() = delete;
        Renderer(entt::registry& registry, const AppSpecification& application_spec, const std::shared_ptr<Console>& console);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;

        Renderer& operator=(const Renderer&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void Tick(const float& delta_time);

        // Getters
        [[nodiscard]] GLFWwindow* GetWindow() const;
        [[nodiscard]] Camera* GetCamera() const;

    private:
        void Init(const AppSpecification& app_spec);
        void InitOpenGLContext(const AppSpecification& app_spec);
        void SetupCallBacks();
        static void LogRendererInfo();

        static void CheckFrameBufferStatus();

        // Buffers
        void InitShadowMap();
        void InitFrameBuffer(const int& width, const int& height);
        void BindFrameBuffer() const;
        void BindWindowBuffer() const;

        // Rendering
        void RenderFullScreenQuad() const;
        void RenderScene() const;
        void RenderShadowMap();

        void UpdateRenderData();

        void SetLightPos(const glm::vec3& pos);

        //ImGui
        static void StartImGuiFrame();
        void EndImGuiFrame(const float& delta_time);
        void ShowDebugUI(const float& delta_time);

        // Define a unique_ptr with a custom deleter type alias
        using GLFWwindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
        GLFWwindowPtr m_window;

        // Access to application console
        std::shared_ptr<Console> m_console{nullptr};

        // Scene information
        entt::registry& m_registry;
        std::unique_ptr<Camera> m_camera{nullptr};
        RenderData m_render_data{};
        RenderData m_old_render_data{};

        // Buffers
        FrameBuffer m_frame_buffer{};
        ShadowMap m_shadow_map{};
        std::unique_ptr<ScreenQuad> m_screen_quad{nullptr};
        GLuint m_uboRenderData = 0;

        //Lighting
        glm::vec3 m_light_pos{20.f, 10.f, 20.f};

        // Debug Settings
        float m_shadow_map_zoom{1.f};
        glm::vec2 m_shadow_map_pan{0.f, 0.f};
        bool m_wireframe_mode{false};
        bool m_enable_debug_output{true};
        bool m_show_metrics{true};
        bool m_show_scene_info{true};
        bool m_show_lighting_tool{true};

    };
}
