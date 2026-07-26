// Hex
#include "HexForge/pch.h"
#include "HexForge/Physics/PhysicsSystem.h"
#include "HexForge/Physics/StabilityLimits.h"
#include "HexForge/Gameplay/EntityManager.h"
#include "HexForge/Core/CudaManager.h"
#include "HexForge/Core/Logger.h"
#include "HexForge/Cuda/CudaSpatialGrid.h"

// STL
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <map>
#include <format>

// CUDA
#if HEX_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#endif

namespace Hex
{
    namespace
    {
        struct PhysicsMaterialKey
        {
            PhysicsMaterial value;
            bool operator==(const PhysicsMaterialKey& other) const
            {
                return std::memcmp(&value, &other.value, sizeof(PhysicsMaterial)) == 0;
            }
        };
        struct PhysicsMaterialKeyHasher
        {
            size_t operator()(const PhysicsMaterialKey& k) const noexcept
            {
                size_t h = 1469598103934665603ull;
                const auto* bytes = reinterpret_cast<const unsigned char*>(&k.value);
                for (size_t i = 0; i < sizeof(PhysicsMaterial); ++i) {
                    h ^= bytes[i];
                    h *= 1099511628211ull;
                }
                return h;
            }
        };

        // Chebyshev omega update — see Wang 2015.
        float NextOmega(float omega_prev, float rho_sq, int iter)
        {
            if (iter <= 0) return 1.0f;
            if (iter == 1) return 2.0f / (2.0f - rho_sq);
            return 4.0f / (4.0f - rho_sq * omega_prev);
        }
    }

    PhysicsSystem::PhysicsSystem(CudaManager& cudaManager)
        : m_cuda_manager(cudaManager) {}

    PhysicsSystem::~PhysicsSystem() { ShutdownGPUResources(); }

    void PhysicsSystem::Init(EntityManager& entityManager)
    {
        InitializeGPUParticles(entityManager);
        InitializeGPUConstraints(entityManager, m_particle_entities);
        InitializeRigidBodies(entityManager, m_particle_entities);
    }

    void PhysicsSystem::AutoTune(float particle_radius,
                                 float /*rest_density*/,
                                 float damping_per_sec,
                                 float boundary_recover,
                                 float /*shape_match_pframe*/)
    {
        using namespace StabilityLimits;
        const float g = glm::length(m_gravity);

        m_particleRadius         = particle_radius;
        m_fluidInteractionRadius = InteractionRadiusFromParticleRadius(particle_radius);
        m_substeps               = RecommendedSubsteps(m_fixedTimeStep, particle_radius, g);
        if (m_solverIterations < 1) m_solverIterations = 2;

        const float sub_dt   = m_fixedTimeStep / float(m_substeps);
        m_maxParticleSpeed   = MaxParticleSpeed(particle_radius, sub_dt);
        m_globalDamping      = 0.994f * DampingPerSubstep(damping_per_sec, sub_dt);

        // PBF defaults — see StabilityLimits.h and PBF Eq. 13.
        m_pbfPressureK       = kPBF_DefaultK;
        m_pbfDeltaQRatio     = kPBF_DefaultDeltaQRatio;

        const int total_iters = m_substeps * m_solverIterations;
        m_boundaryPushStrength = BoundaryPushPerIteration(boundary_recover, total_iters);

        Log(LogLevel::Info, std::format(
            "PhysicsSystem AutoTune: r={:.3f}, h={:.3f}, substeps={}, iters={}, sub_dt={:.4f}, "
            "v_max={:.2f}, damping_per_sub={:.4f}, bpush={:.4f}",
            m_particleRadius, m_fluidInteractionRadius, m_substeps, m_solverIterations,
            sub_dt, m_maxParticleSpeed, m_globalDamping, m_boundaryPushStrength));
    }

    void PhysicsSystem::InitializeGPUParticles(EntityManager& entityManager)
    {
        auto& registry = entityManager.GetRegistry();
        auto view = registry.view<TransformComponent, ParticleComponent>();

        m_particle_entities.clear();
        m_particle_entities.reserve(view.size_hint());
        for (auto entity : view) m_particle_entities.push_back(entity);
        m_num_particles = static_cast<uint32_t>(m_particle_entities.size());
        if (m_num_particles == 0) {
            Log(LogLevel::Warning, "PhysicsSystem: no particles found at init.");
            return;
        }

        // Render-side VBOs: positions and per-particle material id.
        glGenBuffers(1, &m_particle_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_num_particles * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &m_material_id_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_material_id_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_num_particles * sizeof(uint32_t), nullptr, GL_STATIC_DRAW);

        glGenBuffers(1, &m_flags_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_flags_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_num_particles * sizeof(uint32_t), nullptr, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_cuda_manager.RegisterGLBuffer(m_particle_vbo);
        // Foam classifier writes per-particle bits each frame — interop the flags buffer too.
        m_cuda_manager.RegisterGLBuffer(m_flags_vbo);

#if HEX_ENABLE_CUDA
        cudaMalloc(&d_positions,           m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_predicted_positions, m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_velocities,          m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_inverse_masses,      m_num_particles * sizeof(float));
        cudaMalloc(&d_material_ids,        m_num_particles * sizeof(uint32_t));
        cudaMalloc(&d_position_deltas,     m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_contact_normals,     m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_chebyshev_prev,      m_num_particles * sizeof(glm::vec3));
        cudaMalloc(&d_density_lambdas,     m_num_particles * sizeof(float));
#endif

        std::vector<glm::vec3> host_positions;       host_positions.reserve(m_num_particles);
        std::vector<glm::vec3> host_velocities;      host_velocities.reserve(m_num_particles);
        std::vector<float>     host_inverse_masses;  host_inverse_masses.reserve(m_num_particles);
        std::vector<uint32_t>  host_material_ids;    host_material_ids.reserve(m_num_particles);

        m_host_materials.clear();
        m_host_materials.push_back(PhysicsMaterial{});
        std::unordered_map<PhysicsMaterialKey, uint32_t, PhysicsMaterialKeyHasher> material_lookup;
        material_lookup[{ m_host_materials.front() }] = 0;

        for (auto entity : m_particle_entities) {
            const auto& transform = view.get<TransformComponent>(entity);
            const auto& particle  = view.get<ParticleComponent>(entity);
            host_positions.push_back(transform.position);
            host_velocities.push_back(particle.velocity);
            host_inverse_masses.push_back(particle.inverseMass);

            PhysicsMaterial mat{};
            if (auto* mc = registry.try_get<PhysicsMaterialComponent>(entity)) mat = mc->material;

            PhysicsMaterialKey key{ mat };
            auto it = material_lookup.find(key);
            uint32_t mid;
            if (it == material_lookup.end()) {
                mid = static_cast<uint32_t>(m_host_materials.size());
                m_host_materials.push_back(mat);
                material_lookup.emplace(key, mid);
            } else {
                mid = it->second;
            }
            host_material_ids.push_back(mid);
        }

        m_num_materials = static_cast<uint32_t>(m_host_materials.size());
#if HEX_ENABLE_CUDA
        cudaMalloc(&d_materials, m_num_materials * sizeof(PhysicsMaterial));

        cudaMemcpy(d_positions,           host_positions.data(),       host_positions.size() * sizeof(glm::vec3),       cudaMemcpyHostToDevice);
        cudaMemcpy(d_predicted_positions, host_positions.data(),       host_positions.size() * sizeof(glm::vec3),       cudaMemcpyHostToDevice);
        cudaMemcpy(d_chebyshev_prev,      host_positions.data(),       host_positions.size() * sizeof(glm::vec3),       cudaMemcpyHostToDevice);
        cudaMemcpy(d_velocities,          host_velocities.data(),      host_velocities.size() * sizeof(glm::vec3),      cudaMemcpyHostToDevice);
        cudaMemcpy(d_inverse_masses,      host_inverse_masses.data(),  host_inverse_masses.size() * sizeof(float),      cudaMemcpyHostToDevice);
        cudaMemcpy(d_material_ids,        host_material_ids.data(),    host_material_ids.size() * sizeof(uint32_t),     cudaMemcpyHostToDevice);
        cudaMemcpy(d_materials,           m_host_materials.data(),     m_host_materials.size() * sizeof(PhysicsMaterial), cudaMemcpyHostToDevice);
#endif

        // Upload material IDs to the GL buffer once — they don't change at runtime.
        glBindBuffer(GL_ARRAY_BUFFER, m_material_id_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, host_material_ids.size() * sizeof(uint32_t), host_material_ids.data());

        // Flags VBO: bit 0 = rigid body member (set later in InitializeRigidBodies).
        //            bit 1 = fluid phase
        //            bit 2 = cloth phase  (impostor hides these in mesh modes so the wireframe
        //                                  overlay drawn by the renderer can replace them)
        std::vector<uint32_t> initial_flags(m_num_particles, 0u);
        for (uint32_t i = 0; i < m_num_particles; ++i) {
            const auto& mat = m_host_materials[host_material_ids[i]];
            if (mat.phase == static_cast<uint32_t>(ParticlePhase::Fluid)) initial_flags[i] |= 2u;
            if (mat.phase == static_cast<uint32_t>(ParticlePhase::Cloth)) initial_flags[i] |= 4u;
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_flags_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, initial_flags.size() * sizeof(uint32_t), initial_flags.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Device mirror — the foam classifier reads this each frame and writes
        // (static | foam_bit) to the interop'd GL flags buffer (which is WriteDiscard, so we
        // can't rely on its previous-frame contents).
#if HEX_ENABLE_CUDA
        cudaMalloc(&d_static_flags, m_num_particles * sizeof(uint32_t));
        cudaMemcpy(d_static_flags, initial_flags.data(),
                   m_num_particles * sizeof(uint32_t), cudaMemcpyHostToDevice);
#endif

        m_spatial_grid = std::make_unique<CudaSpatialGrid>();
        m_spatial_grid->Init(m_num_particles, m_fluidInteractionRadius);

        Log(LogLevel::Info, std::format(
            "PhysicsSystem: {} particles, {} unique materials.", m_num_particles, m_num_materials));
    }

    void PhysicsSystem::InitializeGPUConstraints(EntityManager& entityManager,
                                                  const std::vector<entt::entity>& particle_entities)
    {
        auto& registry = entityManager.GetRegistry();
        std::unordered_map<entt::entity, uint32_t> entity_to_row;
        entity_to_row.reserve(particle_entities.size());
        for (uint32_t i = 0; i < particle_entities.size(); ++i)
            entity_to_row.emplace(particle_entities[i], i);

        std::vector<DistanceConstraint> host_constraints;
        auto cview = registry.view<DistanceConstraintComponent>();
        host_constraints.reserve(cview.size());
        for (auto entity : cview) {
            const auto& c = cview.get<DistanceConstraintComponent>(entity);
            auto itA = entity_to_row.find(c.entityA);
            auto itB = entity_to_row.find(c.entityB);
            if (itA == entity_to_row.end() || itB == entity_to_row.end()) continue;
            host_constraints.push_back({ itA->second, itB->second, c.restDistance });
        }

        m_num_constraints = static_cast<uint32_t>(host_constraints.size());
        if (m_num_constraints == 0) return;

#if HEX_ENABLE_CUDA
        cudaMalloc(&d_distance_constraints, m_num_constraints * sizeof(DistanceConstraint));
        cudaMalloc(&d_distance_lambdas,     m_num_constraints * sizeof(float));
        cudaMemcpy(d_distance_constraints, host_constraints.data(),
                   m_num_constraints * sizeof(DistanceConstraint), cudaMemcpyHostToDevice);
#endif
        Log(LogLevel::Info, std::format("PhysicsSystem: {} distance constraints.", m_num_constraints));
    }

    void PhysicsSystem::InitializeRigidBodies(EntityManager& entityManager,
                                              const std::vector<entt::entity>& particle_entities)
    {
        auto& registry = entityManager.GetRegistry();
        std::unordered_map<entt::entity, uint32_t> entity_to_row;
        entity_to_row.reserve(particle_entities.size());
        for (uint32_t i = 0; i < particle_entities.size(); ++i)
            entity_to_row.emplace(particle_entities[i], i);

        // Group rigid-body member particles by rigidBodyId.
        std::map<uint32_t, std::vector<std::pair<uint32_t, glm::vec3>>> by_id; // id → list of (row, rest_local)
        auto mview = registry.view<RigidBodyMemberComponent>();
        for (auto e : mview) {
            const auto& m = mview.get<RigidBodyMemberComponent>(e);
            auto it = entity_to_row.find(e);
            if (it == entity_to_row.end()) continue;
            by_id[m.rigidBodyId].emplace_back(it->second, m.restLocalPos);
        }

        // Read per-body parameters.
        std::unordered_map<uint32_t, float> stiffness_for;
        auto bview = registry.view<RigidBodyComponent>();
        for (auto e : bview) {
            const auto& b = bview.get<RigidBodyComponent>(e);
            stiffness_for[b.rigidBodyId] = b.stiffness;
        }

        m_num_rigid_bodies = static_cast<uint32_t>(by_id.size());
        if (m_num_rigid_bodies == 0) return;

        // Fetch host-side initial positions and inverse masses for body mass/inertia computation.
        std::vector<glm::vec3> host_pos(m_num_particles);
        std::vector<float>     host_inv_mass(m_num_particles);
#if HEX_ENABLE_CUDA
        cudaMemcpy(host_pos.data(),      d_positions,      m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToHost);
        cudaMemcpy(host_inv_mass.data(), d_inverse_masses, m_num_particles * sizeof(float),     cudaMemcpyDeviceToHost);
#endif

        std::vector<RigidBodyGPU> host_bodies;
        std::vector<uint32_t>  host_indices;
        std::vector<glm::vec4> host_rest;
        std::vector<uint32_t>  host_slot_to_body;
        host_bodies.reserve(m_num_rigid_bodies);
        m_rigid_body_ids.clear();
        m_rigid_body_ids.reserve(m_num_rigid_bodies);

        for (auto& [id, members] : by_id) {
            m_rigid_body_ids.push_back(id);
            const uint32_t body_index = static_cast<uint32_t>(host_bodies.size());

            RigidBodyGPU body{};
            body.first_particle = static_cast<uint32_t>(host_indices.size());
            body.particle_count = static_cast<uint32_t>(members.size());
            auto it = stiffness_for.find(id);
            body.stiffness  = (it == stiffness_for.end()) ? 1.0f : it->second;
            body.orientation      = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            body.prev_orientation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

            // ---- Mass + centroid in *world* space, and local-frame centroid offset --------
            float total_mass = 0.0f;
            glm::vec3 cm_world(0.0f);
            glm::vec3 cm_local(0.0f);
            for (auto& [row, rest_local] : members) {
                float w = host_inv_mass[row];
                float m = (w > 0.0f) ? (1.0f / w) : 0.0f;   // 0 = pinned anchor — ignored for inertia
                total_mass += m;
                cm_world   += m * host_pos[row];
                cm_local   += m * rest_local;
            }
            if (total_mass > 1e-9f) {
                cm_world /= total_mass;
                cm_local /= total_mass;
            }

            body.position       = glm::vec4(cm_world, 0.0f);
            body.prev_position  = glm::vec4(cm_world, 0.0f);
            body.total_inv_mass = (total_mass > 1e-9f) ? (1.0f / total_mass) : 0.0f;

            // ---- Inertia tensor I = Σ m_i · (|r_i|² · I − r_i ⊗ r_i)  (body-local frame) ---
            //   where r_i is the rest-local position MINUS local-frame centroid (so the
            //   tensor is about the body's centre of mass).
            glm::mat3 I_local(0.0f);
            for (auto& [row, rest_local] : members) {
                float w = host_inv_mass[row];
                float m = (w > 0.0f) ? (1.0f / w) : 0.0f;
                glm::vec3 r = rest_local - cm_local;
                float rr = glm::dot(r, r);
                I_local[0][0] += m * (rr - r.x * r.x);
                I_local[1][1] += m * (rr - r.y * r.y);
                I_local[2][2] += m * (rr - r.z * r.z);
                I_local[0][1] -= m * r.x * r.y;
                I_local[0][2] -= m * r.x * r.z;
                I_local[1][2] -= m * r.y * r.z;
            }
            I_local[1][0] = I_local[0][1];
            I_local[2][0] = I_local[0][2];
            I_local[2][1] = I_local[1][2];

            // Regularise — a perfectly axis-aligned slab can be singular along one axis. Add
            // a tiny diagonal so the inverse stays finite (the magnitude is set well below
            // the smallest realistic moment for a particle cluster).
            const float kInertiaFloor = 1e-6f;
            I_local[0][0] = std::max(I_local[0][0], kInertiaFloor);
            I_local[1][1] = std::max(I_local[1][1], kInertiaFloor);
            I_local[2][2] = std::max(I_local[2][2], kInertiaFloor);

            glm::mat3 I_inv = (total_mass > 1e-9f) ? glm::inverse(I_local) : glm::mat3(0.0f);
            body.inv_inertia_col0 = glm::vec4(I_inv[0], 0.0f);
            body.inv_inertia_col1 = glm::vec4(I_inv[1], 0.0f);
            body.inv_inertia_col2 = glm::vec4(I_inv[2], 0.0f);

            host_bodies.push_back(body);

            // Slot data — rest positions are STORED CENTROID-RELATIVE so warp(q,rest) + x
            // matches both the world frame at init and any future body pose.
            for (auto& [row, rest_local] : members) {
                host_indices.push_back(row);
                host_rest.emplace_back(rest_local - cm_local, 0.0f);
                host_slot_to_body.push_back(body_index);
            }
        }

        m_num_rigid_slots = static_cast<uint32_t>(host_indices.size());

        m_particle_to_rigid_index.assign(m_num_particles, 0xFFFFFFFFu);
        for (uint32_t b = 0; b < host_bodies.size(); ++b) {
            const auto& body = host_bodies[b];
            for (uint32_t k = 0; k < body.particle_count; ++k) {
                uint32_t row = host_indices[body.first_particle + k];
                if (row < m_num_particles) m_particle_to_rigid_index[row] = b;
            }
        }

#if HEX_ENABLE_CUDA
        cudaMalloc(&d_rigid_bodies,           m_num_rigid_bodies * sizeof(RigidBodyGPU));
        cudaMalloc(&d_rigid_particle_indices, m_num_rigid_slots  * sizeof(uint32_t));
        cudaMalloc(&d_rigid_rest_positions,   m_num_rigid_slots  * sizeof(glm::vec4));
        cudaMalloc(&d_slot_to_body,           m_num_rigid_slots  * sizeof(uint32_t));
        cudaMalloc(&d_particle_to_body,       m_num_particles    * sizeof(uint32_t));
        cudaMemcpy(d_rigid_bodies,           host_bodies.data(),       host_bodies.size()       * sizeof(RigidBodyGPU), cudaMemcpyHostToDevice);
        cudaMemcpy(d_rigid_particle_indices, host_indices.data(),      host_indices.size()      * sizeof(uint32_t),     cudaMemcpyHostToDevice);
        cudaMemcpy(d_rigid_rest_positions,   host_rest.data(),         host_rest.size()         * sizeof(glm::vec4),    cudaMemcpyHostToDevice);
        cudaMemcpy(d_slot_to_body,           host_slot_to_body.data(), host_slot_to_body.size() * sizeof(uint32_t),     cudaMemcpyHostToDevice);
        cudaMemcpy(d_particle_to_body,       m_particle_to_rigid_index.data(), m_num_particles * sizeof(uint32_t),     cudaMemcpyHostToDevice);
#endif

        // Mark rigid-member particles in the flags VBO so the renderer can hide them in mesh mode.
        if (m_flags_vbo) {
            std::vector<uint32_t> flags(m_num_particles, 0u);
            glBindBuffer(GL_ARRAY_BUFFER, m_flags_vbo);
            glGetBufferSubData(GL_ARRAY_BUFFER, 0, flags.size() * sizeof(uint32_t), flags.data());
            for (uint32_t idx : host_indices) {
                if (idx < flags.size()) flags[idx] |= 1u;
            }
            glBufferSubData(GL_ARRAY_BUFFER, 0, flags.size() * sizeof(uint32_t), flags.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (d_static_flags) {
#if HEX_ENABLE_CUDA
                cudaMemcpy(d_static_flags, flags.data(),
                           m_num_particles * sizeof(uint32_t), cudaMemcpyHostToDevice);
#endif
            }
        }

        Log(LogLevel::Info, std::format(
            "PhysicsSystem: {} rigid bodies ({} member particles).", m_num_rigid_bodies, m_num_rigid_slots));
    }

    void PhysicsSystem::ShutdownGPUResources()
    {
        m_spatial_grid.reset();
        if (m_particle_vbo) {
            m_cuda_manager.UnregisterGLBuffers();
            glDeleteBuffers(1, &m_particle_vbo);
            m_particle_vbo = 0;
        }
        if (m_material_id_vbo) {
            glDeleteBuffers(1, &m_material_id_vbo);
            m_material_id_vbo = 0;
        }
        if (m_flags_vbo) {
            glDeleteBuffers(1, &m_flags_vbo);
            m_flags_vbo = 0;
        }

        auto try_free = [](auto*& p) {
            if (p) {
#if HEX_ENABLE_CUDA
                cudaFree(p);
#endif
                p = nullptr;
            }
        };
        try_free(d_positions);
        try_free(d_predicted_positions);
        try_free(d_velocities);
        try_free(d_inverse_masses);
        try_free(d_material_ids);
        try_free(d_materials);
        try_free(d_position_deltas);
        try_free(d_contact_normals);
        try_free(d_chebyshev_prev);
        try_free(d_density_lambdas);
        try_free(d_distance_lambdas);
        try_free(d_distance_constraints);
        try_free(d_rigid_bodies);
        try_free(d_rigid_particle_indices);
        try_free(d_rigid_rest_positions);
        try_free(d_slot_to_body);
        try_free(d_particle_to_body);
        try_free(d_static_flags);

        m_num_particles = m_num_materials = m_num_constraints = 0;
        m_num_rigid_bodies = m_num_rigid_slots = 0;
        m_particle_entities.clear();
        m_host_materials.clear();
    }

    void PhysicsSystem::Tick(EntityManager& entityManager, float deltaTime, float /*currentTime*/)
    {
        // SOTA: Synchronous Variable Stepping.
        // Instead of an accumulator 'while' loop (which causes sawtooth jitter),
        // we perform exactly one set of substeps every frame, scaled by dt.
        // XPBD is inherently stable under variable time-steps.
        
        const float max_dt = 1.0f / 30.0f; // Cap dt to prevent explosions during lag
        float clamped_dt = (deltaTime > max_dt) ? max_dt : deltaTime;

        SimulateStep(entityManager, clamped_dt);
    }

    void PhysicsSystem::SimulateStep(EntityManager& entityManager, float fixedDeltaTime)
    {
#if HEX_ENABLE_CUDA
        if (fixedDeltaTime <= 0.0f || m_num_particles == 0) return;

        m_cuda_manager.MapGLBuffers();
        auto* d_vbo_positions = static_cast<glm::vec3*>(m_cuda_manager.GetMappedPointer(m_particle_vbo));
        auto* d_vbo_flags     = static_cast<uint32_t*>(m_cuda_manager.GetMappedPointer(m_flags_vbo));

        // Macklin 2019 small steps: many small substeps, few iterations per substep. With
        // sub_dt small, the constraint linearization is much more accurate and energy drift
        // collapses. Per-frame work ≈ substeps × iters_per_substep neighbour passes.
        const int substeps = (m_substeps > 0) ? m_substeps : 1;
        const float sub_dt = fixedDeltaTime / static_cast<float>(substeps);

        const float rho_sq = m_chebyshevRho * m_chebyshevRho;

        // Per-substep damping multipliers (exponential decay applied at predict time).
        const float lin_damp_sub = std::exp(-m_rigidLinearDampingPerSec  * sub_dt);
        const float ang_damp_sub = std::exp(-m_rigidAngularDampingPerSec * sub_dt);

        for (int sub = 0; sub < substeps; ++sub)
        {
            // 1a. Predict free-particle positions for this substep.
            launch_predictPositions(
                d_positions, d_predicted_positions, d_velocities,
                d_inverse_masses, d_material_ids, d_materials,
                m_num_particles, m_gravity, sub_dt);

            // 1b. Predict rigid bodies (Müller 2020: integrate v_lin, ω with gravity + damping),
            //     then warp rigid-member particles' predicted positions onto the new body pose.
            //     This OVERRIDES whatever predictPositions wrote for rigid members, which is
            //     correct — rigid particles are kinematic followers of the body state.
            if (m_num_rigid_bodies > 0) {
                launch_predictRigidBodies(
                    d_rigid_bodies, m_num_rigid_bodies, m_gravity, sub_dt,
                    lin_damp_sub, ang_damp_sub);
                launch_warpParticlesToBody(
                    d_predicted_positions, d_rigid_particle_indices, d_rigid_rest_positions,
                    d_slot_to_body, d_rigid_bodies, m_num_rigid_slots);
            }

            // 2. Build spatial hash on predicted positions.
            m_spatial_grid->Build(d_predicted_positions, m_num_particles);

            // 3. Reset per-substep accumulators.
            launch_zeroVec3(d_contact_normals, m_num_particles);
            if (m_num_constraints > 0) {
                cudaMemsetAsync(d_distance_lambdas, 0, m_num_constraints * sizeof(float));
            }
            if (m_chebyshevEnabled) {
                cudaMemcpyAsync(d_chebyshev_prev, d_predicted_positions,
                                m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToDevice);
            }

            // 4. Solver iterations (typically 1-2 with small steps).
            float omega = 1.0f;
            for (int iter = 0; iter < m_solverIterations; ++iter)
            {
                launch_resetSolverData(d_position_deltas, d_density_lambdas, m_num_particles);

                launch_calculateLambdas(
                    d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
                    m_spatial_grid.get(), d_density_lambdas,
                    m_num_particles, m_fluidInteractionRadius, sub_dt);

                launch_applyDensityCorrections(
                    d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
                    m_spatial_grid.get(), d_position_deltas, d_density_lambdas,
                    m_num_particles, m_fluidInteractionRadius, m_pbfPressureK, m_pbfDeltaQRatio);

                // Boundary Push & Domain Collision MUST be inside the loop
                // so the Rigid Aggregator can see the corrections and stop the body.
                launch_solveBoundaryPush(
                    d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
                    d_position_deltas, m_num_particles, m_domain,
                    m_fluidInteractionRadius, m_boundaryPushStrength);

                launch_solveDomainCollision(
                    d_positions, d_predicted_positions,
                    d_material_ids, d_materials,
                    d_position_deltas, d_contact_normals,
                    m_num_particles, m_domain, m_particleRadius);

                if (m_num_constraints > 0) {
                    launch_solveDistanceConstraint(
                        d_predicted_positions, d_inverse_masses, d_distance_constraints,
                        d_position_deltas, d_distance_lambdas, m_num_constraints,
                        m_distanceCompliance, sub_dt);
                }

                launch_solveParticleContacts(
                    d_positions, d_predicted_positions, d_inverse_masses,
                    d_material_ids, d_materials, d_particle_to_body, m_spatial_grid.get(),
                    d_position_deltas, d_contact_normals,
                    m_num_particles, m_particleRadius);

                // Aggregate rigid corrections BEFORE applyDeltas.
                if (m_num_rigid_bodies > 0) {
                    launch_aggregateRigidCorrections(
                        d_predicted_positions, d_position_deltas, d_inverse_masses,
                        d_rigid_particle_indices, d_rigid_bodies, m_num_rigid_bodies);
                }

                launch_applyDeltas(d_predicted_positions, d_position_deltas,
                                   d_inverse_masses, m_num_particles);

                if (m_num_rigid_bodies > 0) {
                    launch_warpParticlesToBody(
                        d_predicted_positions, d_rigid_particle_indices, d_rigid_rest_positions,
                        d_slot_to_body, d_rigid_bodies, m_num_rigid_slots);
                }

                if (m_chebyshevEnabled && iter >= 1) {
                    omega = NextOmega(omega, rho_sq, iter);
                    if (omega > 2.0f) omega = 2.0f;
                    launch_chebyshevBlend(d_predicted_positions, d_chebyshev_prev,
                                          omega, m_num_particles);
                }
            }

            // 5. Velocity update for free particles, with restitution along contact normals.
            launch_updateVelocities(
                d_velocities, d_positions, d_predicted_positions,
                d_material_ids, d_materials, d_contact_normals,
                m_num_particles, 1.0f / sub_dt, m_globalDamping);

            // 5b. Finalize rigid bodies: derive v_lin, ω from (x − prev_x), (q · prev_q⁻¹) and
            //     push per-particle velocities to (v_lin + ω × r) for downstream XSPH/viscosity.
            if (m_num_rigid_bodies > 0) {
                launch_finalizeRigidBodies(
                    d_velocities, d_predicted_positions,
                    d_rigid_particle_indices, d_slot_to_body, d_rigid_bodies,
                    m_num_rigid_bodies, m_num_rigid_slots, 1.0f / sub_dt);
            }

            // 5c. Picker spring. For a rigid-body member we apply the spring at body level
            //     (impulse on v_lin AND torque on ω, via the body's inverse inertia). For a
            //     free particle we use the legacy per-particle spring.
            if (m_picked_particle != 0xFFFFFFFFu && m_picked_particle < m_num_particles) {
                if (m_picked_rigid_count > 0 && m_picked_body_index < m_num_rigid_bodies) {
                    launch_applyPickerSpringBody(
                        d_rigid_bodies, m_picked_body_index, d_predicted_positions,
                        m_picked_particle, m_pick_target, m_pickStiffness, m_pickDamping, sub_dt);
                } else {
                    launch_applyPickerSpring(
                        d_velocities, d_predicted_positions, m_picked_particle,
                        m_pick_target, m_pickStiffness, m_pickDamping, sub_dt);
                }
            }

            // 6. Clamp speeds — prevents tunneling on the next predict.
            launch_clampVelocities(d_velocities, m_num_particles, m_maxParticleSpeed);

            // 7. Vorticity confinement (optional, off by default — it injects energy).
            // 7. Advance.
            cudaMemcpyAsync(d_positions, d_predicted_positions,
                            m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToDevice);
        }

        // 8. XSPH Viscosity (Macklin & Müller 2013 §5). 
        // Move OUT of the loop to run once per frame instead of per-substep.
        // This is a massive performance win for neighbor-search intensive passes.
        launch_applyXSPHViscosity(
            d_positions, d_velocities,
            d_material_ids, d_materials,
            m_spatial_grid.get(), m_num_particles, m_fluidInteractionRadius);

        // 9. Publish final positions to the render VBO (single device→device copy per frame).

        // 9. Publish final positions to the render VBO (single device→device copy per frame).
        if (d_vbo_positions) {
            cudaMemcpyAsync(d_vbo_positions, d_predicted_positions,
                            m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToDevice);
        }

        // 10. Foam classification — updates bit 3 of the flags VBO so the SSFR foam pass can
        //     pick up splash/spray particles. Cheap (one neighbour scan), runs once per frame.
        if (d_vbo_flags && m_spatial_grid && d_static_flags) {
            launch_classifyFoam(
                d_vbo_flags, d_static_flags, d_velocities, d_predicted_positions,
                d_material_ids, d_materials, m_spatial_grid.get(),
                m_num_particles, m_fluidInteractionRadius,
                m_foamMinSpeed, m_foamMaxSpeed, m_foamLowNeighbours);
        }

        m_cuda_manager.UnmapGLBuffers();
#else
        if (fixedDeltaTime > 0.0f && m_num_particles > 0)
        {
            const int substeps = (m_substeps > 0) ? m_substeps : 1;
            const float sub_dt = fixedDeltaTime / static_cast<float>(substeps);
            auto& registry = entityManager.GetRegistry();

            for (int sub = 0; sub < substeps; ++sub)
            {
                // 1. Predict positions (XPBD Integration)
                for (auto entity : m_particle_entities) {
                    auto& tc = registry.get<TransformComponent>(entity);
                    auto& pc = registry.get<ParticleComponent>(entity);
                    if (pc.inverseMass == 0.0f) continue;
                    pc.velocity += m_gravity * sub_dt;
                    tc.position += pc.velocity * sub_dt;
                }

                // 2. XPBD Distance Constraints Iterations
                auto cview = registry.view<DistanceConstraintComponent>();
                for (int iter = 0; iter < m_solverIterations; ++iter) {
                    for (auto c_entity : cview) {
                        const auto& c = cview.get<DistanceConstraintComponent>(c_entity);
                        auto* tcA = registry.try_get<TransformComponent>(c.entityA);
                        auto* tcB = registry.try_get<TransformComponent>(c.entityB);
                        auto* pcA = registry.try_get<ParticleComponent>(c.entityA);
                        auto* pcB = registry.try_get<ParticleComponent>(c.entityB);
                        if (!tcA || !tcB) continue;
                        float wA = pcA ? pcA->inverseMass : 0.0f;
                        float wB = pcB ? pcB->inverseMass : 0.0f;
                        float wSum = wA + wB;
                        if (wSum <= 1e-7f) continue;

                        glm::vec3 delta = tcA->position - tcB->position;
                        float len = glm::length(delta);
                        if (len <= 1e-7f) continue;

                        float C = len - c.restDistance;
                        glm::vec3 n = delta / len;
                        float alpha_tilde = m_distanceCompliance / (sub_dt * sub_dt);
                        float delta_lambda = -C / (wSum + alpha_tilde);

                        if (wA > 0.0f) tcA->position += wA * delta_lambda * n;
                        if (wB > 0.0f) tcB->position -= wB * delta_lambda * n;
                    }

                    // 3. XPBD Domain Wall Collisions with velocity reflection & friction
                    for (auto entity : m_particle_entities) {
                        auto& tc = registry.get<TransformComponent>(entity);
                        auto& pc = registry.get<ParticleComponent>(entity);
                        if (pc.inverseMass == 0.0f) continue;

                        const float r = m_particleRadius;
                        glm::vec3 min_bound = m_domain.min + glm::vec3(r);
                        glm::vec3 max_bound = m_domain.max - glm::vec3(r);

                        if (tc.position.x < min_bound.x) { tc.position.x = min_bound.x; pc.velocity.x = std::abs(pc.velocity.x) * 0.1f; }
                        if (tc.position.x > max_bound.x) { tc.position.x = max_bound.x; pc.velocity.x = -std::abs(pc.velocity.x) * 0.1f; }
                        if (tc.position.y < min_bound.y) { tc.position.y = min_bound.y; pc.velocity.y = std::abs(pc.velocity.y) * 0.1f; pc.velocity.x *= 0.95f; pc.velocity.z *= 0.95f; }
                        if (tc.position.y > max_bound.y) { tc.position.y = max_bound.y; pc.velocity.y = -std::abs(pc.velocity.y) * 0.1f; }
                        if (tc.position.z < min_bound.z) { tc.position.z = min_bound.z; pc.velocity.z = std::abs(pc.velocity.z) * 0.1f; }
                        if (tc.position.z > max_bound.z) { tc.position.z = max_bound.z; pc.velocity.z = -std::abs(pc.velocity.z) * 0.1f; }
                    }
                }

                // 3b. Particle-Particle Contact Resolution (Volume Preservation, O(1) Contiguous Spatial Grid)
                const float min_dist = 2.0f * m_particleRadius;
                const float min_dist2 = min_dist * min_dist;
                const size_t num_p = m_particle_entities.size();
                const float cell_size = min_dist * 1.2f;

                glm::vec3 domain_size = m_domain.max - m_domain.min;
                int dim_x = std::max(1, static_cast<int>(std::ceil(domain_size.x / cell_size)));
                int dim_y = std::max(1, static_cast<int>(std::ceil(domain_size.y / cell_size)));
                int dim_z = std::max(1, static_cast<int>(std::ceil(domain_size.z / cell_size)));
                int total_cells = dim_x * dim_y * dim_z;

                std::vector<int> head(total_cells, -1);
                std::vector<int> next(num_p, -1);

                for (size_t i = 0; i < num_p; ++i) {
                    const auto& tc = registry.get<TransformComponent>(m_particle_entities[i]);
                    int cx = std::clamp(static_cast<int>((tc.position.x - m_domain.min.x) / cell_size), 0, dim_x - 1);
                    int cy = std::clamp(static_cast<int>((tc.position.y - m_domain.min.y) / cell_size), 0, dim_y - 1);
                    int cz = std::clamp(static_cast<int>((tc.position.z - m_domain.min.z) / cell_size), 0, dim_z - 1);
                    int c_idx = cx + dim_x * (cy + dim_y * cz);
                    next[i] = head[c_idx];
                    head[c_idx] = static_cast<int>(i);
                }

                for (size_t i = 0; i < num_p; ++i) {
                    auto eA = m_particle_entities[i];
                    auto& tcA = registry.get<TransformComponent>(eA);
                    auto& pcA = registry.get<ParticleComponent>(eA);
                    int cx = std::clamp(static_cast<int>((tcA.position.x - m_domain.min.x) / cell_size), 0, dim_x - 1);
                    int cy = std::clamp(static_cast<int>((tcA.position.y - m_domain.min.y) / cell_size), 0, dim_y - 1);
                    int cz = std::clamp(static_cast<int>((tcA.position.z - m_domain.min.z) / cell_size), 0, dim_z - 1);

                    for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                    for (int dz = -1; dz <= 1; ++dz) {
                        int nx = cx + dx, ny = cy + dy, nz = cz + dz;
                        if (nx < 0 || nx >= dim_x || ny < 0 || ny >= dim_y || nz < 0 || nz >= dim_z) continue;
                        int neighbor_cell = nx + dim_x * (ny + dim_y * nz);

                        for (int j = head[neighbor_cell]; j != -1; j = next[j]) {
                            if (static_cast<size_t>(j) <= i) continue;
                            auto eB = m_particle_entities[j];
                            auto& tcB = registry.get<TransformComponent>(eB);
                            auto& pcB = registry.get<ParticleComponent>(eB);
                            glm::vec3 delta = tcA.position - tcB.position;
                            float d2 = glm::dot(delta, delta);
                            if (d2 > 0.0f && d2 < min_dist2) {
                                float d = std::sqrt(d2);
                                glm::vec3 n = delta / d;
                                float overlap = std::min(min_dist - d, m_particleRadius * 0.2f);
                                float wA = pcA.inverseMass;
                                float wB = pcB.inverseMass;
                                float wSum = wA + wB;
                                if (wSum > 1e-7f) {
                                    if (wA > 0.0f) tcA.position += (wA / wSum) * overlap * 0.5f * n;
                                    if (wB > 0.0f) tcB.position -= (wB / wSum) * overlap * 0.5f * n;
                                }
                            }
                        }
                    }
                }

                // 4. Shape Matching for Sampled Rigid Bodies (Müller et al. 2005 / Macklin 2019 XPBD)
                if (m_num_rigid_bodies > 0) {
                    m_rigid_transforms.resize(m_num_rigid_bodies);
                    std::vector<glm::vec3> centroids(m_num_rigid_bodies, glm::vec3(0.0f));
                    std::vector<int> counts(m_num_rigid_bodies, 0);

                    for (auto entity : m_particle_entities) {
                        auto* rbm = registry.try_get<RigidBodyMemberComponent>(entity);
                        if (!rbm) continue;
                        uint32_t bid = rbm->rigidBodyId - 1;
                        if (bid < m_num_rigid_bodies) {
                            const auto& tc = registry.get<TransformComponent>(entity);
                            centroids[bid] += tc.position;
                            counts[bid]++;
                        }
                    }

                    for (uint32_t b = 0; b < m_num_rigid_bodies; ++b) {
                        if (counts[b] > 0) centroids[b] /= static_cast<float>(counts[b]);
                    }

                    // Covariance A = sum (p_i - cm) * (r_i^0)^T
                    std::vector<glm::mat3> cov(m_num_rigid_bodies, glm::mat3(0.0f));
                    for (auto entity : m_particle_entities) {
                        auto* rbm = registry.try_get<RigidBodyMemberComponent>(entity);
                        if (!rbm) continue;
                        uint32_t bid = rbm->rigidBodyId - 1;
                        if (bid < m_num_rigid_bodies) {
                            const auto& tc = registry.get<TransformComponent>(entity);
                            glm::vec3 p_local = tc.position - centroids[bid];
                            cov[bid] += glm::outerProduct(p_local, rbm->restLocalPos);
                        }
                    }

                    // Extract rotation R via iterative polar decomposition
                    std::vector<glm::quat> orientations(m_num_rigid_bodies, glm::quat(1,0,0,0));
                    for (uint32_t b = 0; b < m_num_rigid_bodies; ++b) {
                        if (counts[b] == 0) continue;
                        glm::mat3 R = cov[b];
                        if (glm::abs(glm::determinant(R)) > 1e-7f) {
                            for (int iter = 0; iter < 5; ++iter) {
                                glm::mat3 R_inv_T = glm::transpose(glm::inverse(R));
                                R = 0.5f * (R + R_inv_T);
                            }
                            orientations[b] = glm::quat_cast(R);
                        }
                        m_rigid_transforms[b].centroid = centroids[b];
                        m_rigid_transforms[b].orientation = orientations[b];

                        // Target position blend
                        float stiffness = 0.95f;
                        for (auto entity : m_particle_entities) {
                            auto* rbm = registry.try_get<RigidBodyMemberComponent>(entity);
                            if (!rbm || (rbm->rigidBodyId - 1) != b) continue;
                            auto& tc = registry.get<TransformComponent>(entity);
                            glm::vec3 target_pos = centroids[b] + R * rbm->restLocalPos;
                            tc.position += stiffness * (target_pos - tc.position);
                        }
                    }
                }

                // 5. Update Particle Velocities & CFL Speed Clamp
                const float max_speed = (m_maxParticleSpeed > 0.0f) ? m_maxParticleSpeed : 4.0f;
                for (auto entity : m_particle_entities) {
                    auto& tc = registry.get<TransformComponent>(entity);
                    auto& pc = registry.get<ParticleComponent>(entity);
                    if (pc.inverseMass == 0.0f) continue;
                    pc.velocity = (tc.position - pc.predictedPosition) / sub_dt;
                    float speed = glm::length(pc.velocity);
                    if (speed > max_speed) {
                        pc.velocity = (pc.velocity / speed) * max_speed;
                    }
                    pc.velocity *= std::pow(m_globalDamping, sub_dt);
                    pc.predictedPosition = tc.position;
                }
            }

            // Upload particle positions to VBO for rendering
            if (m_particle_vbo && m_num_particles > 0) {
                std::vector<glm::vec3> host_pos;
                host_pos.reserve(m_num_particles);
                for (auto entity : m_particle_entities) {
                    const auto& tc = registry.get<TransformComponent>(entity);
                    host_pos.push_back(tc.position);
                }
                glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
                glBufferSubData(GL_ARRAY_BUFFER, 0, host_pos.size() * sizeof(glm::vec3), host_pos.data());
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
#endif

        if (m_syncTransforms) SyncTransformsFromGPU(entityManager);

        // Rigid body transforms are needed for mesh rendering even when particle sync is off.
        if (m_num_rigid_bodies > 0) SyncRigidBodyTransformsFromGPU();

        // Refresh the picked-particle world position so the spring visual stays current.
        if (m_picked_particle != 0xFFFFFFFFu && m_picked_particle < m_num_particles) {
#if HEX_ENABLE_CUDA
            cudaMemcpy(&m_picked_world_pos, d_predicted_positions + m_picked_particle,
                       sizeof(glm::vec3), cudaMemcpyDeviceToHost);
#endif
        }
    }

    void PhysicsSystem::SyncTransformsFromGPU(EntityManager& entityManager)
    {
#if HEX_ENABLE_CUDA
        std::vector<glm::vec3> host_positions(m_num_particles);
        cudaMemcpy(host_positions.data(), d_predicted_positions,
                   m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToHost);

        auto& registry = entityManager.GetRegistry();
        for (uint32_t i = 0; i < m_num_particles; ++i) {
            entt::entity e = m_particle_entities[i];
            if (auto* t = registry.try_get<TransformComponent>(e)) {
                t->position = host_positions[i];
            }
        }
#endif
    }

    void PhysicsSystem::SetMousePicker(entt::entity entity) { m_mousePickerEntity = entity; }
    entt::entity PhysicsSystem::GetMousePicker() const     { return m_mousePickerEntity; }
    void PhysicsSystem::UpdateMousePickerPosition(EntityManager& entityManager,
                                                  const glm::vec3& worldPosition) const
    {
        if (m_mousePickerEntity == entt::null) return;
        auto& t = entityManager.GetComponent<TransformComponent>(m_mousePickerEntity);
        t.position = worldPosition;
    }

    uint32_t PhysicsSystem::PickParticleByRay(const glm::vec3& ray_origin,
                                               const glm::vec3& ray_dir, float max_radius)
    {
        if (m_num_particles == 0) return 0xFFFFFFFFu;

        std::vector<glm::vec3> host(m_num_particles);
#if HEX_ENABLE_CUDA
        cudaMemcpy(host.data(), d_predicted_positions,
                   m_num_particles * sizeof(glm::vec3), cudaMemcpyDeviceToHost);
#endif

        const glm::vec3 d = glm::normalize(ray_dir);
        const float max_r2 = max_radius * max_radius;

        uint32_t best = 0xFFFFFFFFu;
        float best_t = std::numeric_limits<float>::max();

        for (uint32_t i = 0; i < m_num_particles; ++i) {
            glm::vec3 oc = host[i] - ray_origin;
            float t = glm::dot(oc, d);
            if (t <= 0.0f || t >= best_t) continue;       // behind camera or further than best
            glm::vec3 perp = oc - t * d;
            if (glm::dot(perp, perp) > max_r2) continue;  // outside pick radius
            best = i;
            best_t = t;
        }

        if (best != 0xFFFFFFFFu) {
            m_picked_particle  = best;
            m_picked_world_pos = host[best];
            m_pick_target      = host[best];

            // Did we pick a rigid-body member? Stash the body index for the body-level picker
            // spring, plus the legacy first/count range for any external readers (UI).
            m_picked_rigid_first = 0xFFFFFFFFu;
            m_picked_rigid_count = 0;
            m_picked_body_index  = 0xFFFFFFFFu;
            if (best < m_particle_to_rigid_index.size()) {
                uint32_t body_idx = m_particle_to_rigid_index[best];
                if (body_idx != 0xFFFFFFFFu && body_idx < m_num_rigid_bodies) {
                    m_picked_body_index = body_idx;
                    std::vector<RigidBodyGPU> hostb(m_num_rigid_bodies);
#if HEX_ENABLE_CUDA
                    cudaMemcpy(hostb.data(), d_rigid_bodies,
                               m_num_rigid_bodies * sizeof(RigidBodyGPU), cudaMemcpyDeviceToHost);
                    m_picked_rigid_first = hostb[body_idx].first_particle;
                    m_picked_rigid_count = hostb[body_idx].particle_count;
#endif
                }
            }
        }
        return best;
    }

    void PhysicsSystem::SyncRigidBodyTransformsFromGPU()
    {
#if HEX_ENABLE_CUDA
        if (m_num_rigid_bodies == 0) {
            m_rigid_transforms.clear();
            return;
        }
        std::vector<RigidBodyGPU> host(m_num_rigid_bodies);
        cudaMemcpy(host.data(), d_rigid_bodies,
                   m_num_rigid_bodies * sizeof(RigidBodyGPU), cudaMemcpyDeviceToHost);
        m_rigid_transforms.resize(m_num_rigid_bodies);
        for (uint32_t i = 0; i < m_num_rigid_bodies; ++i) {
            m_rigid_transforms[i].centroid    = glm::vec3(host[i].position);
            // RigidBodyGPU.orientation = (x,y,z,w); glm::quat = (w,x,y,z)
            const glm::vec4 q = host[i].orientation;
            m_rigid_transforms[i].orientation = glm::quat(q.w, q.x, q.y, q.z);
        }
#endif
    }
}
