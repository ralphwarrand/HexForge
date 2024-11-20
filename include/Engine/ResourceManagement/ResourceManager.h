#pragma once

// STL
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include "ResourceBase.h"
#include <iostream>

namespace Hex
{
    class ResourceManager
    {
    public:
        // Constructor
        ResourceManager() = default;

        // Destructor
        ~ResourceManager() = default;

        // Loads a resource and stores it in the manager
        template<typename ResourceType>
        std::shared_ptr<ResourceType> loadResource(const std::string& path);

        // Retrieves a resource by its path
        template<typename ResourceType>
        std::shared_ptr<ResourceType> getResource(const std::string& path) const;

        // Removes a resource from the manager using its path
        void unloadResource(const std::string& path);

        // Clears all resources
        void clearResources();

    private:
        // Internal storage for resources (maps hash of path to resource instance)
        std::unordered_map<size_t, std::shared_ptr<ResourceBase>> resources_;

        // Helper function to generate hash key for a given path
        static size_t generateHash(const std::string& path)
        {
            return std::hash<std::string>{}(path);
        }
    };

    template<typename ResourceType>
    std::shared_ptr<ResourceType> ResourceManager::loadResource(const std::string& path)
    {
        static_assert(std::is_base_of<ResourceBase, ResourceType>::value, "ResourceType must derive from Resource");

        size_t key = generateHash(path);

        // Check if the resource is already loaded
        auto it = resources_.find(key);
        if (it != resources_.end())
        {
            return std::dynamic_pointer_cast<ResourceType>(it->second);
        }

        // Create the resource and load its data
        auto resource = std::make_shared<ResourceType>();
        if (!resource->load(path))
        {
            std::cerr << "Failed to load resource: " << path << std::endl;
            return nullptr;
        }

        // Store the loaded resource
        resources_[key] = resource;
        return resource;
    }

    // Implementation of getResource
    template<typename ResourceType>
    std::shared_ptr<ResourceType> ResourceManager::getResource(const std::string& path) const
    {
        static_assert(std::is_base_of<ResourceBase, ResourceType>::value, "ResourceType must derive from Resource");

        // Generate hash key from the path
        size_t key = generateHash(path);

        auto it = resources_.find(key);
        if (it != resources_.end())
        {
            return std::dynamic_pointer_cast<ResourceType>(it->second);
        }
        return nullptr; // Return nullptr if the resource isn't found
    }
}