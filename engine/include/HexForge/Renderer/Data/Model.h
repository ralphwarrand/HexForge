#pragma once

// STL
#include <vector>
#include <memory>
#include <string>

// Hex
#include "HexForge/Renderer/Data/Mesh.h"
#include "HexForge/Core/ResourceManager.h"

namespace Hex
{
    class Model
    {
    public:
        Model() = default;
        Model(const std::string& path);
        explicit Model(std::vector<std::shared_ptr<Mesh>> in_meshes)
            : meshes(std::move(in_meshes)) {}

        void Draw() const;

        [[nodiscard]] const std::vector<std::shared_ptr<Mesh>> &GetMeshes() const { return meshes; }
        void AddMesh(std::shared_ptr<Mesh> mesh) { meshes.push_back(std::move(mesh)); }

    private:
        std::vector<std::shared_ptr<Mesh> > meshes;
    };
}
