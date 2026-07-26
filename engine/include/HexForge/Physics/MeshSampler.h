#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace Hex
{
    // Output of a voxelization pass — one entry per particle that should be spawned for the body.
    //
    // world_position   — position in world space at spawn time (after applying world_transform)
    // local_rest       — same position expressed in the body's local frame around its centroid,
    //                    suitable for shape-matching (Müller et al. 2005).
    struct SampledRigidParticle
    {
        glm::vec3 world_position;
        glm::vec3 local_rest;
    };

    struct SampledRigidBody
    {
        std::vector<SampledRigidParticle> particles;
        glm::vec3 centroid_world{0.0f};     // mean of world_position
        glm::vec3 aabb_min_world{0.0f};
        glm::vec3 aabb_max_world{0.0f};
    };

    // Mesh → particle sampler. Two input modes:
    //   - Raw vertex/index arrays  (you own the data; everything stays on the CPU here).
    //   - File path                (Assimp loads the triangles; we then voxelize).
    //
    // Algorithm:
    //   1. Apply world_transform to the triangles.
    //   2. Compute AABB of the transformed mesh.
    //   3. Build a regular grid at `spacing` and test each cell center for inside-mesh by ray
    //      casting along +x. Odd intersection count → inside (Möller–Trumbore intersection).
    //   4. For each inside cell, emit a particle.
    //
    // Notes:
    //   - `spacing` should equal the close-packed sampling distance, i.e. 2 * particle_radius.
    //   - For non-watertight meshes the inside test will be unreliable. Use solid, manifold
    //     geometry. (For thin shells, a surface-sampling routine is the right approach instead.)
    //   - O(grid_cells * triangles); fine for hundreds of triangles. For dense meshes add a BVH.
    namespace MeshSampler
    {
        SampledRigidBody Voxelize(const std::vector<glm::vec3>& vertices_world,
                                  const std::vector<uint32_t>& indices,
                                  float spacing);

        SampledRigidBody Voxelize(const std::vector<glm::vec3>& vertices_local,
                                  const std::vector<uint32_t>& indices,
                                  const glm::mat4& world_transform,
                                  float spacing);

        // Convenience: load via Assimp and voxelize. Returns an empty body on failure.
        SampledRigidBody VoxelizeFromFile(const std::string& mesh_path,
                                          const glm::mat4& world_transform,
                                          float spacing);
    }
}
