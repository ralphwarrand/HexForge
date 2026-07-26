#pragma once

#include <cuda_runtime.h>
#include <glm/glm.hpp>

namespace Hex::KernelMath
{
    // Mueller "Particle-Based Fluid Simulation for Interactive Applications" (2003) kernels.
    // All coefficients are precomputed against h once per call — h is the smoothing radius.

    __device__ inline float Poly6(const glm::vec3& r, float h)
    {
        const float h2 = h * h;
        const float r2 = dot(r, r);
        if (r2 >= h2) return 0.0f;

        const float h9   = h * h * h * h * h * h * h * h * h;
        const float coeff = 315.0f / (64.0f * 3.14159265358979323846f * h9);
        const float diff  = h2 - r2;
        return coeff * diff * diff * diff;
    }

    __device__ inline glm::vec3 SpikyGradient(const glm::vec3& r, float h)
    {
        const float dist = length(r);
        if (dist >= h || dist < 1e-6f) return glm::vec3(0.0f);

        const float h6    = h * h * h * h * h * h;
        const float coeff = -45.0f / (3.14159265358979323846f * h6);
        const float term  = h - dist;
        return (r / dist) * (coeff * term * term);
    }
}
