#pragma once

// STL
#include <memory>
#include <cstdint>

// Third-party
#include <glad/glad.h>
#include <glm/glm.hpp>

// Hex
#include "HexForge/Renderer/Shader.h"

namespace Hex
{
    // Screen-Space Fluid Renderer (van der Laan et al. 2009, "Screen Space Fluid Rendering
    // for Games", extended with thickness-based absorption, SSR and a foam sprite pass).
    //
    // Pipeline per frame (see FluidRenderer.cpp::Execute):
    //   1. Depth + thickness pass — particle sphere impostors into an MRT (R32F view-z, R16F
    //      additive thickness).
    //   2. Separable bilateral smoothing of the depth (edge-stopped so silhouettes survive).
    //   2b. Separable box smoothing of the thickness.
    //   3. Compose — reconstruct normals from smoothed depth, then refraction + Beer-Lambert
    //      absorption + Fresnel/specular + SSR/sky reflection into the HDR scene buffer.
    //   4. Foam — additive sprites for particles flagged as spray/foam.
    //
    // Shaders are owned by the global ShaderManager (shared_ptr), so this class does not free them.
    class FluidRenderer
    {
    public:
        FluidRenderer();
        ~FluidRenderer();

        FluidRenderer(const FluidRenderer&)            = delete;
        FluidRenderer& operator=(const FluidRenderer&) = delete;

        void Init(int width, int height);
        void Shutdown();
        void Resize(int width, int height);

        void Execute(GLuint particle_pos_vbo, GLuint particle_flags_vbo,
                     uint32_t particle_count, float particle_radius,
                     const glm::mat4& view, const glm::mat4& proj,
                     const glm::vec3& light_dir_world, const glm::vec3& light_color,
                     GLuint scene_color_tex, GLuint scene_depth_tex,
                     int scene_w, int scene_h, GLuint scene_fbo,
                     float time);

        // ---- UI-tweakable parameters --------------------------------------------------------
        // Depth smoothing: each iteration is one separable H+V bilateral pass. depth_sigma is the
        // range (edge-stop) tolerance in view-space depth units — larger keeps more, smaller
        // preserves sharper silhouettes.
        int   m_smooth_iters         = 1;
        float m_smooth_depth_sigma   = 1.0f;

        // Thickness smoothing — separable box blur passes; kills per-splat ridges in absorption.
        int   m_thickness_smooth_iters = 1;
        float m_thickness_scale        = 1.0f;

        // Surface shading.
        float m_fresnel_f0           = 0.02f;   // water's real F0 ≈ 0.02
        float m_refraction_strength  = 1.0f;
        float m_caustic_strength     = 0.0f;

        // Screen-space reflection.
        bool  m_enable_ssr           = true;
        int   m_ssr_steps            = 8;
        float m_ssr_max_distance     = 0.5f;    // fraction of view distance marched

        // Water body colour. tint = colour that survives absorption (so it's bluish); scatter is
        // the in-scattering (SSS) colour added in thick regions.
        glm::vec3 m_water_tint       = { 0.30f, 0.62f, 0.88f };
        glm::vec3 m_water_scatter    = { 0.10f, 0.46f, 0.66f };

        // Foam sprites.
        float m_foam_size_scale      = 0.8f;
        float m_foam_alpha           = 0.25f;

    private:
        void CreateFBOs(int w, int h);
        void DestroyFBOs();
        void DrawFullScreen();

        int m_width  = 0;
        int m_height = 0;

        // Shaders (owned by ShaderManager — shared, not freed here).
        std::shared_ptr<Shader> m_depth_shader;
        std::shared_ptr<Shader> m_smooth_shader;
        std::shared_ptr<Shader> m_thick_smooth_shader;
        std::shared_ptr<Shader> m_compose_shader;
        std::shared_ptr<Shader> m_foam_shader;

        // Fullscreen quad (smooth + compose).
        GLuint m_quad_vao = 0;
        GLuint m_quad_vbo = 0;

        // Instanced particle quad (depth + foam passes).
        GLuint m_particle_vao  = 0;
        GLuint m_particle_quad = 0;
        GLuint m_particle_ebo  = 0;

        // Depth + thickness MRT.
        GLuint m_depth_fbo          = 0;
        GLuint m_depth_tex          = 0;   // R32F view-space z (negative inside fluid, +1 sentinel)
        GLuint m_thickness_tex      = 0;   // R16F additive thickness
        GLuint m_depth_renderbuffer = 0;

        // Depth smoothing ping-pong (R32F).
        GLuint m_smooth_fbo[2] = { 0, 0 };
        GLuint m_smooth_tex[2] = { 0, 0 };

        // Thickness smoothing ping-pong (R16F).
        GLuint m_thick_smooth_fbo[2] = { 0, 0 };
        GLuint m_thick_smooth_tex[2] = { 0, 0 };
    };
}
