#pragma once

#include <glm/glm.hpp>

namespace Hex::KernelMath
{
    // Poly6 Kernel - used for calculating density.
    inline float Poly6(const glm::vec3& r, float h)
    {
        const float h2 = h * h;
        float r2 = glm::length2(r);
        if (r2 < h2) {
            float term = h2 - r2;
            // 315 / (64 * PI * h^9)
            return 315.0f / (64.0f * 3.1415926535f * pow(h, 9.0f)) * term * term * term;
        }
        return 0.0f;
    }

    // Spiky Kernel Gradient - used for calculating pressure forces.
    inline glm::vec3 SpikyGradient(const glm::vec3& r, float h)
    {
        float r_len = glm::length(r);
        if (r_len > 1e-9f && r_len < h) {
            float term = h - r_len;
            // -45 / (PI * h^6)
            return -45.0f / (3.1415926535f * pow(h, 6.0f)) * term * term * (r / r_len);
        }
        return glm::vec3(0.0f);
    }
}