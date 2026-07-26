#include "HexForge/pch.h"
#include "HexForge/Renderer/PostProcess.h"
#include "HexForge/Renderer/ShaderManager.h"
#include "HexForge/Core/Logger.h"

namespace Hex
{
    PostProcess::PostProcess() = default;
    PostProcess::~PostProcess() { Shutdown(); }

    void PostProcess::Init(int width, int height)
    {
        m_width = width;
        m_height = height;

        // Full-screen quad in NDC.
        const float verts[] = {
            -1.f, -1.f, 0.f, 0.f,
             1.f, -1.f, 1.f, 0.f,
             1.f,  1.f, 1.f, 1.f,
            -1.f, -1.f, 0.f, 0.f,
             1.f,  1.f, 1.f, 1.f,
            -1.f,  1.f, 0.f, 1.f,
        };
        glGenVertexArrays(1, &m_quad_vao);
        glBindVertexArray(m_quad_vao);
        glGenBuffers(1, &m_quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);

        CreateFBOs(width, height);

        m_extract_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert", RESOURCES_PATH "shaders/bloom_extract.frag");
        m_blur_shader    = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert", RESOURCES_PATH "shaders/bloom_blur.frag");
        m_compose_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/post.vert", RESOURCES_PATH "shaders/post_compose.frag");
    }

    void PostProcess::CreateFBOs(int w, int h)
    {
        DestroyFBOs();
        m_width = w; m_height = h;
        const int bw = (w > 1) ? (w / 2) : 1;
        const int bh = (h > 1) ? (h / 2) : 1;

        // Output: full res LDR.
        glGenFramebuffers(1, &m_output_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_output_fbo);
        glGenTextures(1, &m_output_texture);
        glBindTexture(GL_TEXTURE_2D, m_output_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_output_texture, 0);

        // Bloom ping-pong: half res HDR.
        for (int i = 0; i < 2; ++i) {
            glGenFramebuffers(1, &m_bloom_fbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo[i]);
            glGenTextures(1, &m_bloom_texture[i]);
            glBindTexture(GL_TEXTURE_2D, m_bloom_texture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bw, bh, 0, GL_RGB, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloom_texture[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                Log(LogLevel::Error, "PostProcess: bloom FBO incomplete");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void PostProcess::DestroyFBOs()
    {
        if (m_output_texture) { glDeleteTextures(1, &m_output_texture); m_output_texture = 0; }
        if (m_output_fbo)     { glDeleteFramebuffers(1, &m_output_fbo); m_output_fbo = 0; }
        for (int i = 0; i < 2; ++i) {
            if (m_bloom_texture[i]) { glDeleteTextures(1, &m_bloom_texture[i]); m_bloom_texture[i] = 0; }
            if (m_bloom_fbo[i])     { glDeleteFramebuffers(1, &m_bloom_fbo[i]); m_bloom_fbo[i] = 0; }
        }
    }

    void PostProcess::Shutdown()
    {
        DestroyFBOs();
        if (m_quad_vbo) { glDeleteBuffers(1, &m_quad_vbo); m_quad_vbo = 0; }
        if (m_quad_vao) { glDeleteVertexArrays(1, &m_quad_vao); m_quad_vao = 0; }
    }

    void PostProcess::Resize(int width, int height)
    {
        if (width <= 0 || height <= 0) return;
        if (width == m_width && height == m_height) return;
        CreateFBOs(width, height);
    }

    void PostProcess::DrawFullScreen()
    {
        glBindVertexArray(m_quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void PostProcess::Execute(GLuint hdr_scene_texture)
    {
        if (!m_output_fbo) return;
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        const int bw = (m_width > 1) ? (m_width / 2) : 1;
        const int bh = (m_height > 1) ? (m_height / 2) : 1;

        // 1. Bright extract: read HDR scene, write thresholded brightness to bloom[0].
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo[0]);
        glViewport(0, 0, bw, bh);
        glClear(GL_COLOR_BUFFER_BIT);
        m_extract_shader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdr_scene_texture);
        m_extract_shader->SetUniform1i("u_scene", 0);
        m_extract_shader->SetUniform1f("u_threshold", m_bloom_threshold);
        DrawFullScreen();

        // 2. Separable Gaussian — horizontal then vertical.
        m_blur_shader->Bind();
        m_blur_shader->SetUniform1i("u_image", 0);
        for (int axis = 0; axis < 2; ++axis) {
            int src = axis;       // 0 -> bloom[0] (extract); 1 -> bloom[1] (after H blur)
            int dst = 1 - axis;
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo[dst]);
            glViewport(0, 0, bw, bh);
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_bloom_texture[src]);
            m_blur_shader->SetUniform2f("u_dir", axis == 0 ? 1.f : 0.f, axis == 0 ? 0.f : 1.f);
            m_blur_shader->SetUniform2f("u_texel", 1.f / float(bw), 1.f / float(bh));
            DrawFullScreen();
        }

        // 3. Compose: scene + bloom -> ACES tonemap -> LDR output.
        glBindFramebuffer(GL_FRAMEBUFFER, m_output_fbo);
        glViewport(0, 0, m_width, m_height);
        glClear(GL_COLOR_BUFFER_BIT);
        m_compose_shader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdr_scene_texture);
        m_compose_shader->SetUniform1i("u_scene", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_bloom_texture[0]); // after V blur
        m_compose_shader->SetUniform1i("u_bloom", 1);
        m_compose_shader->SetUniform1f("u_bloom_intensity", m_bloom_intensity);
        m_compose_shader->SetUniform1f("u_exposure", m_exposure);
        DrawFullScreen();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
}
