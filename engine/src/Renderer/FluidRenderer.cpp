#include "HexForge/pch.h"
#include "HexForge/Renderer/FluidRenderer.h"
#include "HexForge/Renderer/ShaderManager.h"
#include "HexForge/Core/Logger.h"

namespace Hex
{
    FluidRenderer::FluidRenderer() = default;
    FluidRenderer::~FluidRenderer() { Shutdown(); }

    void FluidRenderer::Init(int width, int height)
    {
        // Fullscreen quad for smooth + compose.
        const float verts[] = {
            -1.f, -1.f, 0.f, 0.f,   1.f, -1.f, 1.f, 0.f,   1.f,  1.f, 1.f, 1.f,
            -1.f, -1.f, 0.f, 0.f,   1.f,  1.f, 1.f, 1.f,  -1.f,  1.f, 0.f, 1.f,
        };
        glGenVertexArrays(1, &m_quad_vao);
        glBindVertexArray(m_quad_vao);
        glGenBuffers(1, &m_quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
        glBindVertexArray(0);

        // Instanced particle quad (locations 0=a_quad, 1=a_world_pos, 2=a_flags).
        const float pquad[] = { -1.f, -1.f,  1.f, -1.f,  1.f, 1.f,  -1.f, 1.f };
        const uint32_t pidx[] = { 0, 1, 2, 0, 2, 3 };
        glGenVertexArrays(1, &m_particle_vao);
        glBindVertexArray(m_particle_vao);
        glGenBuffers(1, &m_particle_quad);
        glBindBuffer(GL_ARRAY_BUFFER, m_particle_quad);
        glBufferData(GL_ARRAY_BUFFER, sizeof(pquad), pquad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
        glGenBuffers(1, &m_particle_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_particle_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(pidx), pidx, GL_STATIC_DRAW);
        glBindVertexArray(0);

        CreateFBOs(width, height);

        m_depth_shader   = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/fluid_depth.vert",
            RESOURCES_PATH "shaders/fluid_depth.frag");
        m_smooth_shader  = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert",
            RESOURCES_PATH "shaders/fluid_smooth.frag");
        m_thick_smooth_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert",
            RESOURCES_PATH "shaders/fluid_thickness_smooth.frag");
        m_compose_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert",
            RESOURCES_PATH "shaders/fluid_compose.frag");
        m_foam_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/fluid_foam.vert",
            RESOURCES_PATH "shaders/fluid_foam.frag");
    }

    void FluidRenderer::CreateFBOs(int w, int h)
    {
        DestroyFBOs();
        m_width = w;
        m_height = h;

        // --- Depth + thickness MRT. Background pixels stay at depth=1 (anything > 0 means "no
        //     fluid here"); we use 1.0 as the sentinel. Thickness is additively accumulated.
        glGenFramebuffers(1, &m_depth_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_depth_fbo);

        glGenTextures(1, &m_depth_tex);
        glBindTexture(GL_TEXTURE_2D, m_depth_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_depth_tex, 0);

        glGenTextures(1, &m_thickness_tex);
        glBindTexture(GL_TEXTURE_2D, m_thickness_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_thickness_tex, 0);

        GLenum draw_bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, draw_bufs);

        glGenRenderbuffers(1, &m_depth_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth_renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth_renderbuffer);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Log(LogLevel::Error, "FluidRenderer: depth FBO incomplete");
        }

        // --- Depth smoothing ping-pong (R32F).
        for (int i = 0; i < 2; ++i) {
            glGenFramebuffers(1, &m_smooth_fbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_smooth_fbo[i]);
            glGenTextures(1, &m_smooth_tex[i]);
            glBindTexture(GL_TEXTURE_2D, m_smooth_tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_smooth_tex[i], 0);
            GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, bufs);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                Log(LogLevel::Error, "FluidRenderer: smooth FBO incomplete");
            }
        }

        // --- Thickness smoothing ping-pong (R16F).
        for (int i = 0; i < 2; ++i) {
            glGenFramebuffers(1, &m_thick_smooth_fbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_thick_smooth_fbo[i]);
            glGenTextures(1, &m_thick_smooth_tex[i]);
            glBindTexture(GL_TEXTURE_2D, m_thick_smooth_tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_thick_smooth_tex[i], 0);
            GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, bufs);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                Log(LogLevel::Error, "FluidRenderer: thickness smooth FBO incomplete");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FluidRenderer::DestroyFBOs()
    {
        if (m_depth_tex)         { glDeleteTextures(1, &m_depth_tex);         m_depth_tex = 0; }
        if (m_thickness_tex)     { glDeleteTextures(1, &m_thickness_tex);     m_thickness_tex = 0; }
        if (m_depth_renderbuffer){ glDeleteRenderbuffers(1, &m_depth_renderbuffer); m_depth_renderbuffer = 0; }
        if (m_depth_fbo)         { glDeleteFramebuffers(1, &m_depth_fbo);     m_depth_fbo = 0; }
        for (int i = 0; i < 2; ++i) {
            if (m_smooth_tex[i]) { glDeleteTextures(1, &m_smooth_tex[i]);     m_smooth_tex[i] = 0; }
            if (m_smooth_fbo[i]) { glDeleteFramebuffers(1, &m_smooth_fbo[i]); m_smooth_fbo[i] = 0; }
            if (m_thick_smooth_tex[i]) { glDeleteTextures(1, &m_thick_smooth_tex[i]);     m_thick_smooth_tex[i] = 0; }
            if (m_thick_smooth_fbo[i]) { glDeleteFramebuffers(1, &m_thick_smooth_fbo[i]); m_thick_smooth_fbo[i] = 0; }
        }
    }

    void FluidRenderer::Shutdown()
    {
        DestroyFBOs();
        if (m_particle_ebo)  { glDeleteBuffers(1, &m_particle_ebo);  m_particle_ebo = 0; }
        if (m_particle_quad) { glDeleteBuffers(1, &m_particle_quad); m_particle_quad = 0; }
        if (m_particle_vao)  { glDeleteVertexArrays(1, &m_particle_vao); m_particle_vao = 0; }
        if (m_quad_vbo)      { glDeleteBuffers(1, &m_quad_vbo);      m_quad_vbo = 0; }
        if (m_quad_vao)      { glDeleteVertexArrays(1, &m_quad_vao); m_quad_vao = 0; }
    }

    void FluidRenderer::Resize(int width, int height)
    {
        if (width <= 0 || height <= 0) return;
        // Full resolution for maximum visual quality. 
        if (width == m_width && height == m_height) return;
        CreateFBOs(width, height);
    }

    void FluidRenderer::DrawFullScreen()
    {
        glBindVertexArray(m_quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void FluidRenderer::Execute(GLuint particle_pos_vbo, GLuint particle_flags_vbo,
                                uint32_t particle_count, float particle_radius,
                                const glm::mat4& view, const glm::mat4& proj,
                                const glm::vec3& light_dir_world, const glm::vec3& light_color,
                                GLuint scene_color_tex, GLuint scene_depth_tex,
                                int scene_w, int scene_h, GLuint scene_fbo,
                                float time)
    {
        if (particle_count == 0 || !m_depth_fbo) return;

        // ===========================================================================================
        // 1. Depth + thickness pass.
        // ===========================================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, m_depth_fbo);
        glViewport(0, 0, m_width, m_height);

        glDisable(GL_BLEND);
        // Per-attachment clear: depth target needs sentinel +1.0 (any negative value means "fluid
        // is here"), thickness needs 0.0 because it's additively accumulated.
        const float depth_clear[4]     = { 1.0f, 0.0f, 0.0f, 0.0f };
        const float thickness_clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, depth_clear);
        glClearBufferfv(GL_COLOR, 1, thickness_clear);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        glEnablei(GL_BLEND, 1);
        glBlendFunci(1, GL_ONE, GL_ONE);
        glDisablei(GL_BLEND, 0);

        m_depth_shader->Bind();
        m_depth_shader->SetUniformMat4("u_view", view);
        m_depth_shader->SetUniformMat4("u_proj", proj);
        // Set base render radius to 1.5x for solid surface fusion.
        m_depth_shader->SetUniform1f("u_radius", particle_radius * 1.5f);

        glBindVertexArray(m_particle_vao);
        glBindBuffer(GL_ARRAY_BUFFER, particle_pos_vbo);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glVertexAttribDivisor(1, 1);
        glBindBuffer(GL_ARRAY_BUFFER, particle_flags_vbo);
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
        glVertexAttribDivisor(2, 1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_particle_ebo);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, particle_count);
        glBindVertexArray(0);

        glDisablei(GL_BLEND, 1);

        // ===========================================================================================
        // 2. Bilateral smoothing — separable H/V on view-space depth.
        // ===========================================================================================
        glDisable(GL_DEPTH_TEST);

        GLuint src = m_depth_tex;
        for (int i = 0; i < m_smooth_iters; ++i) {
            // Horizontal pass: src -> smooth[0]
            glBindFramebuffer(GL_FRAMEBUFFER, m_smooth_fbo[0]);
            glViewport(0, 0, m_width, m_height);
            glClear(GL_COLOR_BUFFER_BIT);
            m_smooth_shader->Bind();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src);
            m_smooth_shader->SetUniform1i("u_depth", 0);
            m_smooth_shader->SetUniform2f("u_dir", 1.f, 0.f);
            m_smooth_shader->SetUniform2f("u_texel", 1.f/float(m_width), 1.f/float(m_height));
            m_smooth_shader->SetUniform1f("u_depth_sigma", m_smooth_depth_sigma);
            DrawFullScreen();

            // Vertical pass: smooth[0] -> smooth[1]
            glBindFramebuffer(GL_FRAMEBUFFER, m_smooth_fbo[1]);
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_smooth_tex[0]);
            m_smooth_shader->SetUniform2f("u_dir", 0.f, 1.f);
            DrawFullScreen();

            src = m_smooth_tex[1];
        }

        // ===========================================================================================
        // 2b. Thickness smoothing — separable box blur. Kills per-splat ridges in the absorption
        //     term and the foam alpha. Cheap, ~14 taps.
        // ===========================================================================================
        GLuint thick_src = m_thickness_tex;
        for (int i = 0; i < m_thickness_smooth_iters; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_thick_smooth_fbo[0]);
            glViewport(0, 0, m_width, m_height);
            glClear(GL_COLOR_BUFFER_BIT);
            m_thick_smooth_shader->Bind();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, thick_src);
            m_thick_smooth_shader->SetUniform1i("u_thickness", 0);
            m_thick_smooth_shader->SetUniform2f("u_dir", 1.f, 0.f);
            m_thick_smooth_shader->SetUniform2f("u_texel", 1.f/float(m_width), 1.f/float(m_height));
            DrawFullScreen();

            glBindFramebuffer(GL_FRAMEBUFFER, m_thick_smooth_fbo[1]);
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_thick_smooth_tex[0]);
            m_thick_smooth_shader->SetUniform2f("u_dir", 0.f, 1.f);
            DrawFullScreen();

            thick_src = m_thick_smooth_tex[1];
        }

        // ===========================================================================================
        // 3. Compose: SSR + refraction + caustics + SSS into the HDR scene buffer.
        // ===========================================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
        glViewport(0, 0, scene_w, scene_h);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);              // also write fluid depth so later passes see the surface
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_compose_shader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src);
        m_compose_shader->SetUniform1i("u_fluid_depth", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, thick_src);
        m_compose_shader->SetUniform1i("u_fluid_thickness", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, scene_color_tex);
        m_compose_shader->SetUniform1i("u_scene_color", 2);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, scene_depth_tex);
        m_compose_shader->SetUniform1i("u_scene_depth", 3);

        m_compose_shader->SetUniformMat4("u_proj",     proj);
        m_compose_shader->SetUniformMat4("u_inv_proj", glm::inverse(proj));
        m_compose_shader->SetUniformMat4("u_inv_view", glm::inverse(view));
        // Sun in view space.
        glm::vec3 view_light = glm::normalize(glm::mat3(view) * (-light_dir_world));
        m_compose_shader->SetUniformVec3("u_view_light",  view_light);
        m_compose_shader->SetUniformVec3("u_world_light", -glm::normalize(light_dir_world));
        m_compose_shader->SetUniformVec3("u_light_color", light_color);
        m_compose_shader->SetUniform1f("u_thickness_scale", m_thickness_scale);
        m_compose_shader->SetUniform1f("u_fresnel_f0",    m_fresnel_f0);
        m_compose_shader->SetUniform1f("u_refract_strength", m_refraction_strength);
        m_compose_shader->SetUniform1f("u_caustic_strength", m_caustic_strength);
        m_compose_shader->SetUniform1i("u_enable_ssr",     m_enable_ssr ? 1 : 0);
        m_compose_shader->SetUniform1i("u_ssr_steps",      m_ssr_steps);
        m_compose_shader->SetUniform1f("u_ssr_max_distance", m_ssr_max_distance);
        m_compose_shader->SetUniformVec3("u_water_tint",    glm::vec3(m_water_tint));
        m_compose_shader->SetUniformVec3("u_water_scatter", m_water_scatter);
        m_compose_shader->SetUniform2f("u_screen_size", float(scene_w), float(scene_h));
        m_compose_shader->SetUniform1f("u_time", time);
        DrawFullScreen();

        // ===========================================================================================
        // 4. Foam sprite pass — additive, picks up per-particle "foam" flag (bit 3).
        // ===========================================================================================
        if (m_foam_shader) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // additive
            glDepthMask(GL_FALSE);               // foam doesn't occlude
            m_foam_shader->Bind();
            m_foam_shader->SetUniformMat4("u_view", view);
            m_foam_shader->SetUniformMat4("u_proj", proj);
            m_foam_shader->SetUniform1f("u_radius", particle_radius * m_foam_size_scale);
            m_foam_shader->SetUniform1f("u_alpha",  m_foam_alpha);

            glBindVertexArray(m_particle_vao);
            glBindBuffer(GL_ARRAY_BUFFER, particle_pos_vbo);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glBindBuffer(GL_ARRAY_BUFFER, particle_flags_vbo);
            glEnableVertexAttribArray(2);
            glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
            glVertexAttribDivisor(2, 1);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_particle_ebo);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, particle_count);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}
