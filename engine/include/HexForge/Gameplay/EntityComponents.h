#pragma once

// STL
#include <memory>

// Third-Party
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include<glm/gtx/quaternion.hpp>
#include <glm/detail/type_quat.hpp>

// Hex

#include "entt/entt.hpp"
#include "HexForge/Renderer/Data/Mesh.h"
#include "HexForge/Renderer/Data/Material.h"
#include "HexForge/Renderer/Data/Model.h"

namespace Hex {
	struct TransformComponent
	{
		glm::vec3 position{0.0f};
		glm::quat orientation{};
		glm::vec3 scale{1.0f};

		[[nodiscard]] glm::mat4 GetMatrix() const {
			glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
			m *= glm::toMat4(orientation);
			return glm::scale(m, scale);
		}
	};

	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
	};

	struct ModelComponent
	{
		std::shared_ptr<Model> model;
	};

	struct MaterialComponent
	{
		std::shared_ptr<Material> material;
	};

	struct RotatingComponent
	{
		float rate{10.f};
		glm::vec3 axis{0.f, 1.f, 0.f};
	};


	//************ PHYSICS COMPONENTS **********************************************************************************

	// --- Physics Components for PBD ---

	// Represents a single particle in the physics simulation
	struct ParticleComponent {
		glm::vec3 predictedPosition = {0.0f, 0.0f, 0.0f};
		glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
		float inverseMass = 1.0f; // 0 for static/infinite mass
	};

	// A constraint to maintain distance between two particles
	struct DistanceConstraint {
		entt::entity p1;
		entt::entity p2;
		float restLength;
		float compliance; // Inverse of stiffness. 0.0 = infinitely stiff.
		float lambda;     // Accumulated Lagrange multiplier.

		DistanceConstraint(entt::entity p1, entt::entity p2, float restLength, float compliance)
	   : p1(p1), p2(p2), restLength(restLength), compliance(compliance), lambda(0.0f) {}
	};

	// A constraint to maintain the volume of a tetrahedron
	struct VolumeConstraint {
		entt::entity p1, p2, p3, p4;
		float restVolume;
		float compliance; // Compliance for volume preservation.
		float lambda;     // Accumulated Lagrange multiplier.

		VolumeConstraint(entt::entity p1, entt::entity p2, entt::entity p3, entt::entity p4, float restVolume, float compliance)
	   : p1(p1), p2(p2), p3(p3), p4(p4), restVolume(restVolume), compliance(compliance), lambda(0.0f) {}
	};

	// An entity that holds a set of constraints for a deformable body
	struct DeformableBodyComponent {
		std::vector<DistanceConstraint> distanceConstraints;
		std::vector<VolumeConstraint> volumeConstraints;
	};


	enum class ColliderType {
		Sphere,
		Box
	};

	struct ColliderComponent {
		ColliderType type;
		glm::vec3 size; // radius for sphere, half-extents for box
	};
}

