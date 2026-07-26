// HexForge
#include <HexForge/Core/Application.h>
#include <HexForge/Gameplay/EntityComponents.h>
#include <HexForge/Gameplay/EntityManager.h>
#include <HexForge/Physics/PhysicsSystem.h>
#include <HexForge/Physics/PhysicsMaterial.h>
#include <HexForge/Physics/StabilityLimits.h>
#include <HexForge/Physics/MeshSampler.h>
#include <HexForge/Renderer/Data/Mesh.h>

#include <glm/gtc/random.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <vector>

namespace
{
    // -------------------------------------------------------------------------------------------
    // Particle / scene helpers
    // -------------------------------------------------------------------------------------------
    entt::entity CreateParticle(Hex::EntityManager& em,
                                const glm::vec3& world_pos,
                                const std::shared_ptr<Hex::Material>& renderMat,
                                const Hex::PhysicsMaterial& physMat,
                                float invMass,
                                float visualRadius,
                                bool attach_visual = false)
    {
        auto e = em.CreateEntity();
        em.AddComponent<Hex::TransformComponent>(e,
            Hex::TransformComponent{ world_pos, glm::quat{}, glm::vec3{ visualRadius } });
        em.AddComponent<Hex::ParticleComponent>(e,
            Hex::ParticleComponent{ world_pos, {0.f, 0.f, 0.f}, invMass });
        em.AddComponent<Hex::PhysicsMaterialComponent>(e, Hex::PhysicsMaterialComponent{ physMat });

        if (attach_visual) {
            auto sphereMesh = Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/sphere.obj");
            em.AddComponent<Hex::ModelComponent>(e, Hex::ModelComponent{ sphereMesh });
            em.AddComponent<Hex::MaterialComponent>(e, Hex::MaterialComponent{ renderMat });
        }
        return e;
    }

    // Fluid block spawn that skips positions too close to any rigid-body sample. This is what
    // stops water particles from spawning *inside* a rigid body — once trapped, a particle
    // surrounded by equidistant rigid neighbours can never escape because the contact pushes
    // all cancel.
    void SpawnFluidBlock(Hex::EntityManager& em, const glm::vec3& origin, const glm::ivec3& counts,
                         float spacing, const std::shared_ptr<Hex::Material>& renderMat,
                         const Hex::PhysicsMaterial& physMat, float visualRadius,
                         const std::vector<glm::vec3>& rigid_positions = {},
                         float clearance = 0.0f)
    {
        const float clr_sq = clearance * clearance;
        auto too_close = [&](const glm::vec3& p) {
            if (clr_sq <= 0.0f || rigid_positions.empty()) return false;
            for (const auto& r : rigid_positions) {
                glm::vec3 d = p - r;
                if (dot(d, d) < clr_sq) return true;
            }
            return false;
        };

        for (int z = 0; z < counts.z; ++z)
        for (int y = 0; y < counts.y; ++y)
        for (int x = 0; x < counts.x; ++x) {
            glm::vec3 jitter = glm::linearRand(glm::vec3(-0.03f) * spacing,
                                               glm::vec3( 0.03f) * spacing);
            glm::vec3 pos = origin + glm::vec3(x, y, z) * spacing + jitter;
            if (too_close(pos)) continue;
            CreateParticle(em, pos, renderMat, physMat, 1.0f, visualRadius);
        }
    }

    // -------------------------------------------------------------------------------------------
    // Rigid body (sampled from a mesh) + visual mesh attached for "render as mesh" pipeline.
    //
    // `mesh_to_world_origin` is the world position where the source mesh's *local origin* should
    // sit at spawn time (this is what `world_transform[3]` was when we voxelised). We use it to
    // derive the centroid offset so the rendered mesh's own centroid lands exactly on the
    // physics-tracked centroid — without this, the mesh appears displaced from its particles.
    // -------------------------------------------------------------------------------------------
    void SpawnSampledRigidBody(Hex::EntityManager& em,
                               uint32_t rigidBodyId,
                               const Hex::SampledRigidBody& sample,
                               const std::shared_ptr<Hex::Model>& visual_model,
                               const std::shared_ptr<Hex::Material>& visual_mat,
                               const glm::vec3& visual_scale,
                               const glm::vec3& mesh_to_world_origin,
                               const Hex::PhysicsMaterial& physMat,
                               float visualRadius,
                               float per_frame_stiffness)
    {
        if (sample.particles.empty()) return;

        // Local centroid in mesh space — what the renderer needs so the mesh's geometric
        // centre lands on the body's tracked centroid each frame.
        glm::vec3 local_centroid(0.0f);
        if (visual_scale.x > 1e-6f && visual_scale.y > 1e-6f && visual_scale.z > 1e-6f) {
            local_centroid = (sample.centroid_world - mesh_to_world_origin) / visual_scale;
        }

        auto body_entity = em.CreateEntity();
        em.AddComponent<Hex::RigidBodyComponent>(body_entity,
            Hex::RigidBodyComponent{ rigidBodyId, per_frame_stiffness });
        em.AddComponent<Hex::TransformComponent>(body_entity,
            Hex::TransformComponent{ sample.centroid_world, glm::quat{}, visual_scale });
        if (visual_model) {
            em.AddComponent<Hex::ModelComponent>(body_entity, Hex::ModelComponent{ visual_model });
            em.AddComponent<Hex::MaterialComponent>(body_entity, Hex::MaterialComponent{ visual_mat });
            em.AddComponent<Hex::RigidBodyVisualComponent>(body_entity,
                Hex::RigidBodyVisualComponent{ visual_model, visual_mat, visual_scale, local_centroid });
        }

        for (auto& p : sample.particles) {
            auto e = CreateParticle(em, p.world_position, /*renderMat*/nullptr, physMat,
                                    1.0f, visualRadius, /*attach_visual*/false);
            em.AddComponent<Hex::RigidBodyMemberComponent>(e,
                Hex::RigidBodyMemberComponent{ rigidBodyId, p.local_rest });
        }
    }

    // -------------------------------------------------------------------------------------------
    // Cloth: NxM grid of particles connected by structural + shear distance constraints. The top
    // row is pinned (invMass=0). Bending constraints are skipped for simplicity — adequate for a
    // hanging banner.
    // -------------------------------------------------------------------------------------------
    void SpawnCloth(Hex::EntityManager& em,
                    const glm::vec3& origin,
                    const glm::vec3& u_axis, const glm::vec3& v_axis,
                    int nu, int nv,
                    const std::shared_ptr<Hex::Material>& renderMat,
                    const Hex::PhysicsMaterial& physMat,
                    float visualRadius,
                    bool pin_top_row)
    {
        std::vector<std::vector<entt::entity>> grid(nu, std::vector<entt::entity>(nv));
        const float u_len = glm::length(u_axis);
        const float v_len = glm::length(v_axis);

        // Row-major flat list for the cloth mesh renderer to walk.
        std::vector<entt::entity> grid_flat(nu * nv);

        for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
            glm::vec3 pos = origin + u_axis * (float(i)/(nu-1)) + v_axis * (float(j)/(nv-1));
            bool pinned = pin_top_row && (j == nv - 1);
            grid[i][j] = CreateParticle(em, pos, renderMat, physMat,
                                        pinned ? 0.0f : 1.0f, visualRadius);
            grid_flat[j * nu + i] = grid[i][j];
        }

        // One ClothComponent per banner — the renderer iterates these and builds a dynamic mesh.
        auto cloth_entity = em.CreateEntity();
        em.AddComponent<Hex::ClothComponent>(cloth_entity,
            Hex::ClothComponent{ std::move(grid_flat), nu, nv, physMat.color });

        auto link = [&](entt::entity a, entt::entity b) {
            const auto& pa = em.GetComponent<Hex::TransformComponent>(a).position;
            const auto& pb = em.GetComponent<Hex::TransformComponent>(b).position;
            auto c = em.CreateEntity();
            em.AddComponent<Hex::DistanceConstraintComponent>(c,
                Hex::DistanceConstraintComponent{ a, b, glm::distance(pa, pb) });
        };

        // Structural (axis-aligned) + shear (diagonals). Bending links (skip-one neighbours)
        // were causing overshoot under Jacobi iteration with finite per-substep iter counts —
        // a tightly-sampled cloth has the bending constraint at almost the same rest length as
        // 2x the structural constraint, and the Jacobi sum over many overlapping constraints
        // builds up faster than the lambda can correct it. Stick to 4 links per interior node.
        for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
            if (i + 1 < nu) link(grid[i][j], grid[i+1][j]);                       // structural u
            if (j + 1 < nv) link(grid[i][j], grid[i][j+1]);                       // structural v
            if (i + 1 < nu && j + 1 < nv) link(grid[i][j], grid[i+1][j+1]);       // shear /
            if (i + 1 < nu && j - 1 >= 0) link(grid[i][j], grid[i+1][j-1]);       // shear anti-diagonal
        }
        (void)u_len; (void)v_len;
    }

    // -------------------------------------------------------------------------------------------
    // Rope: linear chain of particles with distance constraints between consecutive segments.
    // The first particle is pinned. Adjust min spacing to taste.
    // -------------------------------------------------------------------------------------------
    void SpawnRope(Hex::EntityManager& em,
                   const glm::vec3& anchor,
                   const glm::vec3& end,
                   int segments,
                   const std::shared_ptr<Hex::Material>& renderMat,
                   const Hex::PhysicsMaterial& physMat,
                   float visualRadius)
    {
        std::vector<entt::entity> beads(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            glm::vec3 pos = anchor + (end - anchor) * (float(i) / float(segments));
            float inv_mass = (i == 0) ? 0.0f : 1.0f;
            beads[i] = CreateParticle(em, pos, renderMat, physMat, inv_mass, visualRadius);
        }
        for (int i = 0; i < segments; ++i) {
            auto c = em.CreateEntity();
            const auto& pa = em.GetComponent<Hex::TransformComponent>(beads[i]).position;
            const auto& pb = em.GetComponent<Hex::TransformComponent>(beads[i+1]).position;
            em.AddComponent<Hex::DistanceConstraintComponent>(c,
                Hex::DistanceConstraintComponent{ beads[i], beads[i+1], glm::distance(pa, pb) });
        }
    }

}

// =============================================================================================
// Sandbox scene
//   - Dam-break water column
//   - Two rigid bodies sampled from primitive meshes (cube, sphere)
//   - A procedural "bunny" rigid body — voxelized + rendered as its own source mesh
//   - A hanging cloth banner
//   - A rope dangling off-center
//   - Visualised tank floor
// =============================================================================================
int main()
{
#if defined(__linux__) || defined(__gnu_linux__)
    ::setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
    ::setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
    ::setenv("DRI_PRIME", "1", 0);
#endif

    Hex::AppSpecification spec;
    spec.name       = "HexaForge XPBD";
    spec.width      = 1920;
    spec.height     = 1080;
    spec.fullscreen = false;
    spec.vsync      = false;

    auto scene = [&](Hex::EntityManager& em, Hex::PhysicsSystem& ps)
    {
        auto defaultMat = Hex::ResourceManager::LoadMaterial(
            RESOURCES_PATH "shaders/debug.vert",
            RESOURCES_PATH "shaders/debug.frag");

        const float particle_radius = 0.08f;
        const float rest_density    = 1000.0f;
        const float spacing         = Hex::StabilityLimits::ParticleSpacingFromRadius(particle_radius);

        ps.m_gravity    = { 0.0f, -9.81f, 0.0f };
        ps.m_domain.min = { -6.0f, -1.5f, -4.0f };
        ps.m_domain.max = {  6.0f,  6.0f,  4.0f };
        ps.AutoTune(particle_radius, rest_density);
        // --- PRO SOLVER: many small substeps, few iters (Macklin 2019) ---
        ps.m_substeps               = 10;
        ps.m_solverIterations       = 2;
        // h = 4r gives ~30-40 SPH neighbours — the minimum for a well-conditioned Poly6 density
        // estimate (StabilityLimits.h). Anything near 2r (~6 neighbours) makes the density solve
        // ill-posed and the fluid explodes regardless of how many substeps you throw at it.
        ps.m_fluidInteractionRadius = Hex::StabilityLimits::InteractionRadiusFromParticleRadius(particle_radius);
        ps.m_pbfPressureK           = Hex::StabilityLimits::kPBF_DefaultK; // 1e-4, leaves density dominant
        ps.m_globalDamping          = 0.99f;  // Less damping for more 'alive' water
        ps.m_vorticityEpsilon       = 0.0f;   // off — injects energy; not wired into the step loop
        ps.m_foamMinSpeed           = 6.0f;
        ps.m_distanceCompliance     = 1e-4f;

        // The PBF density target MUST match the kernel's Poly6 sum on the spawn lattice, not a
        // real-world 1000 kg/m³ (see StabilityLimits::LatticeRestDensity). This is THE fix that
        // stops the "collapse then explode" behaviour.
        const float fluid_rest_density =
            Hex::StabilityLimits::LatticeRestDensity(ps.m_fluidInteractionRadius, spacing);

        // AutoTune derived the speed cap from its own (single-substep) sub_dt; we changed the
        // substep count, so recompute it from the real sub_dt or water ends up clamped ~10x too
        // slow (syrupy) — a separate symptom of the same override.
        {
            const float sub_dt = ps.m_fixedTimeStep / static_cast<float>(ps.m_substeps);
            ps.m_maxParticleSpeed = Hex::StabilityLimits::MaxParticleSpeed(particle_radius, sub_dt);
        }
        // ---------------------------------------------------

        // -------- Rigid cube (sampled from cube.obj). Sits ON the floor at start. --------
        glm::vec3 cube_origin{ 2.2f, -1.2f, 1.5f };
        glm::vec3 cube_scale (0.6f);
        glm::mat4 cube_xform = glm::translate(glm::mat4(1.0f), cube_origin)
                             * glm::scale(glm::mat4(1.0f), cube_scale);
        auto sampled_cube = Hex::MeshSampler::VoxelizeFromFile(
            RESOURCES_PATH "models/cube.obj", cube_xform, spacing * 0.8f);
        SpawnSampledRigidBody(em, /*id*/1, sampled_cube,
            Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/cube.obj"),
            defaultMat, cube_scale, cube_origin,
            Hex::Materials::Rigid({ 0.95f, 0.40f, 0.15f, 1.0f }),
            particle_radius, /*stiffness*/ 0.95f);

        // -------- Rigid sphere — falls into the water --------
        glm::vec3 sphere_origin{ 0.0f, 2.5f, 0.0f };
        glm::vec3 sphere_scale (0.7f);
        glm::mat4 sphere_xform = glm::translate(glm::mat4(1.0f), sphere_origin)
                               * glm::scale(glm::mat4(1.0f), sphere_scale);
        auto sampled_sphere = Hex::MeshSampler::VoxelizeFromFile(
            RESOURCES_PATH "models/sphere.obj", sphere_xform, spacing * 0.8f);
        SpawnSampledRigidBody(em, /*id*/2, sampled_sphere,
            Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/sphere.obj"),
            defaultMat, sphere_scale, sphere_origin,
            Hex::Materials::Rigid({ 0.30f, 0.85f, 0.45f, 1.0f }),
            particle_radius, /*stiffness*/ 0.95f);

        // Collect rigid sample positions so the water block can skip them.
        std::vector<glm::vec3> rigid_positions;
        auto stash = [&](const Hex::SampledRigidBody& s) {
            for (const auto& p : s.particles) rigid_positions.push_back(p.world_position);
        };
        stash(sampled_cube);
        stash(sampled_sphere);

        // -------- Real Stanford bunny — voxelised from bunny.obj, rendered as the bunny mesh.
        // The OBJ is ~0.14 m across in model space; scale up so it's a meaningful size in the
        // 12 m wide tank.
        glm::vec3 bunny_origin{ -3.0f, 4.5f, 0.0f };
        glm::vec3 bunny_scale (20.0f);
        glm::mat4 bunny_xform = glm::translate(glm::mat4(1.0f), bunny_origin)
                              * glm::scale(glm::mat4(1.0f), bunny_scale);
        auto sampled_bunny = Hex::MeshSampler::VoxelizeFromFile(
            RESOURCES_PATH "models/bunny.obj", bunny_xform, spacing * 0.8f);
        SpawnSampledRigidBody(em, /*id*/3, sampled_bunny,
            Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/bunny.obj"),
            defaultMat, bunny_scale, bunny_origin,
            Hex::Materials::Rigid({ 0.95f, 0.85f, 0.7f, 1.0f }),
            particle_radius, /*stiffness*/ 0.95f);
        stash(sampled_bunny);

        // -------- Water dam (spawned LAST so it can skip cells occupied by any rigid body) --------
        Hex::PhysicsMaterial water = Hex::Materials::Water();
        water.rest_density = fluid_rest_density;   // kernel-consistent — see above
        SpawnFluidBlock(em,
            { ps.m_domain.min.x + 0.2f, ps.m_domain.min.y + 0.2f, ps.m_domain.min.z + 0.4f },
            { 14, 22, 22 }, spacing, defaultMat, water, particle_radius,
            rigid_positions, /*clearance=*/ particle_radius * 2.5f);

        // -------- Cloth banner (3x2 m) hanging off the back wall --------
        // Cloth phase: distance constraints handle structure; cloth particles skip the
        // particle-particle contact kernel. Coarser mesh (12x8 instead of 18x12) gives each
        // constraint a longer rest length, which means a 4-iter-per-substep solve actually
        // converges across the whole chain in one frame.
        SpawnCloth(em,
            /*origin*/ { -4.0f, 2.5f, 2.0f },
            /*u_axis*/ { 3.0f, 0.0f, 0.0f },
            /*v_axis*/ { 0.0f, 2.0f, 0.0f },
            /*nu*/ 12, /*nv*/ 8,
            defaultMat,
            Hex::Materials::Cloth({ 0.85f, 0.30f, 0.55f, 1.0f }),
            particle_radius,
            /*pin_top_row*/ true);

        // -------- Rope dangling --------
        SpawnRope(em,
            /*anchor*/ { 4.5f, 4.0f, -2.5f },
            /*end*/    { 4.5f, 1.0f, -2.5f },
            /*segments*/ 16,
            defaultMat,
            Hex::Materials::Rigid({ 0.85f, 0.75f, 0.30f, 1.0f }),
            particle_radius);

        // No floor mesh — the Renderer draws an infinite procedural grid at y = domain.min.y.
    };

    Hex::Application application(spec, scene);
    application.Run();
    return 0;
}
