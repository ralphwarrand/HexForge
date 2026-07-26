#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Hex
{
    enum class ParticlePhase : uint32_t
    {
        Static = 0,
        Fluid  = 1,
        Solid  = 2,
        Cloth  = 3, // distance/bending-constrained sheet; does not self-collide as spheres
        Rigid  = 4, // shape-matched cluster; contributes to fluid density as boundary
    };

    // POD — uploaded to the GPU as-is. Field order is shared between host and device.
    // 16-byte-aligned color sits last so the entire struct stays naturally aligned to vec4.
    struct PhysicsMaterial
    {
        float    rest_density = 1000.0f;
        float    viscosity    = 0.05f;
        float    friction     = 0.30f;
        float    restitution  = 0.10f;
        float    compliance   = 1e-6f;
        uint32_t phase        = static_cast<uint32_t>(ParticlePhase::Fluid);
        float    _pad0        = 0.0f;
        float    _pad1        = 0.0f;
        glm::vec4 color       = { 0.55f, 0.75f, 1.0f, 1.0f }; // sRGB-ish, alpha unused in the lit shader
    };

    namespace Materials
    {
        inline PhysicsMaterial Water()
        {
            PhysicsMaterial m{ 1000.0f, 0.25f, 0.0f, 0.05f, 1e-6f,
                               static_cast<uint32_t>(ParticlePhase::Fluid), 0, 0,
                               { 0.20f, 0.55f, 0.95f, 1.0f } };
            return m;
        }
        inline PhysicsMaterial Oil()
        {
            PhysicsMaterial m{ 850.0f, 0.20f, 0.0f, 0.02f, 1e-6f,
                               static_cast<uint32_t>(ParticlePhase::Fluid), 0, 0,
                               { 0.45f, 0.30f, 0.10f, 1.0f } };
            return m;
        }
        inline PhysicsMaterial Sand()
        {
            PhysicsMaterial m{ 1600.0f, 0.02f, 0.55f, 0.0f, 1e-5f,
                               static_cast<uint32_t>(ParticlePhase::Solid), 0, 0,
                               { 0.92f, 0.80f, 0.50f, 1.0f } };
            return m;
        }
        inline PhysicsMaterial Rubber()
        {
            PhysicsMaterial m{ 1200.0f, 0.0f, 0.6f, 0.7f, 1e-4f,
                               static_cast<uint32_t>(ParticlePhase::Solid), 0, 0,
                               { 0.85f, 0.20f, 0.20f, 1.0f } };
            return m;
        }
        inline PhysicsMaterial Static()
        {
            PhysicsMaterial m{ 0.0f, 0.0f, 0.6f, 0.0f, 0.0f,
                               static_cast<uint32_t>(ParticlePhase::Static), 0, 0,
                               { 0.4f, 0.4f, 0.4f, 1.0f } };
            return m;
        }
        inline PhysicsMaterial Rigid(const glm::vec4& color = { 0.7f, 0.9f, 1.0f, 1.0f })
        {
            // restitution low so a rigid landing doesn't pinball forever.
            PhysicsMaterial m{ 1500.0f, 0.0f, 0.45f, 0.05f, 0.0f,
                               static_cast<uint32_t>(ParticlePhase::Rigid), 0, 0,
                               color };
            return m;
        }
        inline PhysicsMaterial Cloth(const glm::vec4& color = { 0.85f, 0.30f, 0.55f, 1.0f })
        {
            PhysicsMaterial m{ 300.0f, 0.0f, 0.3f, 0.0f, 0.0f,
                               static_cast<uint32_t>(ParticlePhase::Cloth), 0, 0,
                               color };
            return m;
        }
    }
}
