#pragma once

// STL
#include <memory>

// Third-party
#include <glm/glm.hpp>

// Hex
#include "HexForge/Renderer/Shader.h"
#include "HexForge/Renderer/Data/Texture.h"

namespace Hex
{
    class Material {
    public:
        // optional texture maps
        std::shared_ptr<Texture>  albedo_map, normal_map, roughness_map, metallic_map, ao_map;

        // the shader used to render this material
        std::shared_ptr<Shader> shader;

        bool cull_backfaces = true;

        // set all uniforms and bind textures
        void Apply() const;

    };
}
