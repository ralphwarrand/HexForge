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
        Model(const std::string& path);

        // Draws all the sub-meshes in this model.
        void Draw() const;

        [[nodiscard]] const std::vector<std::shared_ptr<Mesh>> &GetMeshes() const { return meshes; }

    private:
        std::vector<std::shared_ptr<Mesh> > meshes;
    };
}
