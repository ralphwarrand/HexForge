#include "HexForge/Cuda/CudaSpatialGrid.h"
#include "HexForge/Cuda/GridHash.cuh"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include <cub/device/device_radix_sort.cuh>

namespace Hex
{

namespace
{
    constexpr uint32_t kEmptyCell = 0xFFFFFFFFu;
    constexpr int kBlockSize = 256;
}

__global__ void calculateHashes_kernel(
    const glm::vec3* positions,
    uint32_t* hashes,
    uint32_t* indices,
    uint32_t num_particles,
    float inv_cell_size,
    uint32_t grid_size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_particles) return;

    glm::ivec3 cell = GridHash::PositionToCell(positions[idx], inv_cell_size);
    hashes[idx]  = GridHash::CellHash(cell.x, cell.y, cell.z, grid_size);
    indices[idx] = idx;
}

__global__ void resetCells_kernel(uint32_t* cell_starts, uint32_t* cell_ends, uint32_t grid_size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= grid_size) return;
    cell_starts[idx] = kEmptyCell;
    cell_ends[idx]   = kEmptyCell;
}

__global__ void findCellStartsAndEnds_kernel(
    const uint32_t* sorted_hashes,
    uint32_t* cell_starts,
    uint32_t* cell_ends,
    uint32_t num_particles)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_particles) return;

    const uint32_t h = sorted_hashes[idx];

    if (idx == 0 || sorted_hashes[idx - 1] != h) {
        cell_starts[h] = idx;
        if (idx > 0) {
            cell_ends[sorted_hashes[idx - 1]] = idx;
        }
    }
    if (idx == num_particles - 1) {
        cell_ends[h] = num_particles;
    }
}

CudaSpatialGrid::CudaSpatialGrid() = default;

CudaSpatialGrid::~CudaSpatialGrid()
{
    Shutdown();
}

void CudaSpatialGrid::Init(uint32_t max_particles, float cell_size)
{
    Shutdown();

    m_max_particles  = max_particles;
    m_num_particles  = 0;
    m_cell_size      = cell_size;
    m_inv_cell_size  = (cell_size > 0.0f) ? (1.0f / cell_size) : 0.0f;
    // SOTA: Use a larger grid multiplier (8x) to minimize hash collisions.
    // This ensures that neighbor query performance remains consistent even in dense fluid.
    m_grid_size      = (max_particles > 0) ? (max_particles * 8u) : 1u;

    cudaMalloc(&d_cell_hashes,        m_max_particles * sizeof(uint32_t));
    cudaMalloc(&d_cell_hashes_alt,    m_max_particles * sizeof(uint32_t));
    cudaMalloc(&d_sorted_indices,     m_max_particles * sizeof(uint32_t));
    cudaMalloc(&d_sorted_indices_alt, m_max_particles * sizeof(uint32_t));
    cudaMalloc(&d_cell_starts,        m_grid_size     * sizeof(uint32_t));
    cudaMalloc(&d_cell_ends,          m_grid_size     * sizeof(uint32_t));

    // Determine temporary device storage requirements for CUB RadixSort
    cub::DoubleBuffer<uint32_t> d_keys(d_cell_hashes, d_cell_hashes_alt);
    cub::DoubleBuffer<uint32_t> d_vals(d_sorted_indices, d_sorted_indices_alt);
    cub::DeviceRadixSort::SortPairs(nullptr, temp_storage_bytes, d_keys, d_vals, m_max_particles);
    cudaMalloc(&d_temp_storage, temp_storage_bytes);
}

void CudaSpatialGrid::Shutdown()
{
    if (d_cell_hashes)        cudaFree(d_cell_hashes);
    if (d_cell_hashes_alt)    cudaFree(d_cell_hashes_alt);
    if (d_sorted_indices)     cudaFree(d_sorted_indices);
    if (d_sorted_indices_alt) cudaFree(d_sorted_indices_alt);
    if (d_cell_starts)        cudaFree(d_cell_starts);
    if (d_cell_ends)          cudaFree(d_cell_ends);
    if (d_temp_storage)       cudaFree(d_temp_storage);

    d_cell_hashes = nullptr;
    d_cell_hashes_alt = nullptr;
    d_sorted_indices = nullptr;
    d_sorted_indices_alt = nullptr;
    d_cell_starts = nullptr;
    d_cell_ends = nullptr;
    d_temp_storage = nullptr;

    m_max_particles = 0;
    m_num_particles = 0;
    m_grid_size = 0;
}

void CudaSpatialGrid::Build(const glm::vec3* d_predicted_positions, uint32_t num_particles)
{
    m_num_particles = num_particles;
    if (num_particles == 0 || m_grid_size == 0) return;

    const int particle_blocks = (num_particles + kBlockSize - 1) / kBlockSize;
    const int grid_blocks     = (m_grid_size + kBlockSize - 1) / kBlockSize;

    resetCells_kernel<<<grid_blocks, kBlockSize>>>(d_cell_starts, d_cell_ends, m_grid_size);

    calculateHashes_kernel<<<particle_blocks, kBlockSize>>>(
        d_predicted_positions, d_cell_hashes, d_sorted_indices,
        num_particles, m_inv_cell_size, m_grid_size);

    // CUB Radix Sort. Only sort the active bits (since hashes are modulo m_grid_size).
    int num_bits = 0;
    uint32_t max_val = m_grid_size - 1;
    while (max_val > 0) {
        num_bits++;
        max_val >>= 1;
    }

    cub::DoubleBuffer<uint32_t> d_keys(d_cell_hashes, d_cell_hashes_alt);
    cub::DoubleBuffer<uint32_t> d_vals(d_sorted_indices, d_sorted_indices_alt);
    
    cub::DeviceRadixSort::SortPairs(
        d_temp_storage, temp_storage_bytes,
        d_keys, d_vals, num_particles,
        0, num_bits);

    // Use a persistent 'current' pointer instead of overwriting the base buffers.
    // Overwriting d_cell_hashes/d_sorted_indices makes the next DoubleBuffer 
    // initialization use the same pointer for both buffers!
    m_active_indices = d_vals.Current();
    m_active_hashes  = d_keys.Current();

    findCellStartsAndEnds_kernel<<<particle_blocks, kBlockSize>>>(
        m_active_hashes, d_cell_starts, d_cell_ends, num_particles);
}

} // namespace Hex
