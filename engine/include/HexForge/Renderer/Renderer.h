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

        void RenderWorld(const float& delta_time);

        void RequestViewportFocus();

        // Getters
        [[nodiscard]] GLFWwindow* GetWindow() const;
        [[nodiscard]] Camera* GetCamera() const;

        // --- Getters for UI ---
        unsigned int GetFrameBufferTexture() const { return m_frame_buffer.texture; }
        unsigned int GetShadowMapTexture() const { return m_shadow_map.texture; }
        float GetFrameBufferWidth() const { return static_cast<float>(m_frame_buffer.render_width); }
        float GetFrameBufferHeight() const { return static_cast<float>(m_frame_buffer.render_height); }
        void ResizeFrameBuffer(float width, float height);

        // --- Public members for UI access ---
        bool m_wireframe_mode = false;
        glm::vec3 m_light_dir{ -0.5f, -1.0f, -0.5f };
        bool m_requestFocus = false;
        void SetLightDir(const glm::vec3 &dir);

    private:
        void Init(const AppSpecification& app_spec);
        void InitOpenGLContext(const AppSpecification& app_spec);

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
        void RenderSceneBatched() const;
        void RenderShadowMap();

        void UpdateRenderData();


        // Define a unique_ptr with a custom deleter type alias
        using GLFWwindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
        GLFWwindowPtr m_window;

        // Access to the application console
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
        glm::vec3 m_light_color{1.0f, 0.95f, 0.95f};

        // Debug Settings
        float m_shadow_map_zoom{1.f};
        glm::vec2 m_shadow_map_pan{0.f, 0.f};
        bool m_enable_debug_output{true};
        bool m_show_metrics{true};
        bool m_show_scene_info{true};
        bool m_show_lighting_tool{true};
    };
}
