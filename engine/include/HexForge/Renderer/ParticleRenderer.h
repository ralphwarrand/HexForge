#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Hex
{
    class PhysicsSystem;
    class Shader;
    struct PhysicsMaterial;

    // Instanced point-sphere renderer. Reads the particle position VBO that PhysicsSystem
    // updates each step via OpenGL/CUDA interop, plus a per-particle material-id VBO and a
    // small materials UBO for color lookup. One draw call regardless of particle count.
    //
    // The geometry is "screen-space spheres" — we render a single quad per particle billboarded
    // toward the camera and shade a sphere impostor in the fragment shader. This avoids needing
    // a real sphere mesh and is dramatically faster than instanced triangle meshes for fluids.
    class ParticleRenderer
    {
    public:
        ParticleRenderer();
        ~ParticleRenderer();

        ParticleRenderer(const ParticleRenderer&) = delete;
        ParticleRenderer& operator=(const ParticleRenderer&) = delete;

        void Init();
        void Shutdown();

        // Uploads the host-side material table to the GL UBO. Call once after PhysicsSystem::Init.
        void UpdateMaterials(const std::vector<PhysicsMaterial>& materials);

        // Issues a single instanced draw covering `particle_count` quads. Two cull flags let
        // the caller skip categories that a different pipeline is drawing:
        //   hide_rigid — rigid body members are drawn as their source mesh.
        //   hide_fluid — fluid particles get screen-space fluid surface (SSFR) shading.
        void Render(GLuint position_vbo, GLuint material_id_vbo, GLuint flags_vbo,
                    uint32_t particle_count,
                    const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& view_pos, const glm::vec3& light_dir,
                    float particle_radius,
                    bool hide_rigid, bool hide_fluid, bool hide_cloth);

    private:
        GLuint m_vao        = 0;
        GLuint m_quad_vbo   = 0;    // 4 vertices of a unit quad
        GLuint m_quad_ebo   = 0;
        GLuint m_materials_ubo = 0;
        std::shared_ptr<Shader> m_shader;

        static constexpr int kMaxMaterials = 32;
    };
}
