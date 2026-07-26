#pragma once

#include <cstdint>
#include <cuda_runtime.h>
#include <glm/glm.hpp>

namespace Hex::GridHash
{
    __host__ __device__ inline uint32_t ExpandBits(uint32_t v)
    {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    }

    // Morton code preserves spatial locality mapping 3D coordinates to a 1D index.
    __host__ __device__ inline uint32_t CellHash(int ix, int iy, int iz, uint32_t grid_size)
    {
        // Offset negative coords assuming typical simulation domain centers around 0.
        uint32_t x = ExpandBits(static_cast<uint32_t>(ix + 1024) & 0x3FF);
        uint32_t y = ExpandBits(static_cast<uint32_t>(iy + 1024) & 0x3FF);
        uint32_t z = ExpandBits(static_cast<uint32_t>(iz + 1024) & 0x3FF);
        uint32_t morton = (z << 2) | (y << 1) | x;
        return morton % grid_size;
    }

    __host__ __device__ inline glm::ivec3 PositionToCell(const glm::vec3& pos, float inv_cell_size)
    {
        return glm::ivec3(
            static_cast<int>(floorf(pos.x * inv_cell_size)),
            static_cast<int>(floorf(pos.y * inv_cell_size)),
            static_cast<int>(floorf(pos.z * inv_cell_size)));
    }
}
