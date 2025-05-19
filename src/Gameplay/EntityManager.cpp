#include "pch.h"

//Hex
#include "Gameplay/EntityManager.h"
#include "Gameplay/EntityComponents.h"

namespace Hex
{
	void EntityManager::TickComponents(const float& delta_time)
	{
		// iterate all entities with a Transform + Rotating
		auto view = registry.view<TransformComponent, RotatingComponent>();
		for (auto entity : view)
		{
			auto &tf = view.get<TransformComponent>(entity);
			auto &rc = view.get<RotatingComponent>(entity);

			// compute the small rotation quaternion for this frame
			float angle_rad = glm::radians(rc.rate * delta_time);
			glm::quat dq    = glm::angleAxis(angle_rad, glm::normalize(rc.axis));

			// apply it to the current orientation
			tf.orientation = glm::normalize(dq * tf.orientation);
		}
    }

	entt::entity EntityManager::CreateEntity(const std::string& name)
	{
		entt::entity entity = registry.create();
		if (!name.empty()) {
			namedEntities[name] = entity;
		}
		return entity;
	}

	entt::entity EntityManager::GetEntity(const std::string& name) const
    {
		auto it = namedEntities.find(name);
		if (it != namedEntities.end()) {
			return it->second;
		}
		throw std::runtime_error("Entity with name '" + name + "' not found.");
	}

	bool EntityManager::EntityExists(const std::string& name) const
    {
		return namedEntities.find(name) != namedEntities.end();
	}

	void EntityManager::Clear() {
		namedEntities.clear();
		registry.clear();
	}

}
