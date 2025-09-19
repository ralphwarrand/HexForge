#pragma once

//STL
#include <string>
#include <unordered_map>

// Third-party
#include <entt/entt.hpp>

//Hex
#include "HexForge/Core/Logger.h"

namespace Hex
{
    class EntityManager {
    public:

        entt::registry& GetRegistry() { return registry; }
        const entt::registry& GetRegistry() const { return registry; }

        //Tick entity components
        void TickComponents(const float& delta_time);

        // Create a new entity
        entt::entity CreateEntity(const std::string& name = "");

        // Get an entity by name
        [[nodiscard]] entt::entity GetEntity(const std::string& name) const;

        // Check if an entity exists by name
        [[nodiscard]] bool EntityExists(const std::string& name) const;

        // Destroy an entity
        void DestroyEntity(entt::entity entity) {
            for (auto it = namedEntities.begin(); it != namedEntities.end();) {
                if (it->second == entity) {
                    it = namedEntities.erase(it);
                } else {
                    ++it;
                }
            }
            registry.destroy(entity);
        }

        // Destroy all entities
        void Clear();

        // Add a component to an entity
        template<typename T, typename... Args>
        T& AddComponent(entt::entity entity, Args&&... args) {
            return registry.emplace<T>(entity, std::forward<Args>(args)...);
        }

        // Get a component from an entity
        template<typename Component>
        Component& GetComponent(entt::entity entity) {
            return registry.get<Component>(entity);
        }

        // Check if an entity has a specific component
        template<typename Component>
        bool HasComponent(entt::entity entity) const {
            return registry.any_of<Component>(entity);
        }

        // Remove a component from an entity
        template<typename Component>
        void RemoveComponent(entt::entity entity) {
            registry.remove<Component>(entity);
        }

        // Print all entities with component
        template <typename Component>
        void PrintEntitiesWithComponent() const
        {
            // Iterate over entities in the view
            for (auto view = registry.view<Component>(); const auto entity : view) {
                // Use the template keyword to disambiguate the dependent name
                const auto& component = view.template get<Component>(entity);

                // Log the entity and its component
                Log(Hex::LogLevel::Debug, std::printf("Entity: %i Component: {}", static_cast<int>(entity)));
            }
        }

    private:
        entt::registry registry;
        std::unordered_map<std::string, entt::entity> namedEntities;
    };
}
