#pragma once

// STL
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>

// Third-party
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Hex
#include "Renderer/Data/Mesh.h"
#include "Core/ResourceManager.h"

namespace Hex
{
    class Model
    {
    public:
        // Loads all sub-meshes from `path` into the shared ResourceManager cache.
        // Throws std::runtime_error if the file fails to load.
        Model(const std::string& path)
        {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_Triangulate
              | aiProcess_GenSmoothNormals
              | aiProcess_FlipUVs
            );

            if (!scene || !scene->HasMeshes()) {
                throw std::runtime_error("Model::Model — failed to load or no meshes in: " + path);
            }

            // Reserve space for a small speedup
            meshes.reserve(scene->mNumMeshes);

            // For each aiMesh in the scene, load (or fetch) it from ResourceManager
            for (unsigned i = 0; i < scene->mNumMeshes; ++i)
            {
                // This will either load + cache, or return the existing one.
                auto meshPtr = ResourceManager::LoadMesh(path, i);
                meshes.push_back(meshPtr);
            }
        }

        // Draws all the sub-meshes in this model.
        void Draw() const
        {
            for (const auto& mesh : meshes)
            {
                mesh->Draw();
            }
        }

    private:
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
}
