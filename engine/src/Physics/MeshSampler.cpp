#include "HexForge/pch.h"
#include "HexForge/Physics/MeshSampler.h"
#include "HexForge/Core/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <format>

namespace Hex::MeshSampler
{
    namespace
    {
        // Möller-Trumbore ray-triangle intersection.
        //   ray:  o + t * d, t > eps
        // Returns true and writes t on hit.
        bool RayTriangle(const glm::vec3& o, const glm::vec3& d,
                         const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                         float& t_out)
        {
            constexpr float kEps = 1e-8f;
            glm::vec3 e1 = v1 - v0;
            glm::vec3 e2 = v2 - v0;
            glm::vec3 p  = glm::cross(d, e2);
            float det    = glm::dot(e1, p);
            if (std::fabs(det) < kEps) return false;
            float inv_det = 1.0f / det;

            glm::vec3 tvec = o - v0;
            float u = glm::dot(tvec, p) * inv_det;
            if (u < 0.0f || u > 1.0f) return false;

            glm::vec3 q = glm::cross(tvec, e1);
            float v = glm::dot(d, q) * inv_det;
            if (v < 0.0f || (u + v) > 1.0f) return false;

            float t = glm::dot(e2, q) * inv_det;
            if (t < kEps) return false;
            t_out = t;
            return true;
        }

        // Cheap projection-aligned AABB per triangle. The inside-mesh test casts a ray in +x,
        // so any triangle whose (y, z) AABB doesn't contain the ray's (y, z) origin can be
        // skipped without computing Möller-Trumbore. With a few-thousand-triangle bunny this
        // turns ~11 000 tests/cell into ~50.
        struct TriYZ {
            float y_min, y_max;
            float z_min, z_max;
            float x_max; // triangles entirely behind the ray origin can't be in front
            uint32_t a, b, c;
        };

        std::vector<TriYZ> BuildTriYZ(const std::vector<glm::vec3>& verts,
                                      const std::vector<uint32_t>& idx)
        {
            const size_t tri_count = idx.size() / 3;
            std::vector<TriYZ> out;
            out.reserve(tri_count);
            for (size_t t = 0; t < tri_count; ++t) {
                uint32_t a = idx[3*t + 0], b = idx[3*t + 1], c = idx[3*t + 2];
                const glm::vec3& va = verts[a];
                const glm::vec3& vb = verts[b];
                const glm::vec3& vc = verts[c];
                TriYZ aabb;
                aabb.y_min = std::min({ va.y, vb.y, vc.y });
                aabb.y_max = std::max({ va.y, vb.y, vc.y });
                aabb.z_min = std::min({ va.z, vb.z, vc.z });
                aabb.z_max = std::max({ va.z, vb.z, vc.z });
                aabb.x_max = std::max({ va.x, vb.x, vc.x });
                aabb.a = a; aabb.b = b; aabb.c = c;
                out.push_back(aabb);
            }
            return out;
        }

        bool IsInsideMesh(const glm::vec3& p,
                          const std::vector<glm::vec3>& verts,
                          const std::vector<TriYZ>& tris)
        {
            const glm::vec3 dir(1.0f, 0.0f, 0.0f);
            int hits = 0;
            for (const auto& tri : tris) {
                if (p.y < tri.y_min || p.y > tri.y_max) continue;
                if (p.z < tri.z_min || p.z > tri.z_max) continue;
                if (tri.x_max < p.x) continue;
                float tmp_t;
                if (RayTriangle(p, dir, verts[tri.a], verts[tri.b], verts[tri.c], tmp_t)) ++hits;
            }
            return (hits & 1) != 0;
        }

        SampledRigidBody Sample(const std::vector<glm::vec3>& verts_world,
                                const std::vector<uint32_t>& idx,
                                float spacing)
        {
            SampledRigidBody out;
            if (verts_world.empty() || idx.size() < 3 || spacing <= 0.0f) return out;

            // 1. World AABB
            glm::vec3 mn(std::numeric_limits<float>::max());
            glm::vec3 mx(-std::numeric_limits<float>::max());
            for (auto& v : verts_world) {
                mn = glm::min(mn, v);
                mx = glm::max(mx, v);
            }
            out.aabb_min_world = mn;
            out.aabb_max_world = mx;

            // 2. Precompute the y/z-AABB-augmented triangle table once. This is the only piece
            //    of the inside-mesh test that depends on triangles — by doing it before the
            //    O(grid) sampling loop, every cell pays only the constant pre-filter cost per
            //    triangle, not the full Möller-Trumbore.
            const auto tris = BuildTriYZ(verts_world, idx);

            glm::vec3 extent = mx - mn;
            float max_extent = std::max({ extent.x, extent.y, extent.z });
            float s = spacing;
            if (max_extent > 0.0f) {
                s = std::max(s, max_extent / 6.0f);
            }
            const float half = 0.5f * s;
            glm::ivec3 gmin = glm::ivec3(std::floor((mn.x) / s),
                                         std::floor((mn.y) / s),
                                         std::floor((mn.z) / s));
            glm::ivec3 gmax = glm::ivec3(std::ceil ((mx.x) / s),
                                         std::ceil ((mx.y) / s),
                                         std::ceil ((mx.z) / s));

            // 3. Sample each cell centre, accept if inside mesh.
            std::vector<glm::vec3> world_positions;
            world_positions.reserve(64);
            for (int ix = gmin.x; ix <= gmax.x; ++ix)
            for (int iy = gmin.y; iy <= gmax.y; ++iy)
            for (int iz = gmin.z; iz <= gmax.z; ++iz) {
                glm::vec3 c(ix * s + half, iy * s + half, iz * s + half);
                if (c.x < mn.x || c.x > mx.x ||
                    c.y < mn.y || c.y > mx.y ||
                    c.z < mn.z || c.z > mx.z) continue;
                if (IsInsideMesh(c, verts_world, tris)) {
                    world_positions.push_back(c);
                }
            }

            // 4. Centroid + local rest positions.
            if (world_positions.empty()) return out;
            glm::vec3 centroid(0.0f);
            for (auto& p : world_positions) centroid += p;
            centroid /= float(world_positions.size());
            out.centroid_world = centroid;

            out.particles.reserve(world_positions.size());
            for (auto& p : world_positions) {
                out.particles.push_back({ p, p - centroid });
            }

            if (out.particles.size() > 300) {
                size_t stride = (out.particles.size() + 299) / 300;
                std::vector<SampledRigidParticle> culled;
                culled.reserve(300);
                for (size_t i = 0; i < out.particles.size(); i += stride) {
                    culled.push_back(out.particles[i]);
                }
                out.particles = std::move(culled);
            }

            return out;
        }
    }

    SampledRigidBody Voxelize(const std::vector<glm::vec3>& vertices_world,
                              const std::vector<uint32_t>& indices,
                              float spacing)
    {
        return Sample(vertices_world, indices, spacing);
    }

    SampledRigidBody Voxelize(const std::vector<glm::vec3>& vertices_local,
                              const std::vector<uint32_t>& indices,
                              const glm::mat4& world_transform,
                              float spacing)
    {
        std::vector<glm::vec3> world;
        world.reserve(vertices_local.size());
        for (auto& v : vertices_local) {
            world.push_back(glm::vec3(world_transform * glm::vec4(v, 1.0f)));
        }
        return Sample(world, indices, spacing);
    }

    SampledRigidBody VoxelizeFromFile(const std::string& mesh_path,
                                      const glm::mat4& world_transform,
                                      float spacing)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            mesh_path,
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices);
        if (!scene || !scene->HasMeshes()) {
            Log(LogLevel::Error, std::format("MeshSampler: failed to load '{}'", mesh_path));
            return {};
        }

        // Flatten all submeshes into one (verts, indices) pair.
        std::vector<glm::vec3> verts;
        std::vector<uint32_t>  idx;
        for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) {
            const aiMesh* m = scene->mMeshes[mi];
            const uint32_t base = static_cast<uint32_t>(verts.size());
            verts.reserve(verts.size() + m->mNumVertices);
            for (unsigned v = 0; v < m->mNumVertices; ++v) {
                const aiVector3D& p = m->mVertices[v];
                verts.emplace_back(p.x, p.y, p.z);
            }
            for (unsigned f = 0; f < m->mNumFaces; ++f) {
                const aiFace& face = m->mFaces[f];
                if (face.mNumIndices != 3) continue;
                idx.push_back(base + face.mIndices[0]);
                idx.push_back(base + face.mIndices[1]);
                idx.push_back(base + face.mIndices[2]);
            }
        }

        // Apply world transform.
        for (auto& v : verts) v = glm::vec3(world_transform * glm::vec4(v, 1.0f));

        auto body = Sample(verts, idx, spacing);
        Log(LogLevel::Info, std::format(
            "MeshSampler '{}': {} triangles -> {} particles",
            mesh_path, idx.size() / 3, body.particles.size()));
        return body;
    }
}
