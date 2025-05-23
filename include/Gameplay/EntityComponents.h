#pragma once

// STL
#include <memory>

// Third-Party
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include<glm/gtx/quaternion.hpp>
#include <glm/detail/type_quat.hpp>

// Hex
#include "Renderer/Data/Mesh.h"
#include "Renderer/Data/Model.h"
#include "Renderer/Data/Material.h"

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
}