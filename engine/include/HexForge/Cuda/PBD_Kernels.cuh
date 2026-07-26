#pragma once

#include <glm/glm.hpp>
#include <cstdint>

#include "HexForge/Physics/PhysicsMaterial.h"
#include "HexForge/Cuda/CudaSpatialGrid.h"

// =================================================================================================
// XPBD kernel suite. Each constraint kernel here corresponds to a published method. The list:
//
//   density (PBF)         [Macklin & Müller 2013]  — incompressibility for fluids
//                                                    Eq. 9 (constraint), Eq. 11 (λ), Eq. 13–14 (s_corr)
//   distance constraint   [Müller et al. 2007]     — soft-bodies / cloth / ropes
//                                                    (XPBD form, Macklin 2016, Eq. 4–7)
//   particle contact      [PBDSurvey17 §4.4]       — non-penetration spheres + Coulomb friction
//   shape matching        [Müller et al. 2005]     — rigid bodies as particle clusters
//                                                    iterative APD polar decomposition (Müller 2016)
//   boundary push         [PBF 2013 §5]            — fluid-wall stickiness fix (band-shaped delta)
//   vorticity confinement [PBF 2013 §6.2]          — recovers angular motion lost to Jacobi damping
//   XSPH viscosity        [Schechter & Bridson 2012;
//                          PBF 2013 §6.1]          — velocity smoothing
//   Chebyshev outer accel [Wang 2015]              — non-stationary semi-iterative acceleration
//
// The overall step structure follows [Macklin et al. 2019, "Small Steps in Physics Simulation"]:
// many small substeps × few iterations beats one big step × many iterations.
// =================================================================================================
namespace Hex
{
    struct DistanceConstraint
    {
        uint32_t p1_idx;
        uint32_t p2_idx;
        float    rest_length;
    };

    struct DomainBox
    {
        glm::vec3 min{ -10.f, -1.5f, -10.f };
        glm::vec3 max{  10.f, 100.f,  10.f };
    };

    // -------------------------------------------------------------------------
    // Time integration
    // -------------------------------------------------------------------------
    void launch_predictPositions(
        glm::vec3* d_positions, glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
        const float* d_inverse_masses, const uint32_t* d_material_ids,
        const PhysicsMaterial* d_materials,
        uint32_t num_particles, const glm::vec3& gravity, float dt);

    // Restitution-aware velocity update using captured per-particle contact normals.
    // d_contact_normals is the (unnormalised) sum of contact directions accumulated during the
    // solver iterations; magnitude doubles as a contact-confidence weight.
    void launch_updateVelocities(
        glm::vec3* d_velocities, const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const glm::vec3* d_contact_normals,
        uint32_t num_particles, float inv_dt, float global_damping);

    // -------------------------------------------------------------------------
    // Solver bookkeeping
    // -------------------------------------------------------------------------
    void launch_resetSolverData(
        glm::vec3* d_position_deltas, float* d_lambdas, uint32_t num_particles);

    void launch_zeroVec3(glm::vec3* d_buffer, uint32_t n);

    // Clamp |v_i| to max_speed. Prevents tunneling at the leading edge of a fast wave.
    void launch_clampVelocities(glm::vec3* d_velocities, uint32_t n, float max_speed);

    // Spring picker — pulls one specific particle toward a target world position with a
    // critically-damped spring of stiffness k and damping c. Applied as a velocity impulse:
    //   v += dt * ( k * (target - p) - c * v )
    // Stable as long as k*dt < ~10 (use small dt). Caller writes index = UINT32_MAX to disable.
    void launch_applyPickerSpring(
        glm::vec3* d_velocities, const glm::vec3* d_predicted_positions,
        uint32_t picked_index, glm::vec3 target, float stiffness, float damping, float dt);

    // (Rigid-body picker now uses launch_applyPickerSpringBody — see rigid body section.)

    // Foam / spray classification (Ihmsen et al. 2012, adapted). For each fluid particle the
    // kernel computes a "splashiness" score from (a) velocity magnitude — fast-moving particles
    // are spray, and (b) trapped-air estimate from neighbour density gradient. Particles above
    // threshold get bit 3 set in the flags VBO, which the foam render pass reads to draw an
    // additive sprite. Bits 0/1/2/4 (rigid-member, fluid, cloth, rigid-phase) are preserved.
    void launch_classifyFoam(
        uint32_t* d_flags_vbo,                         // mapped GL buffer (WriteDiscard)
        const uint32_t* d_static_flags,                // persistent base flags
        const glm::vec3* d_velocities,
        const glm::vec3* d_predicted_positions,
        const uint32_t* d_material_ids,
        const PhysicsMaterial* d_materials,
        const CudaSpatialGrid* h_grid,
        uint32_t num_particles, float interaction_radius,
        float min_speed_for_foam, float max_speed_full_foam,
        float low_neighbour_count);

    void launch_applyDeltas(
        glm::vec3* d_predicted_positions, const glm::vec3* d_position_deltas,
        const float* d_inverse_masses, uint32_t num_particles);

    // Chebyshev semi-iterative acceleration of the outer Jacobi loop:
    //   x_{k+1} = omega * (y_k - x_{k-1}) + x_{k-1}
    // Stores x_k back into d_prev for the next iteration's blend.
    void launch_chebyshevBlend(
        glm::vec3* d_current, glm::vec3* d_prev,
        float omega, uint32_t num_particles);

    // -------------------------------------------------------------------------
    // Constraint solves
    // -------------------------------------------------------------------------
    // SOTA: 2-Pass PBF Solver for maximum stability in parallel environments.
    // calculateLambdas computes pressure without updating movement.
    void launch_calculateLambdas(
        const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const CudaSpatialGrid* h_grid, float* d_density_lambdas,
        uint32_t num_particles, float interaction_radius, float dt);

    // applyDensityCorrections applies movement based on the pressure from Pass 1.
    void launch_applyDensityCorrections(
        const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const CudaSpatialGrid* h_grid, glm::vec3* d_position_deltas,
        const float* d_density_lambdas, uint32_t num_particles,
        float interaction_radius, float k_pressure, float delta_q_ratio);

    // Soft boundary force — pushes fluid particles inward when they enter the (h-thick) outer
    // layer of the domain. Without this, fluid density is artificially deficient near walls,
    // C<0, no constraint applies and the fluid sticks to the boundary. PBF 2013 §5.
    void launch_solveBoundaryPush(
        const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        glm::vec3* d_position_deltas,
        uint32_t num_particles, DomainBox domain, float h, float strength);

    void launch_solveParticleContacts(
        const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
        const float* d_inverse_masses,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const uint32_t* d_particle_to_body,
        const CudaSpatialGrid* h_grid, glm::vec3* d_position_deltas,
        glm::vec3* d_contact_normals,
        uint32_t num_particles, float particle_radius);

    void launch_solveDistanceConstraint(
        const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
        const DistanceConstraint* d_constraints, glm::vec3* d_position_deltas, float* d_distance_lambdas,
        uint32_t num_constraints, float compliance, float dt);

    void launch_solveDomainCollision(
        const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        glm::vec3* d_position_deltas, glm::vec3* d_contact_normals,
        uint32_t num_particles, DomainBox domain, float particle_radius);

    // -------------------------------------------------------------------------
    // Rigid bodies — Müller et al. 2020, "Detailed Rigid Body Simulation with
    // Extended Position Based Dynamics". A rigid body has its own (x, q, v, ω) and
    // an inertia tensor; per-particle "members" are *kinematic* — their position
    // each step is determined entirely by the body's transform. Particle-level
    // constraints (contact, fluid coupling, distance) produce per-particle Δp;
    // those are aggregated back into Δx_cm and Δθ for the body via mass-weighted
    // centroid and torque (r × m·Δp), with Δθ = I⁻¹_world · Στ. After aggregation
    // we re-warp particles to the corrected body frame.
    //
    // PreSolve:
    //   v ← (v + g·dt) · lin_damping
    //   prev_x ← x;      x ← x + v·dt
    //   ω ← ω · ang_damping
    //   prev_q ← q;      q ← normalize(q + 0.5·dt·ω·q)
    //
    // Aggregate (each iter):
    //   Σm_i·Δp_i, Σ(r_i × m_i·Δp_i)
    //   Δx ← Σm·Δp / m_total
    //   Δθ ← I⁻¹_world · Στ
    //   x += Δx;         q ← normalize(rotate(Δθ)·q)
    //   re-warp particles
    //
    // PostSolve:
    //   v ← (x − prev_x)/dt
    //   ω ← 2·(q·prev_q⁻¹).xyz / dt   (negate if .w < 0 for shortest-arc)
    //
    // Memory layout below is std140-aligned; glm::vec4 fields keep the struct
    // packed in 16-byte slots so host and device agree without padding asserts.
    struct RigidBodyGPU
    {
        uint32_t first_particle;   // offset into d_rigid_particle_indices
        uint32_t particle_count;
        float    total_inv_mass;   // 1 / m_total  (0 = kinematic / pinned)
        float    stiffness;        // unused under Müller-2020 (kept for ABI); body is hard-rigid

        glm::vec4 position;          // .xyz = current centroid in world space
        glm::vec4 prev_position;     // .xyz = position at start of substep
        glm::vec4 orientation;       // current world-space quaternion (x,y,z,w)
        glm::vec4 prev_orientation;  // quaternion at start of substep

        glm::vec4 linear_velocity;   // .xyz = v_lin (world)
        glm::vec4 angular_velocity;  // .xyz = ω (world)

        // Inverse inertia tensor in body-local rest frame (3 columns, glm::mat3 conv.)
        glm::vec4 inv_inertia_col0;
        glm::vec4 inv_inertia_col1;
        glm::vec4 inv_inertia_col2;

        glm::vec4 _pad;              // reserved for future use; keeps total size 192 B
    };

    // Per-substep predict on rigid bodies: integrate v, ω with gravity and damping.
    // lin_damping and ang_damping are *per-substep* multipliers (0.999 ≈ gentle).
    void launch_predictRigidBodies(
        RigidBodyGPU* d_rigid_bodies, uint32_t num_rigid_bodies,
        glm::vec3 gravity, float dt,
        float lin_damping, float ang_damping);

    // Write rigid-member particle predicted_positions = q·rest + x. Called after
    // predictRigidBodies and after each aggregateRigidCorrections.
    void launch_warpParticlesToBody(
        glm::vec3* d_predicted_positions,
        const uint32_t* d_rigid_particle_indices,
        const glm::vec4* d_rigid_rest_positions,
        const uint32_t* d_slot_to_body,
        const RigidBodyGPU* d_rigid_bodies,
        uint32_t num_rigid_slots);

    // Project per-particle position_deltas onto the body's (x_cm, q). One thread per
    // body. Zeroes the slot deltas of its members afterwards so subsequent
    // applyDeltas doesn't double-apply (we'll re-warp instead).
    void launch_aggregateRigidCorrections(
        const glm::vec3* d_predicted_positions,
        glm::vec3* d_position_deltas,
        const float* d_inverse_masses,
        const uint32_t* d_rigid_particle_indices,
        RigidBodyGPU* d_rigid_bodies,
        uint32_t num_rigid_bodies);

    // After all iters: derive v_lin, ω from position / quaternion delta. Also kicks
    // per-particle velocities to (v_lin + ω × r_world) for fluid coupling.
    void launch_finalizeRigidBodies(
        glm::vec3* d_velocities,
        const glm::vec3* d_predicted_positions,
        const uint32_t* d_rigid_particle_indices,
        const uint32_t* d_slot_to_body,
        RigidBodyGPU* d_rigid_bodies,
        uint32_t num_rigid_bodies,
        uint32_t num_rigid_slots,
        float inv_dt);

    // Rigid-body picker spring — applies F = k·(target − handle_world) − c·v_handle
    // as a force at the picked particle's current world position, producing both a
    // linear impulse on v_lin and a torque (r × F) on ω. Mass-aware via the body's
    // inertia tensor, so a heavy bunny needs more spring stiffness than a light cube.
    void launch_applyPickerSpringBody(
        RigidBodyGPU* d_rigid_bodies, uint32_t body_index,
        const glm::vec3* d_predicted_positions,
        uint32_t picked_index, glm::vec3 target,
        float stiffness, float damping, float dt);

    // -------------------------------------------------------------------------
    // Post-step velocity treatments
    // -------------------------------------------------------------------------
    // XSPH viscosity (Schechter & Bridson 2012; PBF 2013 §6.1).
    void launch_applyXSPHViscosity(
        const glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const CudaSpatialGrid* h_grid, uint32_t num_particles, float interaction_radius);

    // Vorticity confinement (PBF 2013 §6.2). Recovers angular motion the XPBD/Jacobi solver
    // damps out, giving fluids a livelier "alive" look. eps controls intensity.
    void launch_applyVorticityConfinement(
        const glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
        const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
        const CudaSpatialGrid* h_grid,
        uint32_t num_particles, float interaction_radius, float dt, float epsilon);
}
