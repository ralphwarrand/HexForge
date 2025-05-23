#include "pch.h"

// Hex
#include "Renderer/Data/Model.h"

namespace Hex
{
    Model::Model(const std::string &path)
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

    void Model::Draw() const
    {
        for (const auto& mesh : meshes)
        {
            mesh->Draw();
        }
    }
}
