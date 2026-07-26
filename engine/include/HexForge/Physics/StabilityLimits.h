#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Hex::StabilityLimits
{
    // ============================================================================================
    // Math-derived bounds for the XPBD solver. Every "magic number" in the engine should be
    // derivable from a small set of physical inputs (particle radius r, rest density ρ, gravity g,
    // frame dt). The constants here are intentionally documented from first principles or cited
    // back to the literature so anyone tuning the solver can see *why* a default is what it is.
    //
    // Sources cited:
    //   [PBF13]      Macklin & Müller 2013,        "Position Based Fluids" (SIGGRAPH).
    //   [XPBD16]     Macklin, Müller, Chentanez 2016, "XPBD: Position-Based Simulation of
    //                                                 Compliant Constrained Dynamics".
    //   [SmallStep19] Macklin et al. 2019,          "Small Steps in Physics Simulation" (SCA).
    //   [PBDSurvey17] Bender, Müller, Macklin 2017, "Position-Based Simulation Methods" (EG STAR).
    //   [Mueller05]  Müller et al. 2005,            "Meshless Deformations Based on Shape Matching".
    // ============================================================================================

    inline constexpr float kPi = 3.14159265358979323846f;

    // -- Geometry / sampling --------------------------------------------------------------------
    //
    // Initial close-packed spacing s = 2 r so neighbouring spheres just touch. Any tighter and the
    // density solver starts in a compressed state and explodes on the first step.
    inline float ParticleSpacingFromRadius(float particle_radius)
    {
        return 2.0f * particle_radius;
    }

    // Interaction radius h. [PBF13] uses h = 4 r so that an interior particle has roughly 30–40
    // SPH neighbours — enough for a stable Poly6 density estimate without exploding the
    // O(neighbours) per-iter cost.
    inline float InteractionRadiusFromParticleRadius(float particle_radius)
    {
        return 4.0f * particle_radius;
    }

    // Particle mass derivation:
    //   In a uniform-spacing sampling, each particle "owns" a cell volume s³.
    //   m = ρ_target * s³
    // Note: s is the *initial spacing*, not 2h or anything else.
    inline float ParticleMassFromDensity(float rest_density, float particle_spacing)
    {
        return rest_density * particle_spacing * particle_spacing * particle_spacing;
    }

    // -- Timestep / substep bounds --------------------------------------------------------------
    //
    // CFL-style derivation for "no tunneling under free-fall":
    //   In one substep a particle gains v = g · dt. The next predict moves it by v · dt = g · dt².
    //   To stay below one radius:        g · dt² < r        ⇒   dt < √(r / g).
    // This is the *absolute* upper bound; 50 % safety factor is sensible.
    inline float MaxStableSubstepDt(float particle_radius, float gravity_magnitude)
    {
        if (gravity_magnitude <= 0.0f) return 1.0f / 60.0f;
        return 0.5f * std::sqrt(particle_radius / gravity_magnitude);
    }

    // Substep count for a given frame dt — round up so sub_dt ≤ MaxStableSubstepDt.
    // [SmallStep19] recommends N ≥ 4 for stiff stacks; we clamp to [1, 32] regardless.
    inline int RecommendedSubsteps(float frame_dt, float particle_radius, float gravity_magnitude)
    {
        const float max_sub = MaxStableSubstepDt(particle_radius, gravity_magnitude);
        if (max_sub <= 0.0f) return 4;
        int n = static_cast<int>(std::ceil(frame_dt / max_sub));
        if (n < 1)  n = 1;
        if (n > 32) n = 32;
        return n;
    }

    // Maximum particle speed before two adjacent particles can swap places in one substep.
    //   Relative velocity 2v over dt traverses 2v·dt. For non-tunneling we need
    //     2v · dt < 2 r           ⇒  v < r / dt.
    // 0.5 was too restrictive (capped Earth gravity at 9.6 m/s after ~1 second) — bump to 0.9
    // so we still have headroom against tunneling but a 5 m drop reaches the right speed.
    inline float MaxParticleSpeed(float particle_radius, float sub_dt)
    {
        if (sub_dt <= 0.0f) return 25.0f;
        return 0.9f * particle_radius / sub_dt;
    }

    // -- Damping -------------------------------------------------------------------------------
    //
    // The velocity-update multiplies v by a per-substep factor. To get a target damping rate
    // [1/s] independent of substep count, use exponential decay:
    //   v(t) = v(0) · e^{-λ t}      ⇒   per-step factor = e^{-λ · dt}.
    // λ ≈ 0.1 s⁻¹ gives a very gentle "scene loss" of ~10 % over one second.
    inline float DampingPerSubstep(float damping_rate_per_second, float sub_dt)
    {
        return std::exp(-damping_rate_per_second * sub_dt);
    }

    // -- Shape-matching stiffness --------------------------------------------------------------
    //
    // Per-substep stiffness α_sub for a target *per-frame* stiffness α_frame:
    //   After N substeps the remaining error fraction is (1 - α_sub)^N, want this to equal
    //   (1 - α_frame). So
    //       α_sub = 1 - (1 - α_frame)^{1/N}.
    // This keeps the visual settling time of a shape-matched rigid invariant under substep count.
    inline float ShapeMatchStiffnessPerSubstep(float per_frame_stiffness, int substeps)
    {
        if (substeps <= 1) return per_frame_stiffness;
        const float clamped = std::clamp(per_frame_stiffness, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - clamped, 1.0f / static_cast<float>(substeps));
    }

    // -- Boundary push --------------------------------------------------------------------------
    //
    // The push delta in the kernel is
    //   Δ(d) = strength · h · (1 - d/h)²        for d ∈ [0, h).
    // Cumulative push at d=0 across one frame's K total iterations:
    //   K · strength · h.
    // We expose the user-facing dial as "fraction of h pushed inward per frame at d=0":
    //   strength = fraction_per_frame / K_total
    // where K_total = substeps · iters_per_substep.
    inline float BoundaryPushPerIteration(float fraction_per_frame_at_wall, int total_iterations)
    {
        if (total_iterations <= 0) return 0.0f;
        return fraction_per_frame_at_wall / static_cast<float>(total_iterations);
    }

    // -- PBF artificial pressure ----------------------------------------------------------------
    //
    // [PBF13 Eq. 13] s_corr = -k · (W(r,h) / W(Δq,h))^n. Recommended values:
    //   Δq = 0.2 h to 0.3 h    (the "preferred neighbour distance")
    //   k  = 1e-4 ... 1e-3     (small enough to leave the density constraint dominant)
    //   n  = 4
    // We expose those as defaults; the macros below let callers hard-code matching values.
    inline constexpr float kPBF_DefaultK            = 1.0e-4f;
    inline constexpr float kPBF_DefaultDeltaQRatio  = 0.2f;
    inline constexpr int   kPBF_DefaultExponent     = 4;

    // -- Kernel-consistent rest density ---------------------------------------------------------
    //
    // CRITICAL for stability. PBF's density constraint is  C = ρ/ρ0 − 1, where ρ is a *pure
    // Poly6 sum* over neighbours (the kernels carry no SI mass — every particle contributes
    // W(r,h)). For C ≈ 0 at the spawn configuration, ρ0 MUST equal that same Poly6 sum evaluated
    // on the rest lattice — NOT a real-world figure like 1000 kg/m³.
    //
    // Mismatch failure mode (the classic "fluid collapses then explodes"):
    //   ρ0 too high  →  C clamped to 0 everywhere  →  no pressure  →  free-fall + interpenetration
    //                →  local clusters spike ρ to many × ρ0  →  λ overshoots  →  particles eject.
    //
    // This sums Poly6 over a regular cubic lattice of the given spacing within radius h (including
    // the self term at r=0), giving the exact rest density to hand to the fluid material.
    inline float LatticeRestDensity(float interaction_radius, float particle_spacing)
    {
        if (particle_spacing <= 0.0f || interaction_radius <= 0.0f) return 1.0f;
        const float h  = interaction_radius;
        const float h2 = h * h;
        const float coeff = 315.0f / (64.0f * kPi * std::pow(h, 9.0f));
        const int   range = std::max(1, static_cast<int>(std::ceil(h / particle_spacing)));

        float density = 0.0f;
        for (int dz = -range; dz <= range; ++dz)
        for (int dy = -range; dy <= range; ++dy)
        for (int dx = -range; dx <= range; ++dx) {
            const float rx = dx * particle_spacing;
            const float ry = dy * particle_spacing;
            const float rz = dz * particle_spacing;
            const float r2 = rx * rx + ry * ry + rz * rz;
            if (r2 < h2) { const float d = h2 - r2; density += coeff * d * d * d; }
        }
        return density;
    }
}
