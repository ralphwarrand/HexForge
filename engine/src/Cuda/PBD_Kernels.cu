#include "HexForge/Cuda/PBD_Kernels.cuh"
#include "HexForge/Cuda/KernelMath.cuh"
#include "HexForge/Cuda/CudaSpatialGrid.h"
#include "HexForge/Cuda/GridHash.cuh"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

namespace Hex
{

namespace
{
    constexpr int kBlock = 256;
    constexpr uint32_t kEmptyCell = 0xFFFFFFFFu;
    inline int Blocks(uint32_t n) { return static_cast<int>((n + kBlock - 1) / kBlock); }

    __device__ inline void atomicAddVec3(glm::vec3* address, const glm::vec3& v)
    {
        atomicAdd(&address->x, v.x);
        atomicAdd(&address->y, v.y);
        atomicAdd(&address->z, v.z);
    }

    __device__ inline bool IsFluid(uint32_t phase)  { return phase == static_cast<uint32_t>(ParticlePhase::Fluid); }
    __device__ inline bool IsStatic(uint32_t phase) { return phase == static_cast<uint32_t>(ParticlePhase::Static); }
    __device__ inline bool IsRigid(uint32_t phase)  { return phase == static_cast<uint32_t>(ParticlePhase::Rigid); }
    __device__ inline bool IsCloth(uint32_t phase)  { return phase == static_cast<uint32_t>(ParticlePhase::Cloth); }
    // "Boundary" — non-fluid solid stuff that contributes to fluid density estimation so fluid
    // can't pass through it. (Bender et al. 2014 "Boundary handling and adaptive time-stepping
    // for PCISPH" formalises this; here we just add their density contribution.)
    __device__ inline bool IsFluidBoundary(uint32_t phase)
    {
        return phase == static_cast<uint32_t>(ParticlePhase::Rigid) ||
               phase == static_cast<uint32_t>(ParticlePhase::Static) ||
               phase == static_cast<uint32_t>(ParticlePhase::Solid);
    }
}

// =============================================================================
// PREDICT
// =============================================================================
__global__ void predictPositions_kernel(
    glm::vec3* positions, glm::vec3* predicted_positions, glm::vec3* velocities,
    const float* inverse_masses, const uint32_t* material_ids,
    const PhysicsMaterial* materials,
    uint32_t num_particles, glm::vec3 gravity, float dt)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    positions[i] = predicted_positions[i];

    const float w = inverse_masses[i];
    const uint32_t phase = materials[material_ids[i]].phase;
    if (w <= 0.0f || IsStatic(phase)) {
        velocities[i] = glm::vec3(0.0f);
        return;
    }
    velocities[i] += gravity * dt;
    predicted_positions[i] = positions[i] + velocities[i] * dt;
}

// =============================================================================
// BOOKKEEPING
// =============================================================================
__global__ void resetSolverData_kernel(glm::vec3* deltas, float* lambdas, uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    if (deltas)  deltas[i]  = glm::vec3(0.0f);
    if (lambdas) lambdas[i] = 0.0f;
}

__global__ void zeroVec3_kernel(glm::vec3* buf, uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    buf[i] = glm::vec3(0.0f);
}

__global__ void applyDeltas_kernel(
    glm::vec3* predicted_positions, const glm::vec3* position_deltas,
    const float* inverse_masses, uint32_t num_particles)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;
    if (inverse_masses[i] <= 0.0f) return;
    predicted_positions[i] += position_deltas[i];
}

__global__ void chebyshev_blend_kernel(
    glm::vec3* current, glm::vec3* prev, float omega, uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    glm::vec3 c = current[i];
    glm::vec3 p = prev[i];
    current[i] = omega * (c - p) + p;
    prev[i] = c;
}

// =============================================================================
// DENSITY (XPBD fluid) + PBF artificial pressure (s_corr) for tensile instability.
// Macklin & Müller 2013, "Position Based Fluids", Eq. 13-14.
//
//   s_corr = -k * (W(r_ij, h) / W(Δq, h))^n         (Δq = δ * h, typical δ=0.2, n=4)
//   Δp_i  = (1/ρ_0) Σ_j (λ_i + λ_j + s_corr) ∇W_ij
//
// We pack s_corr into the per-pair correction so it nudges clustered particles apart even
// when the density estimate alone wouldn't trigger a correction. This eliminates the famous
// "particle string" artifacts in free surfaces.
// =============================================================================
// --- SOTA: PASS 1: Calculate Lambdas (Pressure) ---
__global__ void calculateLambdas_kernel(
    const glm::vec3* predicted_positions,
    const float* inverse_masses,
    const uint32_t* material_ids,
    const PhysicsMaterial* materials,
    CudaSpatialGridView grid,
    float* lambdas,
    uint32_t num_particles,
    float h, float dt)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles || inverse_masses[i] < 1e-9f) return;

    const auto& mat_i = materials[material_ids[i]];
    if (!IsFluid(mat_i.phase)) return;

    const float rest_density = mat_i.rest_density;
    const float h2 = h * h;
    const glm::vec3 pos_i = predicted_positions[i];
    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);

    float density = 0.0f;
    float sum_grad_sq = 0.0f;
    glm::vec3 grad_i(0.0f);

    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            const auto& mat_j = materials[material_ids[j]];
            bool same_fluid = IsFluid(mat_j.phase) && (mat_j.rest_density == rest_density);
            bool is_boundary = IsFluidBoundary(mat_j.phase);
            if (!same_fluid && !is_boundary) continue;

            const glm::vec3 r = pos_i - predicted_positions[j];
            const float d2 = glm::dot(r, r);
            if (d2 >= h2) continue;

            density += KernelMath::Poly6(r, h);
            
            if (i != j) {
                const glm::vec3 g = KernelMath::SpikyGradient(r, h) / rest_density;
                grad_i += g;
                if (same_fluid) sum_grad_sq += glm::dot(g, g);
            }
        }
    }
    sum_grad_sq += glm::dot(grad_i, grad_i);

    const float C = fmaxf(0.0f, (density / rest_density) - 1.0f);
    const float alpha_tilde = mat_i.compliance / (dt * dt);
    // XPBD Lambda update: only solve if there is actually pressure (C > 0)
    if (C <= 0.0f) {
        lambdas[i] = 0.0f;
        return;
    }
    lambdas[i] = (-C - alpha_tilde * lambdas[i]) / (sum_grad_sq + alpha_tilde + 1e-6f);
}

// --- SOTA: PASS 2: Apply Corrections (Movement) ---
__global__ void applyDensityCorrections_kernel(
    const glm::vec3* predicted_positions,
    const float* inverse_masses,
    const uint32_t* material_ids,
    const PhysicsMaterial* materials,
    CudaSpatialGridView grid,
    glm::vec3* position_deltas,
    const float* lambdas,
    uint32_t num_particles,
    float h, float k_pressure, float delta_q_ratio)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles || inverse_masses[i] < 1e-9f) return;

    const auto& mat_i = materials[material_ids[i]];
    if (!IsFluid(mat_i.phase)) return;

    const float rest_density = mat_i.rest_density;
    const float h2 = h * h;
    const float lambda_i = lambdas[i];
    const glm::vec3 pos_i = predicted_positions[i];
    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);

    // Reference Poly6 value at Δq = δ * h — denominator of s_corr.
    const glm::vec3 ref_offset(delta_q_ratio * h, 0.0f, 0.0f);
    const float W_dq = KernelMath::Poly6(ref_offset, h);
    const float inv_W_dq = (W_dq > 1e-12f) ? (1.0f / W_dq) : 0.0f;

    glm::vec3 delta_p(0.0f);

    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;

            const auto& mat_j = materials[material_ids[j]];
            bool same_fluid  = IsFluid(mat_j.phase) && (mat_j.rest_density == rest_density);
            bool is_boundary = IsFluidBoundary(mat_j.phase);
            if (!same_fluid && !is_boundary) continue;

            const glm::vec3 r = pos_i - predicted_positions[j];
            const float d2 = glm::dot(r, r);
            if (d2 >= h2) continue;
            const glm::vec3 g = KernelMath::SpikyGradient(r, h) / rest_density;

            if (same_fluid) {
                float s_corr = 0.0f;
                if (k_pressure > 0.0f && inv_W_dq > 0.0f) {
                    float ratio = KernelMath::Poly6(r, h) * inv_W_dq;
                    float r2 = ratio * ratio;
                    s_corr = -k_pressure * r2 * r2;
                }
                delta_p += (lambda_i + lambdas[j] + s_corr) * g;
            } else {
                delta_p += lambda_i * g;
            }
        }
    }

    // Safety clamp: a single density projection may never move a particle more than a quarter
    // of the smoothing radius in one iteration. If two particles are momentarily near-coincident
    // the Spiky gradient diverges and the unclamped correction would eject the particle at
    // escape velocity — the visible "explosion". Bounding it keeps a bad frame recoverable.
    const float max_dp  = 0.25f * h;
    const float dp_len2 = dot(delta_p, delta_p);
    if (dp_len2 > max_dp * max_dp) delta_p *= max_dp * rsqrtf(dp_len2);

    atomicAddVec3(&position_deltas[i], delta_p);
}

// =============================================================================
// PARTICLE CONTACT (solid/rigid/static) — now also accumulates contact normals
// =============================================================================
__global__ void solveParticleContacts_kernel(
    const glm::vec3* prev_positions, const glm::vec3* predicted_positions,
    const float* inverse_masses,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    const uint32_t* particle_to_body,
    CudaSpatialGridView grid,
    glm::vec3* position_deltas, glm::vec3* contact_normals,
    uint32_t num_particles, float radius)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const PhysicsMaterial& mat_i = materials[material_ids[i]];
    const bool i_fluid  = IsFluid(mat_i.phase);
    const bool i_static = IsStatic(mat_i.phase);
    const bool i_cloth  = IsCloth(mat_i.phase);

    const float w_i = inverse_masses[i];
    // Fluid particles are allowed in — but only to do hard non-penetration against rigid/static
    // neighbours (so water can't clip through a bunny). Fluid-fluid is still handled by PBF.
    if (w_i <= 0.0f && !i_static) return;

    const glm::vec3 pos_i = predicted_positions[i];
    const glm::vec3 prev_i = prev_positions[i];
    const uint32_t body_i = particle_to_body ? particle_to_body[i] : 0xFFFFFFFFu;

    // Minimal inflation to prevent tunneling. 
    // 1.05/1.01 were too bouncy; 1.001 is much more stable.
    const float diameter = 2.0f * radius * 1.001f;
    const float diameter_sq = diameter * diameter;

    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);

    // --- gather corrections ---
    // Instead of symmetric updates with j > i, we iterate ALL neighbors and only
    // update OUR OWN delta. This is the "Gather" pattern that makes GPU solvers fast.
    glm::vec3 delta_p(0.0f);
    glm::vec3 normal_sum(0.0f);

    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;

            const uint32_t body_j = particle_to_body ? particle_to_body[j] : 0xFFFFFFFFu;
            if (body_i != 0xFFFFFFFFu && body_j == body_i) continue;

            const PhysicsMaterial& mat_j = materials[material_ids[j]];
            const bool j_fluid = IsFluid(mat_j.phase);
            const bool j_cloth = IsCloth(mat_j.phase);

            // Skip categories handled elsewhere or where contact would fight another solver:
            //   fluid-fluid     — PBF density constraint
            //   cloth-cloth     — distance constraint graph
            //   fluid-cloth     — cloth too thin to be a barrier; let fluid pass
            if (i_fluid && j_fluid) continue;
            if (i_cloth && j_cloth) continue;
            if ((i_fluid && j_cloth) || (i_cloth && j_fluid)) continue;

            const glm::vec3 r = pos_i - predicted_positions[j];
            const float dist_sq = glm::dot(r, r);
            if (dist_sq >= diameter_sq || dist_sq < 1e-12f) continue;

            const float dist = sqrtf(dist_sq);
            const glm::vec3 n = r / dist;
            const float penetration = diameter - dist;

            const float w_j = inverse_masses[j];
            const float w_sum = w_i + w_j;
            if (w_sum < 1e-9f) continue;

            // For fluid-solid pairs we want the fluid to take essentially all of the correction
            // (water yields, the bunny doesn't). The standard mass-weighted XPBD split does the
            // right thing when masses are equal, but here we want a bias: the rigid receives
            // only a small fraction so it doesn't get jostled by every droplet.
            float push_i = w_i;
            float push_j = w_j;
            const bool mixed_fluid = (i_fluid != j_fluid);
            if (mixed_fluid) {
                if (i_fluid) { push_i = w_i; push_j = w_j * 0.1f; }
                else         { push_i = w_i * 0.1f; push_j = w_j; }
            }
            const float push_sum = push_i + push_j;
            if (push_sum < 1e-9f) continue;
            
            // Jacobi under-relaxation. For fluid-fluid we use 0.5 to prevent explosions.
            // But for ANY collision with a Static/Rigid boundary, we want 1.0 (Full Projection)
            // to ensure no sinking or tunneling occurs.
            const bool i_static = (w_i < 1e-9f);
            const bool j_static = (w_j < 1e-9f);
            const bool is_boundary_collision = i_static || j_static || (body_i != 0xFFFFFFFFu) || (body_j != 0xFFFFFFFFu);
            
            const float kJacobiRelax = is_boundary_collision ? 1.0f : 0.5f;
            const float kInflation   = is_boundary_collision ? 1.05f : 1.001f;
            
            float eff_diameter = 2.0f * radius * kInflation;
            float eff_pen = eff_diameter - dist;
            
            if (eff_pen > 0.0f) {
                delta_p += (eff_pen / push_sum) * push_i * n * kJacobiRelax;
            }

            if (!i_fluid && !j_fluid) {
                normal_sum += penetration * n;
                const float mu = 0.5f * (mat_i.friction + mat_j.friction);
                if (mu > 0.0f) {
                    const glm::vec3 dx_rel = (pos_i - prev_i) - (predicted_positions[j] - prev_positions[j]);
                    const glm::vec3 dx_t = dx_rel - glm::dot(dx_rel, n) * n;
                    const float t_len = glm::length(dx_t);
                    if (t_len > 1e-6f) {
                        const float clamp_t = fminf(t_len, mu * penetration);
                        delta_p -= (dx_t / t_len) * (clamp_t * w_i / w_sum);
                    }
                }
            }
        }
    }

    if (glm::dot(delta_p, delta_p) > 0.0f) atomicAddVec3(&position_deltas[i], delta_p);
    if (glm::dot(normal_sum, normal_sum) > 0.0f) atomicAddVec3(&contact_normals[i], normal_sum);
}

// =============================================================================
// DISTANCE CONSTRAINT
// =============================================================================
__global__ void solveDistanceConstraint_kernel(
    const glm::vec3* predicted_positions, const float* inverse_masses,
    const DistanceConstraint* constraints,
    glm::vec3* position_deltas, float* lambdas,
    uint32_t num_constraints, float compliance, float dt)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_constraints) return;

    const DistanceConstraint c = constraints[i];
    const float w1 = inverse_masses[c.p1_idx];
    const float w2 = inverse_masses[c.p2_idx];
    const float w_sum = w1 + w2;
    if (w_sum < 1e-9f) return;

    glm::vec3 n = predicted_positions[c.p2_idx] - predicted_positions[c.p1_idx];
    const float dist = length(n);
    if (dist < 1e-9f) return;
    n /= dist;

    const float C = dist - c.rest_length;
    const float alpha_tilde = compliance / (dt * dt);
    const float delta_lambda = (-C - alpha_tilde * lambdas[i]) / (w_sum + alpha_tilde);
    lambdas[i] += delta_lambda;

    // Jacobi under-relaxation. A cloth-grid particle is involved in 4-8 distance constraints; if
    // every one of them dumps its full lambda·grad into the same position_deltas atom in one
    // iter, the sum overshoots far past the correct position and the cloth either explodes or
    // oscillates. Scaling each contribution by 0.5 means a single-neighbour constraint (rope
    // bead) converges in 2 iters and a heavily-connected cloth node converges in ~4 iters —
    // exactly the convergence the small-step substepping schedule is designed for.
    const float kJacobiRelax = 0.5f;
    atomicAddVec3(&position_deltas[c.p1_idx], -w1 * delta_lambda * n * kJacobiRelax);
    atomicAddVec3(&position_deltas[c.p2_idx],  w2 * delta_lambda * n * kJacobiRelax);
}

// =============================================================================
// DOMAIN COLLISION — half-space projection per axis, writes wall push as a Δp into
// position_deltas (so rigid bodies pick it up through their aggregator) plus the
// wall normal into contact_normals (used by velocity update for restitution).
// =============================================================================
__global__ void solveDomainCollision_kernel(
    const glm::vec3* prev_positions, const glm::vec3* predicted_positions,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    glm::vec3* position_deltas, glm::vec3* contact_normals,
    uint32_t num_particles, DomainBox domain, float radius)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const PhysicsMaterial& mat = materials[material_ids[i]];
    if (IsStatic(mat.phase)) return;

    glm::vec3 p = predicted_positions[i];
    const glm::vec3 p_in = p;
    glm::vec3 accumulated_normal(0.0f);

    // Per axis, half-space projection. Push the particle out and stamp the wall normal.
    // Friction is then applied in proper Coulomb form below — dragging tangential motion
    // by min(|Δx_t|, μ·penetration_total), which keeps the drag bounded and physical.
    const glm::vec3 prev = prev_positions[i];

    auto resolve = [&](float& p_axis, float lo, float hi,
                       glm::vec3 normal_pos, glm::vec3 normal_neg) {
        if (p_axis < lo) {
            float pen = lo - p_axis;
            p_axis = lo;
            accumulated_normal += pen * normal_pos;
        } else if (p_axis > hi) {
            float pen = p_axis - hi;
            p_axis = hi;
            accumulated_normal += pen * normal_neg;
        }
    };

    resolve(p.x, domain.min.x + radius, domain.max.x - radius,
            { 1.f, 0.f, 0.f }, { -1.f, 0.f, 0.f });
    resolve(p.y, domain.min.y + radius, domain.max.y - radius,
            { 0.f, 1.f, 0.f }, { 0.f, -1.f, 0.f });
    resolve(p.z, domain.min.z + radius, domain.max.z - radius,
            { 0.f, 0.f, 1.f }, { 0.f, 0.f, -1.f });

    // Coulomb friction against the *sum* of penetrations encountered this step.
    float total_pen_sq = dot(accumulated_normal, accumulated_normal);
    if (total_pen_sq > 1e-12f && mat.friction > 0.0f) {
        float inv_pen = rsqrtf(total_pen_sq);
        glm::vec3 n   = accumulated_normal * inv_pen;
        float pen     = total_pen_sq * inv_pen;          // |Σ pen·n_i|
        glm::vec3 dx_rel = (p - prev);
        glm::vec3 dx_t   = dx_rel - dot(dx_rel, n) * n;
        float t_len      = length(dx_t);
        if (t_len > 1e-6f) {
            float clamp_t = fminf(t_len, mat.friction * pen);
            p -= (dx_t / t_len) * clamp_t;
        }
    }

    // Emit the wall correction as a position delta. 
    // Set to 1.0 for solid walls to prevent sinking/falling through.
    const float kWallRestitution = 1.0f;
    const float kJacobiRelax = 1.0f; // Full projection for domain walls
    glm::vec3 dp = (p - p_in) * kWallRestitution * kJacobiRelax;
    if (dot(dp, dp) > 0.0f) {
        position_deltas[i] += dp;
    }
    if (total_pen_sq > 0.0f) {
        contact_normals[i] = contact_normals[i] + accumulated_normal;
    }
}

// =============================================================================
// RIGID BODIES — Müller et al. 2020, Extended Position-Based Rigid Body Dynamics
// =============================================================================
__device__ inline glm::vec4 quat_mul(const glm::vec4& a, const glm::vec4& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

__device__ inline glm::vec4 quat_normalize(const glm::vec4& q)
{
    float n = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n < 1e-9f) return { 0.f, 0.f, 0.f, 1.f };
    return q / n;
}

__device__ inline glm::vec4 quat_conj(const glm::vec4& q) { return { -q.x, -q.y, -q.z, q.w }; }

__device__ inline glm::vec3 quat_rotate(const glm::vec4& q, const glm::vec3& v)
{
    glm::vec3 u(q.x, q.y, q.z);
    float s = q.w;
    return 2.0f * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0f * s * cross(u, v);
}

__device__ inline glm::mat3 quat_to_mat3(const glm::vec4& q)
{
    float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
    glm::mat3 R;
    R[0] = { 1.f - 2.f*(yy + zz),       2.f*(xy + wz),       2.f*(xz - wy) };
    R[1] = {       2.f*(xy - wz), 1.f - 2.f*(xx + zz),       2.f*(yz + wx) };
    R[2] = {       2.f*(xz + wy),       2.f*(yz - wx), 1.f - 2.f*(xx + yy) };
    return R;
}

__device__ inline glm::mat3 body_inv_inertia_world(const RigidBodyGPU& b)
{
    glm::mat3 I_inv_local;
    I_inv_local[0] = glm::vec3(b.inv_inertia_col0);
    I_inv_local[1] = glm::vec3(b.inv_inertia_col1);
    I_inv_local[2] = glm::vec3(b.inv_inertia_col2);
    glm::mat3 R = quat_to_mat3(b.orientation);
    return R * I_inv_local * glm::transpose(R);
}

// ---- Pre-substep: integrate v, ω with gravity and damping ------------------
__global__ void predictRigidBodies_kernel(
    RigidBodyGPU* bodies, uint32_t num_bodies,
    glm::vec3 gravity, float dt,
    float lin_damping, float ang_damping)
{
    uint32_t b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= num_bodies) return;
    RigidBodyGPU& body = bodies[b];

    body.prev_position    = body.position;
    body.prev_orientation = body.orientation;

    if (body.total_inv_mass <= 0.0f) {
        // Kinematic — pinned in place but still kept consistent.
        body.linear_velocity  = glm::vec4(0.0f);
        body.angular_velocity = glm::vec4(0.0f);
        return;
    }

    // Linear: damped, then add gravity impulse, then integrate.
    glm::vec3 v = glm::vec3(body.linear_velocity) * lin_damping + gravity * dt;
    glm::vec3 x = glm::vec3(body.position) + v * dt;
    body.linear_velocity = glm::vec4(v, 0.0f);
    body.position        = glm::vec4(x, 0.0f);

    // Angular: damp ω, integrate q with quaternion derivative q̇ = 0.5·ω·q.
    glm::vec3 omega = glm::vec3(body.angular_velocity) * ang_damping;
    glm::vec4 q = body.orientation;
    glm::vec4 omega_q{ omega.x, omega.y, omega.z, 0.0f };
    glm::vec4 dq    = 0.5f * dt * quat_mul(omega_q, q);
    body.orientation      = quat_normalize(q + dq);
    body.angular_velocity = glm::vec4(omega, 0.0f);
}

// ---- Each iter (and post-aggregate): warp rigid particles to (q·rest + x) --
__global__ void warpParticlesToBody_kernel(
    glm::vec3* predicted_positions,
    const uint32_t* rigid_particle_indices,
    const glm::vec4* rigid_rest_positions,
    const uint32_t* slot_to_body,
    const RigidBodyGPU* bodies,
    uint32_t num_rigid_slots)
{
    uint32_t k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_rigid_slots) return;
    const uint32_t pi = rigid_particle_indices[k];
    const uint32_t b  = slot_to_body[k];
    const RigidBodyGPU& body = bodies[b];
    const glm::vec3 rest = glm::vec3(rigid_rest_positions[k]);
    predicted_positions[pi] = quat_rotate(body.orientation, rest) + glm::vec3(body.position);
}

// ---- Aggregate particle Δp into body Δx_cm and Δθ, then update q, x --------
// One thread per body. Reads d_position_deltas (the accumulated correction from this iter's
// constraint solves), projects it onto the body's rigid-body dofs, and clears the slot
// deltas so the next applyDeltas pass (for free particles) doesn't re-apply them.
__global__ void aggregateRigidCorrections_kernel(
    const glm::vec3* predicted_positions,
    glm::vec3* position_deltas,
    const float* inverse_masses,
    const uint32_t* rigid_particle_indices,
    RigidBodyGPU* bodies,
    uint32_t num_bodies)
{
    uint32_t b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= num_bodies) return;
    RigidBodyGPU& body = bodies[b];
    const uint32_t first = body.first_particle;
    const uint32_t count = body.particle_count;
    if (count == 0 || body.total_inv_mass <= 0.0f) {
        // Still need to clear slot deltas so they don't leak into the next iter.
        for (uint32_t k = 0; k < count; ++k)
            position_deltas[rigid_particle_indices[first + k]] = glm::vec3(0.0f);
        return;
    }

    const glm::vec3 x_cm = glm::vec3(body.position);

    glm::vec3 sum_force(0.0f);   // Σ m_i · Δp_i
    glm::vec3 sum_torque(0.0f);  // Σ (r_i × m_i · Δp_i)
    for (uint32_t k = 0; k < count; ++k) {
        uint32_t pi = rigid_particle_indices[first + k];
        float w = inverse_masses[pi];
        float m = (w > 0.0f) ? (1.0f / w) : 0.0f;
        glm::vec3 dp = position_deltas[pi];
        glm::vec3 r  = predicted_positions[pi] - x_cm;
        sum_force  += m * dp;
        sum_torque += cross(r, m * dp);
        position_deltas[pi] = glm::vec3(0.0f);  // consumed
    }

    // Linear: Δx_cm = Σm·Δp / m_total
    glm::vec3 dx_cm = sum_force * body.total_inv_mass;
    body.position = glm::vec4(x_cm + dx_cm, 0.0f);

    // Angular: Δθ = I⁻¹_world · Στ  (small-angle quaternion update)
    glm::mat3 I_inv_world = body_inv_inertia_world(body);
    glm::vec3 dtheta = I_inv_world * sum_torque;
    float angle = length(dtheta);
    if (angle > 1e-9f) {
        const float kMaxAnglePerIter = 0.5f;  // ~28°/iter — clamp rotational impulses
        if (angle > kMaxAnglePerIter) {
            dtheta = dtheta * (kMaxAnglePerIter / angle);
            angle  = kMaxAnglePerIter;
        }
        glm::vec3 axis = dtheta / angle;
        float half = 0.5f * angle;
        float s = sinf(half);
        glm::vec4 dq{ axis.x * s, axis.y * s, axis.z * s, cosf(half) };
        body.orientation = quat_normalize(quat_mul(dq, body.orientation));
    }
}

// ---- Post-substep: derive v_lin, ω from (x − prev_x), (q · prev_q⁻¹) --------
__global__ void finalizeRigidBodyVelocities_kernel(
    RigidBodyGPU* bodies, uint32_t num_bodies, float inv_dt)
{
    uint32_t b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= num_bodies) return;
    RigidBodyGPU& body = bodies[b];
    if (body.total_inv_mass <= 0.0f) {
        body.linear_velocity  = glm::vec4(0.0f);
        body.angular_velocity = glm::vec4(0.0f);
        return;
    }

    glm::vec3 v = (glm::vec3(body.position) - glm::vec3(body.prev_position)) * inv_dt;
    body.linear_velocity = glm::vec4(v, 0.0f);

    glm::vec4 dq = quat_mul(body.orientation, quat_conj(body.prev_orientation));
    glm::vec3 omega = 2.0f * glm::vec3(dq.x, dq.y, dq.z) * inv_dt;
    if (dq.w < 0.0f) omega = -omega;
    body.angular_velocity = glm::vec4(omega, 0.0f);
}

// ---- Push per-particle velocities to match (v_lin + ω × r_world) -----------
// Keeps the per-particle velocity array consistent for XSPH viscosity and any
// neighbour reads from fluid kernels — without this, rigid particles would
// inherit whatever predict gave them and lie to surrounding fluid.
__global__ void rigidParticleVelocities_kernel(
    glm::vec3* velocities,
    const glm::vec3* predicted_positions,
    const uint32_t* rigid_particle_indices,
    const uint32_t* slot_to_body,
    const RigidBodyGPU* bodies,
    uint32_t num_rigid_slots)
{
    uint32_t k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_rigid_slots) return;
    const uint32_t pi = rigid_particle_indices[k];
    const uint32_t b  = slot_to_body[k];
    const RigidBodyGPU& body = bodies[b];
    if (body.total_inv_mass <= 0.0f) { velocities[pi] = glm::vec3(0.0f); return; }
    glm::vec3 r = predicted_positions[pi] - glm::vec3(body.position);
    glm::vec3 v = glm::vec3(body.linear_velocity) + cross(glm::vec3(body.angular_velocity), r);
    velocities[pi] = v;
}

// ---- Picker spring — body-level (impulse on v_lin AND torque on ω) ---------
__global__ void pickerSpringBody_kernel(
    RigidBodyGPU* bodies, uint32_t body_idx,
    const glm::vec3* predicted_positions,
    uint32_t picked_index, glm::vec3 target,
    float k_spring, float c_damp, float dt)
{
    if (threadIdx.x != 0 || blockIdx.x != 0) return;
    RigidBodyGPU& body = bodies[body_idx];
    if (body.total_inv_mass <= 0.0f) return;

    glm::vec3 handle = predicted_positions[picked_index];
    glm::vec3 r      = handle - glm::vec3(body.position);
    glm::vec3 v_lin  = glm::vec3(body.linear_velocity);
    glm::vec3 omega  = glm::vec3(body.angular_velocity);
    glm::vec3 v_handle = v_lin + cross(omega, r);

    glm::vec3 F = k_spring * (target - handle) - c_damp * v_handle;

    // Linear impulse on v: Δv = (F/m) · dt  =  F · inv_m · dt
    v_lin += dt * F * body.total_inv_mass;
    body.linear_velocity = glm::vec4(v_lin, 0.0f);

    // Angular impulse on ω: Δω = I⁻¹_world · τ · dt
    glm::vec3 tau = cross(r, F);
    glm::mat3 I_inv_world = body_inv_inertia_world(body);
    omega += dt * (I_inv_world * tau);
    body.angular_velocity = glm::vec4(omega, 0.0f);
}

// =============================================================================
// UPDATE VELOCITIES — restitution applied along accumulated contact normal.
// =============================================================================
__global__ void updateVelocities_kernel(
    glm::vec3* velocities,
    const glm::vec3* prev_positions, const glm::vec3* predicted_positions,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    const glm::vec3* contact_normals,
    uint32_t num_particles, float inv_dt, float global_damping)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const PhysicsMaterial& mat = materials[material_ids[i]];
    if (IsStatic(mat.phase)) { velocities[i] = glm::vec3(0.0f); return; }

    glm::vec3 v = (predicted_positions[i] - prev_positions[i]) * inv_dt;
    v *= global_damping;

    // NaN/Inf scrub. A single bad value here propagates through the next predict and
    // poisons the spatial grid, so we trap it here cheaply.
    if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z)) v = glm::vec3(0.0f);

    glm::vec3 cn = contact_normals[i];
    float cn_len_sq = dot(cn, cn);
    if (cn_len_sq > 1e-12f && mat.restitution > 0.0f) {
        float inv_len = rsqrtf(cn_len_sq);
        glm::vec3 n = cn * inv_len;
        float v_n = dot(v, n);
        // Reflect the inward-going normal component with restitution loss.
        if (v_n < 0.0f) {
            v -= (1.0f + mat.restitution) * v_n * n;
        }
    }
    velocities[i] = v;
}

// =============================================================================
// XSPH VISCOSITY
// =============================================================================
__global__ void xsphViscosity_kernel(
    const glm::vec3* predicted_positions, glm::vec3* velocities,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    CudaSpatialGridView grid, uint32_t num_particles, float h)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const PhysicsMaterial& mat_i = materials[material_ids[i]];
    if (!IsFluid(mat_i.phase) || mat_i.viscosity <= 0.0f) return;

    const glm::vec3 pos_i = predicted_positions[i];
    const glm::vec3 vel_i = velocities[i];
    const float h2 = h * h;
    glm::vec3 delta_v(0.0f);

    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);
    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;
            if (materials[material_ids[j]].phase != mat_i.phase) continue;
            const glm::vec3 r = pos_i - predicted_positions[j];
            if (dot(r, r) >= h2) continue;
            delta_v += (velocities[j] - vel_i) * KernelMath::Poly6(r, h);
        }
    }
    velocities[i] = vel_i + mat_i.viscosity * delta_v;
}

// =============================================================================
// LAUNCHERS
// =============================================================================
void launch_predictPositions(
    glm::vec3* d_positions, glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
    const float* d_inverse_masses, const uint32_t* d_material_ids,
    const PhysicsMaterial* d_materials, uint32_t n, const glm::vec3& gravity, float dt)
{
    if (n == 0) return;
    predictPositions_kernel<<<Blocks(n), kBlock>>>(
        d_positions, d_predicted_positions, d_velocities,
        d_inverse_masses, d_material_ids, d_materials, n, gravity, dt);
}

void launch_updateVelocities(
    glm::vec3* d_velocities, const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const glm::vec3* d_contact_normals,
    uint32_t n, float inv_dt, float global_damping)
{
    if (n == 0) return;
    updateVelocities_kernel<<<Blocks(n), kBlock>>>(
        d_velocities, d_positions, d_predicted_positions,
        d_material_ids, d_materials, d_contact_normals,
        n, inv_dt, global_damping);
}

void launch_resetSolverData(glm::vec3* d_deltas, float* d_lambdas, uint32_t n)
{
    if (n == 0) return;
    resetSolverData_kernel<<<Blocks(n), kBlock>>>(d_deltas, d_lambdas, n);
}

void launch_zeroVec3(glm::vec3* d_buffer, uint32_t n)
{
    if (n == 0 || !d_buffer) return;
    zeroVec3_kernel<<<Blocks(n), kBlock>>>(d_buffer, n);
}

__global__ void clampVelocity_kernel(glm::vec3* v, uint32_t n, float max_speed)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float s2 = dot(v[i], v[i]);
    float max2 = max_speed * max_speed;
    if (s2 > max2) v[i] *= max_speed * rsqrtf(s2);
}

void launch_clampVelocities(glm::vec3* d_velocities, uint32_t n, float max_speed)
{
    if (n == 0 || max_speed <= 0.0f) return;
    clampVelocity_kernel<<<Blocks(n), kBlock>>>(d_velocities, n, max_speed);
}

__global__ void pickerSpring_kernel(
    glm::vec3* velocities, const glm::vec3* predicted_positions,
    uint32_t picked_index, glm::vec3 target, float k, float c, float dt)
{
    // Single-particle update: launched with 1 thread.
    if (threadIdx.x != 0 || blockIdx.x != 0) return;
    glm::vec3 dx = target - predicted_positions[picked_index];
    velocities[picked_index] += dt * (k * dx - c * velocities[picked_index]);
}

void launch_applyPickerSpring(
    glm::vec3* d_velocities, const glm::vec3* d_predicted_positions,
    uint32_t picked_index, glm::vec3 target, float stiffness, float damping, float dt)
{
    if (picked_index == 0xFFFFFFFFu) return;
    pickerSpring_kernel<<<1, 1>>>(d_velocities, d_predicted_positions, picked_index,
                                  target, stiffness, damping, dt);
}

void launch_applyPickerSpringBody(
    RigidBodyGPU* d_rigid_bodies, uint32_t body_index,
    const glm::vec3* d_predicted_positions,
    uint32_t picked_index, glm::vec3 target,
    float stiffness, float damping, float dt)
{
    if (picked_index == 0xFFFFFFFFu) return;
    pickerSpringBody_kernel<<<1, 1>>>(
        d_rigid_bodies, body_index, d_predicted_positions, picked_index, target,
        stiffness, damping, dt);
}

void launch_applyDeltas(
    glm::vec3* d_predicted_positions, const glm::vec3* d_position_deltas,
    const float* d_inverse_masses, uint32_t n)
{
    if (n == 0) return;
    applyDeltas_kernel<<<Blocks(n), kBlock>>>(d_predicted_positions, d_position_deltas, d_inverse_masses, n);
}

void launch_chebyshevBlend(glm::vec3* d_current, glm::vec3* d_prev, float omega, uint32_t n)
{
    if (n == 0) return;
    chebyshev_blend_kernel<<<Blocks(n), kBlock>>>(d_current, d_prev, omega, n);
}

void launch_calculateLambdas(
    const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const CudaSpatialGrid* h_grid, float* d_density_lambdas,
    uint32_t n, float h, float dt)
{
    if (n == 0 || !h_grid) return;
    calculateLambdas_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
        h_grid->View(), d_density_lambdas, n, h, dt);
}

void launch_applyDensityCorrections(
    const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const CudaSpatialGrid* h_grid, glm::vec3* d_position_deltas,
    const float* d_density_lambdas, uint32_t n,
    float h, float k_pressure, float delta_q_ratio)
{
    if (n == 0 || !h_grid) return;
    applyDensityCorrections_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
        h_grid->View(), d_position_deltas, d_density_lambdas, n,
        h, k_pressure, delta_q_ratio);
}

void launch_solveParticleContacts(
    const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
    const float* d_inverse_masses,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const uint32_t* d_particle_to_body,
    const CudaSpatialGrid* h_grid, glm::vec3* d_position_deltas,
    glm::vec3* d_contact_normals,
    uint32_t n, float radius)
{
    if (n == 0 || !h_grid) return;
    solveParticleContacts_kernel<<<Blocks(n), kBlock>>>(
        d_positions, d_predicted_positions, d_inverse_masses,
        d_material_ids, d_materials, d_particle_to_body, h_grid->View(),
        d_position_deltas, d_contact_normals, n, radius);
}

void launch_solveDistanceConstraint(
    const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
    const DistanceConstraint* d_constraints,
    glm::vec3* d_position_deltas, float* d_lambdas,
    uint32_t n, float compliance, float dt)
{
    if (n == 0) return;
    solveDistanceConstraint_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_inverse_masses, d_constraints,
        d_position_deltas, d_lambdas, n, compliance, dt);
}

void launch_solveDomainCollision(
    const glm::vec3* d_positions, const glm::vec3* d_predicted_positions,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    glm::vec3* d_position_deltas, glm::vec3* d_contact_normals,
    uint32_t n, DomainBox domain, float radius)
{
    if (n == 0) return;
    solveDomainCollision_kernel<<<Blocks(n), kBlock>>>(
        d_positions, d_predicted_positions, d_material_ids, d_materials,
        d_position_deltas, d_contact_normals, n, domain, radius);
}

void launch_predictRigidBodies(
    RigidBodyGPU* d_rigid_bodies, uint32_t num_rigid_bodies,
    glm::vec3 gravity, float dt,
    float lin_damping, float ang_damping)
{
    if (num_rigid_bodies == 0) return;
    int threads = (num_rigid_bodies < 64u) ? static_cast<int>(num_rigid_bodies) : 64;
    int blocks  = (num_rigid_bodies + threads - 1) / threads;
    predictRigidBodies_kernel<<<blocks, threads>>>(
        d_rigid_bodies, num_rigid_bodies, gravity, dt, lin_damping, ang_damping);
}

void launch_warpParticlesToBody(
    glm::vec3* d_predicted_positions,
    const uint32_t* d_rigid_particle_indices,
    const glm::vec4* d_rigid_rest_positions,
    const uint32_t* d_slot_to_body,
    const RigidBodyGPU* d_rigid_bodies,
    uint32_t num_rigid_slots)
{
    if (num_rigid_slots == 0) return;
    warpParticlesToBody_kernel<<<Blocks(num_rigid_slots), kBlock>>>(
        d_predicted_positions, d_rigid_particle_indices, d_rigid_rest_positions,
        d_slot_to_body, d_rigid_bodies, num_rigid_slots);
}

void launch_aggregateRigidCorrections(
    const glm::vec3* d_predicted_positions,
    glm::vec3* d_position_deltas,
    const float* d_inverse_masses,
    const uint32_t* d_rigid_particle_indices,
    RigidBodyGPU* d_rigid_bodies,
    uint32_t num_rigid_bodies)
{
    if (num_rigid_bodies == 0) return;
    int threads = (num_rigid_bodies < 64u) ? static_cast<int>(num_rigid_bodies) : 64;
    int blocks  = (num_rigid_bodies + threads - 1) / threads;
    aggregateRigidCorrections_kernel<<<blocks, threads>>>(
        d_predicted_positions, d_position_deltas, d_inverse_masses,
        d_rigid_particle_indices, d_rigid_bodies, num_rigid_bodies);
}

void launch_finalizeRigidBodies(
    glm::vec3* d_velocities,
    const glm::vec3* d_predicted_positions,
    const uint32_t* d_rigid_particle_indices,
    const uint32_t* d_slot_to_body,
    RigidBodyGPU* d_rigid_bodies,
    uint32_t num_rigid_bodies,
    uint32_t num_rigid_slots,
    float inv_dt)
{
    if (num_rigid_bodies == 0) return;
    {
        int threads = (num_rigid_bodies < 64u) ? static_cast<int>(num_rigid_bodies) : 64;
        int blocks  = (num_rigid_bodies + threads - 1) / threads;
        finalizeRigidBodyVelocities_kernel<<<blocks, threads>>>(
            d_rigid_bodies, num_rigid_bodies, inv_dt);
    }
    if (num_rigid_slots > 0) {
        rigidParticleVelocities_kernel<<<Blocks(num_rigid_slots), kBlock>>>(
            d_velocities, d_predicted_positions, d_rigid_particle_indices,
            d_slot_to_body, d_rigid_bodies, num_rigid_slots);
    }
}

void launch_applyXSPHViscosity(
    const glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const CudaSpatialGrid* h_grid, uint32_t n, float h)
{
    if (n == 0 || !h_grid) return;
    xsphViscosity_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_velocities, d_material_ids, d_materials,
        h_grid->View(), n, h);
}

// =============================================================================
// BOUNDARY PUSH — soft layer that prevents wall stickiness for fluid particles.
//   PBF 2013 §5. Inside an h-thick band along each wall we add an inward delta
//   that scales with how deep into the band we are. The hard domain clamp still
//   runs afterwards as a safety net for fast-moving / non-fluid particles.
// =============================================================================
__global__ void boundaryPush_kernel(
    const glm::vec3* predicted_positions, const float* inverse_masses,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    glm::vec3* position_deltas,
    uint32_t num_particles, DomainBox domain, float h, float strength)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;
    if (inverse_masses[i] <= 0.0f) return;

    const PhysicsMaterial& mat = materials[material_ids[i]];
    if (!IsFluid(mat.phase)) return;

    const glm::vec3 p = predicted_positions[i];
    glm::vec3 delta(0.0f);

    auto push_wall = [&](float d, glm::vec3 inward) {
        if (d > 0.0f && d < h) {
            float t = 1.0f - d / h;            // 0 at boundary edge, 1 at the wall
            delta += inward * (strength * h * t * t);
        }
    };

    push_wall(p.x - domain.min.x,  glm::vec3( 1.f, 0.f, 0.f));
    push_wall(domain.max.x - p.x,  glm::vec3(-1.f, 0.f, 0.f));
    push_wall(p.y - domain.min.y,  glm::vec3( 0.f, 1.f, 0.f));
    push_wall(domain.max.y - p.y,  glm::vec3( 0.f,-1.f, 0.f));
    push_wall(p.z - domain.min.z,  glm::vec3( 0.f, 0.f, 1.f));
    push_wall(domain.max.z - p.z,  glm::vec3( 0.f, 0.f,-1.f));

    if (dot(delta, delta) > 0.0f) atomicAddVec3(&position_deltas[i], delta);
}

void launch_solveBoundaryPush(
    const glm::vec3* d_predicted_positions, const float* d_inverse_masses,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    glm::vec3* d_position_deltas,
    uint32_t n, DomainBox domain, float h, float strength)
{
    if (n == 0 || strength <= 0.0f) return;
    boundaryPush_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_inverse_masses, d_material_ids, d_materials,
        d_position_deltas, n, domain, h, strength);
}

// =============================================================================
// VORTICITY CONFINEMENT — PBF 2013 §6.2.
//   ω_i = Σ_j (v_j - v_i) × ∇W_ij
//   N   = ∇|ω|_i / ||∇|ω|_i||
//   f   = ε * (N × ω_i)
// We approximate ∇|ω| with a finite difference along each velocity neighbour. The resulting
// force is integrated into velocity in-place: v_i += f * dt.
// =============================================================================
__global__ void vorticityConfinement_kernel(
    const glm::vec3* predicted_positions, glm::vec3* velocities,
    const uint32_t* material_ids, const PhysicsMaterial* materials,
    CudaSpatialGridView grid,
    uint32_t num_particles, float h, float dt, float epsilon)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const PhysicsMaterial& mat_i = materials[material_ids[i]];
    if (!IsFluid(mat_i.phase)) return;

    const glm::vec3 pos_i = predicted_positions[i];
    const glm::vec3 vel_i = velocities[i];
    const float h2 = h * h;

    // 1. ω_i
    glm::vec3 omega(0.0f);
    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);
    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;
            if (materials[material_ids[j]].phase != mat_i.phase) continue;
            const glm::vec3 r = pos_i - predicted_positions[j];
            if (dot(r, r) >= h2) continue;
            const glm::vec3 grad = KernelMath::SpikyGradient(r, h);
            omega += cross(velocities[j] - vel_i, grad);
        }
    }
    float omega_mag = length(omega);
    if (omega_mag < 1e-6f) return;

    // 2. Gradient of |ω| via central difference on neighbours.
    glm::vec3 eta(0.0f);
    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;
            if (materials[material_ids[j]].phase != mat_i.phase) continue;
            const glm::vec3 r = pos_i - predicted_positions[j];
            if (dot(r, r) >= h2) continue;
            eta += omega_mag * KernelMath::SpikyGradient(r, h);
        }
    }
    float eta_len = length(eta);
    if (eta_len < 1e-6f) return;
    glm::vec3 N = eta / eta_len;
    glm::vec3 f_vorticity = epsilon * cross(N, omega);
    velocities[i] = vel_i + f_vorticity * dt;
}

void launch_applyVorticityConfinement(
    const glm::vec3* d_predicted_positions, glm::vec3* d_velocities,
    const uint32_t* d_material_ids, const PhysicsMaterial* d_materials,
    const CudaSpatialGrid* h_grid,
    uint32_t n, float h, float dt, float epsilon)
{
    if (n == 0 || epsilon <= 0.0f || !h_grid) return;
    vorticityConfinement_kernel<<<Blocks(n), kBlock>>>(
        d_predicted_positions, d_velocities, d_material_ids, d_materials,
        h_grid->View(), n, h, dt, epsilon);
}

// =============================================================================
// FOAM CLASSIFICATION — Ihmsen et al. 2012 ("Unified spray, foam and air bubbles
// for particle-based fluids"), trimmed to a per-particle scalar. We mark a fluid
// particle as foam if either
//   - speed exceeds a threshold (spray on splash crests), or
//   - it has very few same-fluid neighbours (surface / detached droplet).
// Both signals fade in over a small range so the mask isn't binary jitter.
// =============================================================================
__global__ void classifyFoam_kernel(
    uint32_t* flags_vbo,
    const uint32_t* static_flags,
    const glm::vec3* velocities,
    const glm::vec3* predicted_positions,
    const uint32_t* material_ids,
    const PhysicsMaterial* materials,
    CudaSpatialGridView grid,
    uint32_t num_particles, float h,
    float v_min, float v_max, float low_n)
{
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_particles) return;

    const uint32_t phase = materials[material_ids[i]].phase;
    uint32_t f = static_flags[i];  // start from clean static base (no stale foam bit)

    const glm::vec3 pos_i = predicted_positions[i];
    const float h2 = h * h;
    int neighbours = 0;

    // 1) Gather neighbors for ALL particles to support adaptive splatting
    const glm::ivec3 home = GridHash::PositionToCell(pos_i, grid.inv_cell_size);
    for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        const uint32_t cell = GridHash::CellHash(home.x + dx, home.y + dy, home.z + dz, grid.grid_size);
        const uint32_t s = grid.cell_starts[cell];
        if (s == kEmptyCell) continue;
        const uint32_t e = grid.cell_ends[cell];
        for (uint32_t k = s; k < e; ++k) {
            const uint32_t j = grid.sorted_indices[k];
            if (j == i) continue;
            if (materials[material_ids[j]].phase != phase) continue;
            const glm::vec3 r = pos_i - predicted_positions[j];
            if (dot(r, r) < h2) ++neighbours;
        }
    }

    // 2) Foam Logic (Speed + Isolation)
    if (phase == static_cast<uint32_t>(ParticlePhase::Fluid)) {
        const float v2 = dot(velocities[i], velocities[i]);
        const float v_min2 = v_min * v_min;
        if (v2 >= v_min2) {
            const float v_max2 = v_max * v_max;
            const float speed_score = (v2 >= v_max2) ? 1.f : (sqrtf(v2) - v_min) / fmaxf(v_max - v_min, 1e-3f);
            const float n_score = (float(neighbours) <= low_n) ? 1.f : fmaxf(0.f, 1.f - (float(neighbours) - low_n) / fmaxf(low_n, 1.f));
            if (speed_score * n_score > 0.30f) f |= 8u;
        }
    }

    // 3) Store neighbor count in high bits (24-31) for the renderer
    uint32_t n_clamped = (uint32_t)fminf((float)neighbours, 255.0f);
    f |= (n_clamped << 24);

    flags_vbo[i] = f;
}

void launch_classifyFoam(
    uint32_t* d_flags_vbo,
    const uint32_t* d_static_flags,
    const glm::vec3* d_velocities,
    const glm::vec3* d_predicted_positions,
    const uint32_t* d_material_ids,
    const PhysicsMaterial* d_materials,
    const CudaSpatialGrid* h_grid,
    uint32_t n, float h, float v_min, float v_max, float low_n)
{
    if (n == 0 || !h_grid || !d_flags_vbo || !d_static_flags) return;
    classifyFoam_kernel<<<Blocks(n), kBlock>>>(
        d_flags_vbo, d_static_flags, d_velocities, d_predicted_positions,
        d_material_ids, d_materials, h_grid->View(),
        n, h, v_min, v_max, low_n);
}

} // namespace Hex
