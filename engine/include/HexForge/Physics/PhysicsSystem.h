#pragma once

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <glad/glad.h>

#include <memory>
#include <vector>
#include <cstdint>

#include "HexForge/Gameplay/EntityComponents.h"
#include "HexForge/Cuda/PBD_Kernels.cuh"

struct cudaGraphicsResource;

namespace Hex
{
    class EntityManager;
    class CudaManager;
    class CudaSpatialGrid;

    // GPU-resident XPBD solver. Solves a single uniform particle pool that lives entirely on
    // the device between frames. Different physical media (water, oil, sand, rubber, rigids,
    // statics) share the same kernels; behaviour branches off a small per-particle material id.
    //
    // Per-step pipeline:
    //   predict → spatial hash → K iterations of {density + distance + contacts + apply + walls}
    //              with optional Chebyshev acceleration between iterations
    //          → velocity update with restitution along accumulated contact normals
    //          → XSPH viscosity smoothing
    //          → DtoD copy into the render VBO  (no CPU readback)
    class PhysicsSystem
    {
    public:
        explicit PhysicsSystem(CudaManager& cudaManager);
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&)            = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        void Init(EntityManager& entityManager);
        void Tick(EntityManager& entityManager, float deltaTime, float currentTime);

        // High-level: derive *every* solver parameter from a small set of physical inputs.
        // See StabilityLimits.h for the derivations.
        //   particle_radius   — sphere radius [m]
        //   rest_density      — target fluid density at close-packed spacing [kg/m³]
        //   damping_per_sec   — exponential decay rate for global damping [1/s]
        //   boundary_recover  — fraction of h that fluid is pushed inward per frame at d=0
        //   shape_match_pframe — per-frame shape-match stiffness for rigid bodies
        void AutoTune(float particle_radius,
                      float rest_density,
                      float damping_per_sec   = 0.15f,  // ~14 %/s — gentle drag, doesn't kill
                                                         // freefall, lets gravity feel like 9.81
                      float boundary_recover  = 0.35f,
                      float shape_match_pframe = 0.99f);

        // ---------------------- Legacy mouse picker (kinematic entity at y=5) ---------
        void SetMousePicker(entt::entity entity);
        entt::entity GetMousePicker() const;
        void UpdateMousePickerPosition(EntityManager& entityManager, const glm::vec3& worldPosition) const;

        // ---------------------- Spring picker (interactive drag) ----------------------
        // Cast a ray, pick the nearest particle within max_radius (world units, perpendicular to
        // the ray). Returns UINT32_MAX if nothing is hit. Forces a fresh CPU readback of positions.
        uint32_t PickParticleByRay(const glm::vec3& ray_origin, const glm::vec3& ray_dir,
                                   float max_radius);
        void SetPickTarget(const glm::vec3& target_world) { m_pick_target = target_world; }
        void ReleasePick() {
            m_picked_particle    = 0xFFFFFFFFu;
            m_picked_rigid_first = 0xFFFFFFFFu;
            m_picked_rigid_count = 0;
            m_picked_body_index  = 0xFFFFFFFFu;
        }
        bool HasPickedParticle() const { return m_picked_particle != 0xFFFFFFFFu; }
        glm::vec3 GetPickedParticleWorldPos() const { return m_picked_world_pos; }
        glm::vec3 GetPickTarget() const { return m_pick_target; }

        // Picker spring tuning (UI-exposed). For a free particle these act directly. For a
        // rigid body the same force is distributed across all members so the body acceleration
        // is mass-independent. Stiffness in N/m, damping in N·s/m. Increase to fight gravity
        // on heavier bodies — 4000 is enough to lift a 125-particle cube.
        float m_pickStiffness = 4000.0f;
        float m_pickDamping   = 60.0f;

        // ---------------------- Rigid body transforms (for mesh rendering) -----------
        struct RigidBodyTransform { glm::vec3 centroid; glm::quat orientation; };
        const std::vector<RigidBodyTransform>& GetRigidBodyTransforms() const { return m_rigid_transforms; }
        // Parallel array: m_rigid_body_ids[i] = the user-defined id for transforms[i].
        const std::vector<uint32_t>& GetRigidBodyIds() const { return m_rigid_body_ids; }

        // -------- Tunable parameters --------
        // Macklin et al. 2019 "Small Steps in Physics Simulation" — many small substeps
        // with few iterations per substep beats one big step with many iterations. With dt
        // small the constraint linearization is more accurate; energy drift collapses.
        int        m_substeps               = 4;  // per fixed timestep
        int        m_solverIterations       = 4;  // per substep
        glm::vec3  m_gravity                = { 0.0f, -9.81f, 0.0f };
        float      m_fluidInteractionRadius = 0.45f;
        float      m_particleRadius         = 0.10f;
        float      m_globalDamping          = 0.999f;
        // Cap per-particle speed to prevent tunneling through walls / other particles when a
        // wave-front accelerates faster than radius/dt. Use 0 to disable.
        float      m_maxParticleSpeed       = 25.0f;
        DomainBox  m_domain;

        // Chebyshev semi-iterative acceleration. rho is the spectral-radius estimate;
        // a value near 1 gives stronger acceleration but risks instability. 0.0 disables.
        // (Wang 2015, "A Chebyshev Semi-Iterative Approach for Accelerating PBD/PD")
        // Off by default — with substepping the convergence acceleration isn't needed and the
        // overshoot can add visible energy.
        bool       m_chebyshevEnabled = false;
        float      m_chebyshevRho     = 0.95f;

        // When false the per-step DtoH copy of positions to TransformComponents is skipped — used
        // by the renderer when it's drawing particles directly from the VBO (Particles-only mode).
        // Saves ~1ms per step on 20k-particle scenes and removes a CPU sync point.
        bool       m_syncTransforms = true;

        // PBF artificial pressure (Macklin & Müller 2013, Eq. 13). Counters tensile instability.
        float      m_pbfPressureK     = 0.005f;
        float      m_pbfDeltaQRatio   = 0.2f;  // Δq = ratio * h

        // Foam classifier — promotes only properly-splashing fluid particles ("detached or fast")
        // to foam so the sprite pass doesn't smear the whole surface white. Defaults are tuned
        // for a 0.08 m radius / h ≈ 0.32 m setup; raise minSpeed for calmer water, lower
        // lowNeighbours for more aggressive splash detection.
        float      m_foamMinSpeed       = 18.0f;  // higher threshold = less "too much white"
        float      m_foamMaxSpeed       = 16.0f;  // m/s; saturates the velocity score
        float      m_foamLowNeighbours  = 3.0f;   // truly detached droplets only

        // Rigid body damping (per-second exponential decay rates). Linear damping mirrors
        // particle global damping; angular damping is independent because rotational kinetic
        // energy is bookkept separately under Müller 2020 — without it, asymmetric contact
        // pumps energy into spin and the body wobbles indefinitely.
        float      m_rigidLinearDampingPerSec  = 0.30f;   // ~26%/s — gentle drag on translation
        float      m_rigidAngularDampingPerSec = 2.0f;    // ~86%/s — kills spurious spin fast

        // Distance-constraint compliance (XPBD α). 0 = perfectly stiff but slow to converge
        // through long chains (cloth/ropes need many iters at α=0). A tiny non-zero value
        // (~1e-7) lets the solver converge in fewer iterations without visible sag.
        float      m_distanceCompliance = 1.0e-7f;

        // Soft boundary push — proportional inward force inside a band along each wall.
        // Per-iteration strength; total push after K iters is roughly K * strength * h.
        // Kept small — even 0.02 gives ~0.16h total push per step, plenty to unstick fluid.
        float      m_boundaryPushStrength = 0.02f;

        // Vorticity confinement. 0 disables. INJECTS energy by design (PBF §6.2), so off by
        // default — turn it up if you want a swirly look, but keep it tiny.
        float      m_vorticityEpsilon = 0.0f;

        // Fixed timestep loop
        float       m_timeAccumulator = 0.0f;
        float       m_totalTime       = 0.0f;
        const float m_fixedTimeStep   = 1.0f / 60.0f;

        entt::entity m_mousePickerEntity = entt::null;

        // -------- Accessors --------
        uint32_t GetParticleCount()   const { return m_num_particles; }
        uint32_t GetMaterialCount()   const { return m_num_materials; }
        uint32_t GetConstraintCount() const { return m_num_constraints; }
        uint32_t GetRigidBodyCount()  const { return m_num_rigid_bodies; }
        GLuint   GetParticleVBO()     const { return m_particle_vbo; }
        GLuint   GetMaterialIdVBO()   const { return m_material_id_vbo; }
        GLuint   GetFlagsVBO()        const { return m_flags_vbo; }
        uint32_t GetMaterialIdCount() const { return m_num_particles; }
        const std::vector<PhysicsMaterial>& GetHostMaterials() const { return m_host_materials; }

    private:
        void InitializeGPUParticles(EntityManager& entityManager);
        void InitializeGPUConstraints(EntityManager& entityManager,
                                      const std::vector<entt::entity>& particle_entities);
        void InitializeRigidBodies(EntityManager& entityManager,
                                   const std::vector<entt::entity>& particle_entities);
        void ShutdownGPUResources();
        void SimulateStep(EntityManager& entityManager, float fixedDeltaTime);
        void SyncTransformsFromGPU(EntityManager& entityManager);
        void SyncRigidBodyTransformsFromGPU();

        // Picker state
        uint32_t   m_picked_particle = 0xFFFFFFFFu;
        glm::vec3  m_pick_target{0.0f};
        glm::vec3  m_picked_world_pos{0.0f};
        // If the picked particle is a rigid body member, these identify the body. Otherwise
        // UINT32_MAX. The body-level picker uses m_picked_body_index only; the legacy first/count
        // fields are kept around so external code (UI) can still query body extent.
        uint32_t   m_picked_rigid_first = 0xFFFFFFFFu;
        uint32_t   m_picked_rigid_count = 0;
        uint32_t   m_picked_body_index  = 0xFFFFFFFFu;

        // CPU-side lookup: row_index → rigid_body_index (or UINT32_MAX if free particle).
        std::vector<uint32_t> m_particle_to_rigid_index;

        // CPU-side cache of rigid-body transforms (updated after each step).
        std::vector<RigidBodyTransform> m_rigid_transforms;
        std::vector<uint32_t>           m_rigid_body_ids;

        CudaManager& m_cuda_manager;
        std::unique_ptr<CudaSpatialGrid> m_spatial_grid;

        // Render-side resources
        GLuint m_particle_vbo    = 0; // float3 positions, updated every step via interop
        GLuint m_material_id_vbo = 0; // uint32 material id per particle, written once at init
        GLuint m_flags_vbo       = 0; // uint32 per-particle flags (bit 0 = rigid body member)
        cudaGraphicsResource* m_cuda_vbo_resource    = nullptr;
        cudaGraphicsResource* m_cuda_matid_resource  = nullptr;

        // Counts
        uint32_t m_num_particles    = 0;
        uint32_t m_num_materials    = 0;
        uint32_t m_num_constraints  = 0;
        uint32_t m_num_rigid_bodies = 0;
        uint32_t m_num_rigid_slots  = 0;

        // SoA particle pool (device)
        glm::vec3* d_positions           = nullptr;
        glm::vec3* d_predicted_positions = nullptr;
        glm::vec3* d_velocities          = nullptr;
        float*     d_inverse_masses      = nullptr;
        uint32_t*  d_material_ids        = nullptr;
        // Per-particle static flag bits (phase + rigid-member), uploaded once at init. The
        // foam classifier ORs the foam bit on top of these each frame and writes the result
        // into the (interop'd, WriteDiscard) flags VBO.
        uint32_t*  d_static_flags        = nullptr;

        // Solver scratch (device)
        glm::vec3* d_position_deltas   = nullptr;
        glm::vec3* d_contact_normals   = nullptr;
        glm::vec3* d_chebyshev_prev    = nullptr;
        float*     d_density_lambdas   = nullptr;
        float*     d_distance_lambdas  = nullptr;

        // Materials + constraints (device)
        PhysicsMaterial*    d_materials            = nullptr;
        DistanceConstraint* d_distance_constraints = nullptr;

        // Rigid body storage (device)
        RigidBodyGPU* d_rigid_bodies            = nullptr;
        uint32_t*     d_rigid_particle_indices  = nullptr;
        glm::vec4*    d_rigid_rest_positions    = nullptr;
        // Slot index k → body index. Same length as d_rigid_particle_indices.
        uint32_t*     d_slot_to_body            = nullptr;
        // Particle index i -> body index (or UINT32_MAX if free). Same length as d_positions.
        uint32_t*     d_particle_to_body        = nullptr;

        // Host copy of materials (for renderer color lookup)
        std::vector<PhysicsMaterial> m_host_materials;

        // Particle row index -> ECS entity (for transform sync).
        std::vector<entt::entity> m_particle_entities;
    };
}
