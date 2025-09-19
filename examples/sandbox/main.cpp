// HexForge
#include <HexForge/Core/Application.h>
#include <HexForge/Gameplay/EntityComponents.h>
#include <HexForge/Gameplay/EntityManager.h>
#include <HexForge/Physics/PhysicsSystem.h>

// glm
#include <glm/gtc/random.hpp>
#include <glm/gtx/compatibility.hpp>

// Helper function to create a particle
entt::entity CreateParticle(Hex::EntityManager& em, glm::vec3 pos, const std::shared_ptr<Hex::Material>& material, float invMass = 1.0f) {
    auto e = em.CreateEntity();
    em.AddComponent<Hex::TransformComponent>(e, Hex::TransformComponent{pos, glm::quat{}, glm::vec3{0.05f}});
    em.AddComponent<Hex::ParticleComponent>(e, Hex::ParticleComponent{ pos, {0.f, 0.f, 0.f}, invMass});

    auto sphereMesh = Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/sphere.obj");
    em.AddComponent<Hex::ModelComponent>(e, Hex::ModelComponent{sphereMesh});

    em.AddComponent<Hex::MaterialComponent>(e, Hex::MaterialComponent{material});
    return e;
}

// Helper to calculate rest volume of a tetrahedron
float GetRestVolume(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4) {
    return glm::dot(p2 - p1, glm::cross(p3 - p1, p4 - p1)) / 6.0f;
}

void CreateCloth(Hex::EntityManager& em, Hex::DeformableBodyComponent& body, const std::shared_ptr<Hex::Material>& material, const glm::vec3& origin, int width, int height, float spacing) {
    std::vector<entt::entity> particles;
    particles.resize(width * height);

    // --- ANCHOR POINTS ---
    // Create two new invisible particles that will act as our fixed pins.
    // They are not part of the cloth body and have 0 inverse mass.
    glm::vec3 topLeftAnchorPos = origin + glm::vec3(0 * spacing, (height - 1) * spacing, 0.0f);
    glm::vec3 topRightAnchorPos = origin + glm::vec3((width - 1) * spacing, (height - 1) * spacing, 0.0f);

    auto topLeftAnchor = em.CreateEntity();
    em.AddComponent<Hex::TransformComponent>(topLeftAnchor, Hex::TransformComponent{topLeftAnchorPos});
    em.AddComponent<Hex::ParticleComponent>(topLeftAnchor, Hex::ParticleComponent{topLeftAnchorPos,
        {0,0,0}, 0.0f});

    auto topRightAnchor = em.CreateEntity();
    em.AddComponent<Hex::TransformComponent>(topRightAnchor, Hex::TransformComponent{topRightAnchorPos});
    em.AddComponent<Hex::ParticleComponent>(topRightAnchor, Hex::ParticleComponent{topRightAnchorPos,
        {0,0,0}, 0.0f});


    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            glm::vec3 pos = origin + glm::vec3(i * spacing, j * spacing, 0.0f);
            // ALL cloth particles now have mass and can move.
            particles[j * width + i] = CreateParticle(em, pos, material, 1.0f);
        }
    }

    // --- WELD CONSTRAINTS ---
    // Attach the top corners of the cloth to the anchor points with stiff, zero-length constraints.
    entt::entity topLeftCorner = particles[(height - 1) * width + 0];
    entt::entity topRightCorner = particles[(height - 1) * width + (width - 1)];

    body.distanceConstraints.emplace_back(topLeftAnchor, topLeftCorner, 0.0f, 0.0f);
    body.distanceConstraints.emplace_back(topRightAnchor, topRightCorner, 0.0f, 0.0f);


    // Structural springs (unchanged)
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            if (i < width - 1) body.distanceConstraints.emplace_back(particles[j*width+i], particles[j*width+i+1],
                spacing, 1e-6f);
            if (j < height - 1) body.distanceConstraints.emplace_back(particles[j*width+i], particles[(j+1)*width+i],
                spacing, 1e-6f);
        }
    }
}

// Helper to create a stiff rod ---
void CreateRod(Hex::EntityManager& em, Hex::DeformableBodyComponent& body, const std::shared_ptr<Hex::Material>& material,
    const glm::vec3& start, const glm::vec3& end, int segments) {
    std::vector<entt::entity> particles;
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        glm::vec3 pos = glm::lerp(start, end, t);
        float invMass = (i == 0) ? 0.0f : 1.0f; // Pin one end
        particles.push_back(CreateParticle(em, pos, material, invMass));
    }
    for (int i = 0; i < segments; ++i) {
        // Very low compliance for a stiff constraint
        body.distanceConstraints.emplace_back(particles[i], particles[i+1],
            glm::distance(start, end) / segments, 0.0f); // Stiff
    }
}


int main()
{
    Hex::AppSpecification spec;
    spec.name = "XPBD Sandbox";
    spec.width = 1920;
    spec.height = 1080;
    spec.fullscreen = false;
    spec.vsync = false;

    auto scene = [&](Hex::EntityManager& em, Hex::PhysicsSystem& ps)
    {
        auto defaultMat = Hex::ResourceManager::LoadMaterial(
            RESOURCES_PATH "shaders/debug.vert",
            RESOURCES_PATH "shaders/debug.frag"
        );

        // --- NEW: Create a visible mouse picker sphere ---
        //auto picker = CreateParticle(em, {0, 5, 5}, defaultMat, 0.f); // Zero inverse mass so it's not affected by physics
        //em.GetComponent<Hex::TransformComponent>(picker).scale = {0.2f, 0.2f, 0.2f};
        //ps.SetMousePicker(picker);

       auto deformableBodyEntity1 = em.CreateEntity("DeformableBody1");
       auto& body1 = em.AddComponent<Hex::DeformableBodyComponent>(deformableBodyEntity1);

       // --- EXAMPLE 1: Create a stiff rod ---
       CreateRod(em, body1, defaultMat, {15, 15, 0}, {15, 15, 15}, 50);

        auto deformableBodyEntity2 = em.CreateEntity("DeformableBody2");
        auto& body2 = em.AddComponent<Hex::DeformableBodyComponent>(deformableBodyEntity2);

        // --- EXAMPLE 2: Create a soft cloth ---
       CreateCloth(em, body2, defaultMat, {0, 5, 0}, 50, 50, 0.25f);


        // --- Create a static floor ---
        auto floor = em.CreateEntity("floor");
        em.AddComponent<Hex::TransformComponent>(floor, Hex::TransformComponent{{0.0f, -2.f, 0.0f},
            {}, {20.0f, 1.0f, 20.0f}});
        auto cubeMesh = Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/cube.obj");
        em.AddComponent<Hex::ModelComponent>(floor, Hex::ModelComponent{cubeMesh});
        em.AddComponent<Hex::MaterialComponent>(floor, Hex::MaterialComponent{defaultMat});
    };

    auto application = Hex::Application(spec, scene);
    application.Run();

    return 0;
}

