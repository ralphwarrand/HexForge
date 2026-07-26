#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Hex
{
    // Trivially copyable view of the grid suitable for passing by value into a kernel.
    // (Passing the owning CudaSpatialGrid* into a kernel would dereference host memory
    // on the device — that's why the previous iteration of this code never worked.)
    struct CudaSpatialGridView
    {
        uint32_t num_particles = 0;
        float    inv_cell_size = 0.0f;
        uint32_t grid_size     = 0;
        const uint32_t* sorted_indices = nullptr;
        const uint32_t* cell_starts    = nullptr;
        const uint32_t* cell_ends      = nullptr;
    };

    // Uniform-grid spatial hash for fixed-radius neighbor queries.
    //
    // After Build():
    //   d_sorted_indices[k] = original particle index sitting at sorted slot k.
    //   d_cell_starts[h] / d_cell_ends[h] = [start, end) range of sorted slots in cell h
    //     (0xFFFFFFFF in d_cell_starts means the cell is empty).
    //
    // A device-side kernel iterates the 27 neighbouring cells of a particle's home cell,
    // dereferences the sorted-index table to fetch original particle data, and filters by
    // distance squared (hash collisions can place unrelated cells in the same bucket).
    class CudaSpatialGrid
    {
    public:
#if HEX_ENABLE_CUDA
        CudaSpatialGrid();
        ~CudaSpatialGrid();

        void Init(uint32_t max_particles, float cell_size);
        void Shutdown();

        void Build(const glm::vec3* d_predicted_positions, uint32_t num_particles);
#else
        CudaSpatialGrid() = default;
        ~CudaSpatialGrid() = default;

        void Init(uint32_t max_particles, float cell_size) { (void)max_particles; (void)cell_size; }
        void Shutdown() {}

        void Build(const glm::vec3* d_predicted_positions, uint32_t num_particles) { (void)d_predicted_positions; (void)num_particles; }
#endif

        // Snapshot the current build state as a POD view that kernels can take by value.
        CudaSpatialGridView View() const
        {
            return CudaSpatialGridView{
                m_num_particles, m_inv_cell_size, m_grid_size,
                m_active_indices, d_cell_starts, d_cell_ends
            };
        }

        // Public for direct device access through a CudaSpatialGrid* (pass by value into kernels).
        uint32_t m_max_particles = 0;
        uint32_t m_num_particles = 0;
        float m_cell_size = 0.0f;
        float m_inv_cell_size = 0.0f;
        uint32_t m_grid_size = 0;

        uint32_t* d_cell_hashes = nullptr;     // [N] hash for each particle
        uint32_t* d_cell_hashes_alt = nullptr; // [N] ping-pong buffer for CUB sort
        uint32_t* d_sorted_indices = nullptr;  // [N] original index at sorted slot k
        uint32_t* d_sorted_indices_alt = nullptr; // [N] ping-pong buffer for CUB sort
        
        // Active sorted pointers (set each Build)
        uint32_t* m_active_indices = nullptr;
        uint32_t* m_active_hashes  = nullptr;

        void*  d_temp_storage = nullptr;
        size_t temp_storage_bytes = 0;
        
        uint32_t* d_cell_starts = nullptr;     // [grid_size]
        uint32_t* d_cell_ends = nullptr;       // [grid_size]
    };
}
