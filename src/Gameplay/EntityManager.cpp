//Hex
#include "Gameplay/EntityManager.h"

namespace Hex
{
	void EntityManager::TickComponents(const float& delta_time)
	{
		// Iterate over entities with both Position and Velocity components
		registry.view<Hex::Position, Hex::Velocity>().each(
        	[&delta_time](Hex::Position& pos, const Hex::Velocity& vel)
        	{
				pos.value += vel.value * delta_time; // Update position by velocity
			}
        );

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