#pragma once

//Hex
#include "RenderStructs.h"

//Lib
#include <glm/glm.hpp>

//STL
#include <memory>
#include <vector>

#include "entt/entt.hpp"

struct GLFWwindow;

namespace Hex
{
    // Forward declarations
    class Console;

    class Renderer
    {
    public:
        Renderer() = delete;
        explicit Renderer(const AppSpecification& application_spec, const std::shared_ptr<Console>& console);
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

        static void CheckFrameBufferStatus();

        void InitShadowMap();
        void InitFrameBuffer(const int& width, const int& height);
        void BindFrameBuffer() const;
        void BindWindowBuffer() const;
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

        std::shared_ptr<Console> m_console{nullptr};

        std::unique_ptr<Camera> m_camera{nullptr};
        RenderData m_render_data{};
        RenderData m_old_render_data{};

        std::vector<std::shared_ptr<Primitive>> m_primitives;
        LineBatch* m_cached_line_batch{nullptr};
        SphereBatch* m_cached_sphere_batch{nullptr};
        CubeBatch* m_cached_cube_batch{nullptr};
        std::unique_ptr<ScreenQuad> m_screen_quad{nullptr};

        glm::vec3 m_light_pos{};
        ShadowMap m_shadow_map{};

        float m_shadow_map_zoom{1.f};
        glm::vec2 m_shadow_map_pan{0.f, 0.f};

        FrameBuffer m_frame_buffer{};

        bool m_wireframe_mode{false};

        // Debug output toggle
        bool m_enable_debug_output{true};

        bool m_show_metrics{true};
        bool m_show_scene_info{true};
        bool m_show_lighting_tool{true};
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
        static_assert(std::is_base_of_v<Primitive, T>, "T must derive from Primitive");

        // Create and store the primitive
        auto primitive = std::make_shared<T>(std::forward<Args>(args)...);
        m_primitives.emplace_back(primitive);

        // Return a pointer to the newly created primitive
        return static_cast<T*>(primitive.get());
    }
}