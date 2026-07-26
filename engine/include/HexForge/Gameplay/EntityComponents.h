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
#include "HexForge/Physics/PhysicsMaterial.h"

namespace Hex
{
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
	struct DistanceConstraintComponent
	{
		entt::entity entityA;
		entt::entity entityB;
		float restDistance;
	};

	struct FluidComponent
	{
		float density = 0.0f;
	};

	// Per-particle physical material. The PhysicsSystem deduplicates these at upload time —
	// two particles whose PhysicsMaterial values compare bitwise-equal share a material slot
	// on the GPU, so the small AoS table stays cache-friendly even with many particles.
	struct PhysicsMaterialComponent
	{
		PhysicsMaterial material{};
	};

	// Marks a particle as a member of a shape-matching rigid body. All particles sharing the
	// same `rigidBodyId` will be treated as one rigid cluster — every step the solver fits a
	// rigid transform to their predicted positions and pulls them back toward the rest shape.
	struct RigidBodyMemberComponent
	{
		uint32_t rigidBodyId = 0; // arbitrary scene-defined id
		glm::vec3 restLocalPos = {0.0f, 0.0f, 0.0f}; // rest position in the body's local frame
	};

	// Per-rigid-body parameters (one entity per body; the body's id is referenced from member
	// particles). The PhysicsSystem reads this at init to size the GPU buffers.
	struct RigidBodyComponent
	{
		uint32_t rigidBodyId = 0;
		float stiffness = 1.0f; // [0,1] fraction of the shape-matching correction applied per step
	};

	// Optional: attach to a rigid-body entity to draw the original source mesh at the body's
	// shape-matched transform (centroid + orientation). When this is present, the renderer will
	// SKIP drawing the body's constituent particles in Mesh/Both modes so the mesh isn't covered
	// by a cloud of voxel spheres. The model/material live here so a body can be rendered as the
	// bunny it was sampled from while still being simulated as XPBD particles.
	struct RigidBodyVisualComponent
	{
		std::shared_ptr<Model> model;
		std::shared_ptr<Material> material;
		glm::vec3 meshScale{1.0f};            // applied on top of body's pose
		glm::vec3 centroidOffset{0.0f};       // mesh space → body centroid alignment (rest cm in mesh space)
	};

	// Grid-topology cloth — stores the regular nu×nv layout of particle entities so the renderer
	// can build a dynamic triangle mesh from their per-frame TransformComponent positions. The
	// solver doesn't see this component; it's purely a rendering aid.
	struct ClothComponent
	{
		std::vector<entt::entity> particles; // row-major, size = nu * nv
		int nu = 0;
		int nv = 0;
		glm::vec4 color{ 0.85f, 0.30f, 0.55f, 1.0f };
	};
}

