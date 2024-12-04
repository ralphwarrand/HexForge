#pragma once

//Hex
#include "EntityComponents.h"

//Lib
#include <entt/entt.hpp>

//STL
#include <iostream>
#include <string>
#include <unordered_map>

namespace Hex
{

    class EntityManager {
    private:
        entt::registry registry;
        std::unordered_map<std::string, entt::entity> namedEntities;

    public:
        //Tick entity components
        void TickComponents()
        {

        }

        // Create a new entity
        entt::entity CreateEntity(const std::string& name = "") {
            entt::entity entity = registry.create();
            if (!name.empty()) {
                namedEntities[name] = entity;
            }
            return entity;
        }

        // Get an entity by name
        entt::entity GetEntity(const std::string& name) const {
            auto it = namedEntities.find(name);
            if (it != namedEntities.end()) {
                return it->second;
            }
            throw std::runtime_error("Entity with name '" + name + "' not found.");
        }

        // Check if an entity exists by name
        bool EntityExists(const std::string& name) const {
            return namedEntities.find(name) != namedEntities.end();
        }

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
        void Clear() {
            namedEntities.clear();
            registry.clear();
        }

        // Print all entities and their components (debugging utility)
        template<typename Component>
        void PrintEntitiesWithComponent() const
        {
            for (auto view = registry.view<Component>(); auto entity : view) {
                const auto& component = view.get<Component>(entity);
                Log(LogLevel::Debug, std::format("Entity: {} Component: {}",  static_cast<int>(entity) , component ));
            }
        }
    };
}