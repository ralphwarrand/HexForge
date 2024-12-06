#pragma once

//Hex
#include "EntityComponents.h"
#include "Engine/Logger.h"

//Lib
#include <entt/entt.hpp>

//STL
#include <iostream>
#include <string>
#include <unordered_map>

namespace Hex
{
    class EntityManager {
    public:
        //Tick entity components
        void TickComponents(const float& delta_time);

        // Create a new entity
        entt::entity CreateEntity(const std::string& name = "");

        // Get an entity by name
        entt::entity GetEntity(const std::string& name) const;

        // Check if an entity exists by name
        bool EntityExists(const std::string& name) const;

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
        template<typename Component, typename... Args>
        void AddComponent(entt::entity entity, Args&&... args) {
            registry.emplace<Component>(entity, std::forward<Args>(args)...);
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

        // Print all entities and their components (debugging utility)
        template<typename Component>
        void PrintEntitiesWithComponent() const
        {
            for (auto view = registry.view<Component>(); auto entity : view) {
                const auto& component = view.get<Component>(entity);
                Log(Hex::LogLevel::Debug, std::format("Entity: {} Component: {}",  static_cast<int>(entity) , component ));
            }
        }

    private:
        entt::registry registry;
        std::unordered_map<std::string, entt::entity> namedEntities;
    };
}
