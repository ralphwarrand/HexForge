#pragma once

// STL
#include <memory>

// Third-party
#include <glm/glm.hpp>

// Hex
#include "Renderer/Shader.h"
#include "Texture.h"

namespace Hex
{
    class Material {
    public:
        // simple Phong-ish parameters
        glm::vec3 ambient  = {0.1f, 0.1f, 0.1f};
        glm::vec3 diffuse  = {0.8f, 0.8f, 0.8f};
        glm::vec3 specular = {1.0f, 1.0f, 1.0f};
        float shininess = 32.0f;

        // optional texture maps
        std::shared_ptr<Texture>  albedo_map;
        std::shared_ptr<Texture>  normal_map;
        std::shared_ptr<Texture>  roughness_map;

        // the shader used to render this material
        std::shared_ptr<Shader> shader;

        // set all uniforms and bind textures
        void Apply() const;

    };
}
