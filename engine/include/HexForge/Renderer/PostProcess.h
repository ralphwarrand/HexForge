#pragma once

#include <glad/glad.h>
#include <memory>

namespace Hex
{
    class Shader;

    // HDR post-processing pipeline:
    //   bright extract -> separable Gaussian (H, V) -> ACES tonemap + bloom composite -> LDR.
    //
    // The Renderer owns the scene HDR framebuffer; PostProcess owns the bloom intermediates and
    // the final LDR output texture. UIManager reads `GetOutputTexture()` to display in the viewport.
    class PostProcess
    {
    public:
        PostProcess();
        ~PostProcess();

        PostProcess(const PostProcess&) = delete;
        PostProcess& operator=(const PostProcess&) = delete;

        void Init(int width, int height);
        void Shutdown();
        void Resize(int width, int height);

        // hdr_scene_texture: RGB16F texture from the scene framebuffer.
        // Renders into the post output FBO (LDR).
        void Execute(GLuint hdr_scene_texture);

        GLuint GetOutputTexture() const { return m_output_texture; }
        GLuint GetOutputFBO()     const { return m_output_fbo; }

        // Tuning (exposed for UI).
        // Threshold sits above the typical lit-surface luminance so only real highlights bloom.
        float m_bloom_threshold = 1.5f;
        float m_bloom_intensity = 0.35f;
        float m_exposure        = 1.0f;

    private:
        void CreateFBOs(int w, int h);
        void DestroyFBOs();
        void DrawFullScreen();

        int m_width = 0;
        int m_height = 0;

        // Full-screen quad shared by all post passes.
        GLuint m_quad_vao = 0;
        GLuint m_quad_vbo = 0;

        // Output (LDR, displayed in the UI viewport).
        GLuint m_output_fbo     = 0;
        GLuint m_output_texture = 0;

        // Bloom intermediates (half resolution, HDR).
        GLuint m_bloom_fbo[2]      = { 0, 0 };
        GLuint m_bloom_texture[2]  = { 0, 0 };

        std::shared_ptr<Shader> m_extract_shader;
        std::shared_ptr<Shader> m_blur_shader;
        std::shared_ptr<Shader> m_compose_shader;
    };
}
