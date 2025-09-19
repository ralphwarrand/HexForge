#pragma once

// STL
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <filesystem>
#include <iostream>

namespace Hex
{
    class Shader;
    class Texture;
    class Model;
    class Mesh;
    class Material;

    class ResourceManager
    {
    public:
        // Generic load: caches by key, constructs via T(args...)
        template<typename T, typename... Args>
        static std::shared_ptr<T> Load(const std::string& rawKey, Args&&... args) {
            auto& cache = GetCache<T>();

            // NormaliSe the key
            std::string key = NormaliseKey(rawKey);

            // First quick lookup
            {
                std::lock_guard lock(cache.mutex);
                auto it = cache.map.find(key);
                if (it != cache.map.end()) {
                    //std::cout << "[ResourceManager] cache hit for \"" << key << "\"\n";
                    return std::static_pointer_cast<T>(it->second);
                }
            }

            // Create resource outside lock
            auto resource = std::make_shared<T>(std::forward<Args>(args)...);

            // Insert, but check if someone beat us to it
            {
                std::lock_guard lock(cache.mutex);
                auto [it, inserted] = cache.map.emplace(key, resource);
                if (!inserted) {
                    std::cout << "[ResourceManager]  someone else inserted \"" << key << "\" first, reusing it\n";
                    return std::static_pointer_cast<T>(it->second);
                }
                std::cout << "[ResourceManager] loaded and cached \"" << key << "\"\n";
            }

            return resource;
        }

        // Custom-loader variant: you supply a lambda that returns shared_ptr<T>
        template<typename T>
        static std::shared_ptr<T> LoadWith(const std::string& rawKey, std::function<std::shared_ptr<T>()> loader) {
            auto& cache = GetCache<T>();
            std::string key = NormaliseKey(rawKey);

            {
                std::lock_guard lock(cache.mutex);
                auto it = cache.map.find(key);
                if (it != cache.map.end())
                    return std::static_pointer_cast<T>(it->second);
            }

            auto resource = loader();

            {
                std::lock_guard lock(cache.mutex);
                auto [it, inserted] = cache.map.emplace(key, resource);
                if (!inserted)
                    return std::static_pointer_cast<T>(it->second);
            }

            return resource;
        }

        // Clear caches
        template<typename T>
        static void Clear() {
            auto& cache = GetCache<T>();
            std::lock_guard lock(cache.mutex);
            cache.map.clear();
        }

        static void ClearAll() {
            Clear<Model>();
            Clear<Mesh>();
            Clear<Material>();
            Clear<Texture>();
            Clear<Shader>();
        }

        // --- Convenience loaders ---

        // Load an Assimp Model by filepath (key == filepath)
        static std::shared_ptr<Model> LoadModel(const std::string& filepath);

        // Load one mesh (sub-mesh) from file. Key == filepath#meshIndex
        static std::shared_ptr<Mesh> LoadMesh(const std::string& filepath, const unsigned int& meshIndex);

        static std::shared_ptr<Material> LoadMaterial(const std::string& vs,
            const std::string& fs, const std::string& albedoTex = "", const std::string& normalTex = "",
            const std::string& roughnessTex = "", const std::string& metallicTex = "",
            const std::string& aoTex = "");

        // Load an image file via stb_image into an OpenGL Texture, Key == filepath
        static std::shared_ptr<Texture> LoadTexture(const std::string& filepath, const bool& srgb = true);

        // Load a Shader by two file paths. Key == vsPath + "|" + fsPath
        static std::shared_ptr<Shader> LoadShader(const std::string& vsPath, const std::string& fsPath);

    private:
        // Internal cache type
        template<typename T>
        struct Cache {
            std::unordered_map<std::string, std::shared_ptr<void>> map;
            std::mutex mutex;
        };

        template<typename T>
        static Cache<T>& GetCache() {
            static Cache<T> cache;
            return cache;
        }

        static std::string Canonical(const std::string& path) {
            try {
                return std::filesystem::weakly_canonical(path).string();
            } catch (...) {
                return path; // fallback
            }
        }

        static std::string NormaliseKey(const std::string& raw) {
            //TODO: Implement key normalisation
            return raw;
        }
    };
}
