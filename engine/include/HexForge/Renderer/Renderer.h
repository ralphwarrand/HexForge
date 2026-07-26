#pragma once

// Third-party
#include <glm/glm.hpp>
#include <entt/entt.hpp>

//STL
#include <memory>

//Hex
#include "Data/RenderStructs.h"
#include "HexForge/Renderer/ParticleRenderer.h"
#include "HexForge/Renderer/PostProcess.h"
#include "HexForge/Renderer/FluidRenderer.h"

struct GLFWwindow;

namespace Hex
{
    // Forward declarations
    class Console;
    class PhysicsSystem;

    enum class RenderMode : int
    {
        ParticlesOnly = 0,
        MeshesOnly    = 1,
        Both          = 2,
    };

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
        void RenderDebug() const;

        void RequestViewportFocus();

        // Getters
        [[nodiscard]] GLFWwindow* GetWindow() const;
        [[nodiscard]] Camera* GetCamera() const;

        // --- Getters for UI ---
        // Returns the post-processed LDR texture (what should be displayed). Falls back to the
        // scene FBO if post-processing isn't initialized yet.
        unsigned int GetFrameBufferTexture() const;
        unsigned int GetShadowMapTexture() const { return m_shadow_map.texture; }
        float GetFrameBufferWidth() const { return static_cast<float>(m_frame_buffer.render_width); }
        float GetFrameBufferHeight() const { return static_cast<float>(m_frame_buffer.render_height); }
        void ResizeFrameBuffer(float width, float height);
        PostProcess* GetPostProcess() { return m_post_process.get(); }

        // --- Public members for UI access ---
        bool m_wireframe_mode = false;
        glm::vec3 m_light_dir{ -0.5f, -1.0f, -0.5f };
        bool m_requestFocus = false;
        RenderMode m_render_mode = RenderMode::ParticlesOnly;
        void SetLightDir(const glm::vec3 &dir);
        void SetPhysicsSystem(PhysicsSystem* physics);
        void SetRenderMode(RenderMode mode);

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
        void RenderParticles();
        void RenderInfiniteGrid();
        void RenderClothMeshes();
        void RenderShadowMap();
        // Pose every entity that carries (RigidBodyComponent + RigidBodyVisualComponent +
        // TransformComponent + Model/Material) at its physics-driven centroid+orientation.
        void UpdateRigidBodyVisualTransforms();

        // Grid configuration (read by RenderInfiniteGrid).
        float m_grid_y             = -1.5f;
        float m_grid_minor_spacing = 1.0f;
        float m_grid_major_every   = 10.0f;
        float m_grid_fade_distance = 80.0f;
        glm::vec3 m_grid_color_minor{ 0.18f, 0.22f, 0.28f };
        glm::vec3 m_grid_color_major{ 0.42f, 0.50f, 0.58f };
        glm::vec3 m_grid_color_base { 0.08f, 0.10f, 0.13f };
        float     m_grid_plane_far_fade = 200.0f;

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
        // HDR sun intensity — ACES tonemap maps this back to a natural-looking brightness while
        // leaving headroom for specular highlights.
        glm::vec3 m_light_color{2.8f, 2.6f, 2.3f};

        // Particle renderer (instanced screen-space sphere impostors).
        std::unique_ptr<ParticleRenderer> m_particle_renderer;
        PhysicsSystem* m_physics_system = nullptr; // weak ref — owned by Application
        bool m_particle_materials_dirty = true;

        // HDR post-processing pipeline (bloom + ACES tonemap).
        std::unique_ptr<PostProcess> m_post_process;

        // Screen-space fluid surface (used in MeshesOnly mode).
        std::unique_ptr<FluidRenderer> m_fluid_renderer;

        // Dynamic cloth mesh — single shared VAO/VBO/EBO that gets re-uploaded per frame for
        // every ClothComponent. Capacity is generous so we don't reallocate at runtime.
        GLuint m_cloth_vao = 0;
        GLuint m_cloth_vbo = 0;
        GLuint m_cloth_ebo = 0;
        std::shared_ptr<class Shader> m_cloth_shader;
        static constexpr size_t kMaxClothVerts = 16384; // 128 x 128 cloth fits
        static constexpr size_t kMaxClothIndices = kMaxClothVerts * 6;

        // Debug Settings
        float m_shadow_map_zoom{1.f};
        glm::vec2 m_shadow_map_pan{0.f, 0.f};
        bool m_enable_debug_output{true};
        bool m_show_metrics{true};
        bool m_show_scene_info{true};
        bool m_show_lighting_tool{true};
    };
}
