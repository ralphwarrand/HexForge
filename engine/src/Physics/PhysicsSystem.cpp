// Hex
#include "HexForge/pch.h"
#include "HexForge/Physics/PhysicsSystem.h"
#include "HexForge/Gameplay/EntityManager.h"
#include "HexForge/Gameplay/EntityComponents.h"

// Third-party
#include <glm/gtx/norm.hpp>
#include <unordered_map>

namespace Hex
{
    void PhysicsSystem::Tick(EntityManager& entityManager, float deltaTime, float currentTime)
    {
        // Add the real-world frame time to the accumulator
        m_timeAccumulator += deltaTime;

        // Run the simulation in fixed steps as many times as needed to catch up
        while (m_timeAccumulator >= m_fixedTimeStep)
        {
            SimulateStep(entityManager, m_fixedTimeStep);
            m_timeAccumulator -= m_fixedTimeStep;
            m_totalTime += m_fixedTimeStep; // Increment total simulation time
        }
    }

    void PhysicsSystem::SimulateStep(EntityManager& entityManager, float fixedDeltaTime)
    {
        if (fixedDeltaTime <= 0.0f || m_solverIterations == 0) return;

        auto view = entityManager.GetRegistry().view<TransformComponent, ParticleComponent>();

        // Store original positions from the start of the frame.
        std::unordered_map<entt::entity, glm::vec3> originalPositions;
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            originalPositions[entity] = transform.position;
        }

        // --- 1. EXPLICIT PREDICTION STEP (Algorithm 1, line 1) ---
        // The XPBD algorithm begins with an explicit prediction of where particles will be
        // at the end of the timestep, including external forces like gravity and wind.
        for (auto entity : view) {
            auto& particle = view.get<ParticleComponent>(entity);
            if (particle.inverseMass > 0.0f) {
                // --- Calculate Wind Force ---
                // A sine wave gives a gusting effect, and turbulence is based on particle position.
                float wave = sin(m_totalTime * m_windFrequency + originalPositions.at(entity).x * m_turbulence);
                glm::vec3 windForce = m_windDirection * m_windStrength * wave;
                glm::vec3 windAcceleration = windForce * particle.inverseMass;

                // Combine all external accelerations
                glm::vec3 totalAcceleration = m_gravity + windAcceleration;

                // Predict position using current velocity and applying total acceleration.
                particle.predictedPosition = originalPositions.at(entity)
                                           + particle.velocity * fixedDeltaTime
                                           + totalAcceleration * fixedDeltaTime * fixedDeltaTime;
            } else {
                // Static objects don't move.
                particle.predictedPosition = originalPositions.at(entity);
            }
        }

        // --- 2. Initialize Lagrange Multipliers (Algorithm 1, line 4) ---
        // These are reset once per frame before the solver begins.
        auto bodyView = entityManager.GetRegistry().view<DeformableBodyComponent>();
        for (auto bodyEntity : bodyView) {
            auto& body = bodyView.get<DeformableBodyComponent>(bodyEntity);
            for (auto& constraint : body.distanceConstraints) constraint.lambda = 0.0f;
            for (auto& constraint : body.volumeConstraints) constraint.lambda = 0.0f;
        }

        // --- 3. IMPLICIT-LIKE SOLVER LOOP (Algorithm 1, lines 5-13) ---
        // This is the core of XPBD. The loop iteratively corrects the predicted positions
        // to satisfy constraints. This process approximates a true implicit solve,
        // which gives the method its stability and iteration-independent stiffness.
        for (int i = 0; i < m_solverIterations; ++i) {
            SolveConstraints(entityManager, fixedDeltaTime);
        }

        // --- 4. Update Final State (Algorithm 1, lines 15-16) ---
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& particle = view.get<ParticleComponent>(entity);

            if (particle.inverseMass > 0.0f) {
                // Velocity is updated once at the end based on the total displacement.
                particle.velocity = (particle.predictedPosition - originalPositions.at(entity)) / fixedDeltaTime;

                // Simple velocity damping helps stabilize the simulation by removing any excess energy.
                particle.velocity *= 0.995f;

                // Update the final renderable transform position.
                transform.position = particle.predictedPosition;
            }
        }
    }

    void PhysicsSystem::SolveConstraints(EntityManager& entityManager, float deltaTime)
    {
        auto bodyView = entityManager.GetRegistry().view<DeformableBodyComponent>();
        for (auto bodyEntity : bodyView)
        {
            auto& body = bodyView.get<DeformableBodyComponent>(bodyEntity);

            for (auto& constraint : body.distanceConstraints)
            {
                SolveDistanceConstraint(entityManager, constraint, deltaTime);
            }
            for (auto& constraint : body.volumeConstraints)
            {
                SolveVolumeConstraint(entityManager, constraint, deltaTime);
            }
        }
        ProjectCollisionConstraints(entityManager);
    }

    void PhysicsSystem::SolveDistanceConstraint(EntityManager& entityManager, DistanceConstraint& constraint, float deltaTime)
    {
        auto& p1 = entityManager.GetComponent<ParticleComponent>(constraint.p1);
        auto& p2 = entityManager.GetComponent<ParticleComponent>(constraint.p2);

        float totalInverseMass = p1.inverseMass + p2.inverseMass;
        if (totalInverseMass == 0.0f) return;

        glm::vec3 delta = p2.predictedPosition - p1.predictedPosition;
        float currentDist = glm::length(delta);
        if (currentDist < 1e-9f) return;

        glm::vec3 correctionDir = delta / currentDist;

        // This is Equation (18) from the XPBD paper.
        // alpha_tilde is calculated with the full deltaTime to ensure iteration count independence.
        float C = currentDist - constraint.restLength;
        float alpha_tilde = constraint.compliance / (deltaTime * deltaTime);

        float denominator = totalInverseMass + alpha_tilde;
        if (glm::abs(denominator) < 1e-9f) return;

        float delta_lambda = -(C + alpha_tilde * constraint.lambda) / denominator;

        // Accumulate lambda.
        constraint.lambda += delta_lambda;

        // Apply position correction, derived from Equation (17). The signs are crucial here.
        glm::vec3 correction = correctionDir * delta_lambda;
        p1.predictedPosition -= p1.inverseMass * correction;
        p2.predictedPosition += p2.inverseMass * correction;
    }

    void PhysicsSystem::SolveVolumeConstraint(EntityManager& entityManager, VolumeConstraint& constraint, float deltaTime)
    {
        auto& p1 = entityManager.GetComponent<ParticleComponent>(constraint.p1);
        auto& p2 = entityManager.GetComponent<ParticleComponent>(constraint.p2);
        auto& p3 = entityManager.GetComponent<ParticleComponent>(constraint.p3);
        auto& p4 = entityManager.GetComponent<ParticleComponent>(constraint.p4);

        float currentVolume = TetVolume(p1.predictedPosition, p2.predictedPosition, p3.predictedPosition, p4.predictedPosition);
        float C = currentVolume - constraint.restVolume;
        if (glm::abs(C) < 1e-9) return;

        glm::vec3 grad1 = glm::cross(p2.predictedPosition - p3.predictedPosition, p4.predictedPosition - p3.predictedPosition) / 6.0f;
        glm::vec3 grad2 = glm::cross(p3.predictedPosition - p1.predictedPosition, p4.predictedPosition - p1.predictedPosition) / 6.0f;
        glm::vec3 grad3 = glm::cross(p4.predictedPosition - p2.predictedPosition, p1.predictedPosition - p2.predictedPosition) / 6.0f;
        glm::vec3 grad4 = glm::cross(p1.predictedPosition - p3.predictedPosition, p2.predictedPosition - p3.predictedPosition) / 6.0f;

        float sum_grad_sq = p1.inverseMass * glm::length2(grad1) +
                              p2.inverseMass * glm::length2(grad2) +
                              p3.inverseMass * glm::length2(grad3) +
                              p4.inverseMass * glm::length2(grad4);

        if (sum_grad_sq < 1e-9) return;

        // alpha_tilde is calculated with the full deltaTime to ensure iteration count independence.
        float alpha_tilde = constraint.compliance / (deltaTime * deltaTime);
        float delta_lambda = -(C + alpha_tilde * constraint.lambda) / (sum_grad_sq + alpha_tilde);

        constraint.lambda += delta_lambda;

        // Apply position corrections, derived from Equation (17).
        p1.predictedPosition += delta_lambda * p1.inverseMass * grad1;
        p2.predictedPosition += delta_lambda * p2.inverseMass * grad2;
        p3.predictedPosition += delta_lambda * p3.inverseMass * grad3;
        p4.predictedPosition += delta_lambda * p4.inverseMass * grad4;
    }

    void PhysicsSystem::ProjectCollisionConstraints(EntityManager& entityManager)
    {
        auto view = entityManager.GetRegistry().view<ParticleComponent>();

        // --- Floor Collision ---
        for (auto entity : view)
        {
            auto& particle = view.get<ParticleComponent>(entity);
            if (particle.predictedPosition.y < -2.0f)
            {
                particle.predictedPosition.y = -2.0f;
            }
        }
    }

    void PhysicsSystem::SetMousePicker(entt::entity entity)
    {
        m_mousePickerEntity = entity;
    }

    entt::entity PhysicsSystem::GetMousePicker() const
    {
        return m_mousePickerEntity;
    }

    void PhysicsSystem::UpdateMousePickerPosition(EntityManager& entityManager, const glm::vec3& worldPosition) const
    {
        if (m_mousePickerEntity != entt::null)
        {
            auto& transform = entityManager.GetComponent<TransformComponent>(m_mousePickerEntity);
            transform.position = worldPosition;
        }
    }

    float PhysicsSystem::TetVolume(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4)
    {
        return glm::dot(p2 - p1, glm::cross(p3 - p1, p4 - p1)) / 6.0f;
    }
}

