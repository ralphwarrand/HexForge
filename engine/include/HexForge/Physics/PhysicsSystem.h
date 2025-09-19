#pragma once

// Third-Party
#include <glm/glm.hpp>
#include <entt/entt.hpp>

#include "HexForge/Gameplay/EntityComponents.h"

namespace Hex
{
    // Forward declarations
    class EntityManager;

    class PhysicsSystem
    {
    public:
        PhysicsSystem() = default;

        void Tick(EntityManager& entityManager, float deltaTime, float currentTime);

        // Mouse Picker Methods
        void SetMousePicker(entt::entity entity);
        entt::entity GetMousePicker() const;
        void UpdateMousePickerPosition(EntityManager& entityManager, const glm::vec3& worldPosition) const;


        int m_solverIterations = 40;
        glm::vec3 m_gravity = {0.0f, -9.81f, 0.0f};
        entt::entity m_mousePickerEntity = entt::null;

        // Wind parameters
        glm::vec3 m_windDirection = {0.0f, -1.0f, 1.0f};
        float m_windStrength = 16.f;
        float m_windFrequency = 0.2f;
        float m_turbulence = 5.0f;

        // Fixed timestep members
        float m_timeAccumulator = 0.0f;
        float m_totalTime = 0.0f; // Total elapsed simulation time
        const float m_fixedTimeStep = 1.0f / 60.0f; // Physics updates at a fixed 60Hz

    private:
        void SimulateStep(EntityManager& entityManager, float fixedDeltaTime);
        void SolveConstraints(EntityManager& entityManager, float deltaTime);
        void SolveDistanceConstraint(EntityManager& entityManager, DistanceConstraint& constraint, float deltaTime);
        void SolveVolumeConstraint(EntityManager& entityManager, VolumeConstraint& constraint, float deltaTime);
        void ProjectCollisionConstraints(EntityManager& entityManager);
        static float TetVolume(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4);


    };
}

