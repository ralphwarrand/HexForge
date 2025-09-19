// Hex
#include "HexForge/pch.h"
#include "Renderer/Data/Material.h"

namespace Hex
{

    void Material::Apply() const {
        shader->Bind();

        // Feature flags
        shader->SetUniform1i("hasAlbedoMap",    albedo_map    ? 1 : 0);
        shader->SetUniform1i("hasNormalMap",    normal_map    ? 1 : 0);
        shader->SetUniform1i("hasRoughnessMap", roughness_map ? 1 : 0);
        shader->SetUniform1i("hasMetallicMap",  metallic_map  ? 1 : 0);
        shader->SetUniform1i("hasAoMap",        ao_map        ? 1 : 0);

        // Bind samplers 0..4
        static const char* names[5] = {
            "albedoMap","normalMap","roughnessMap","metallicMap","aoMap"
          };
        std::shared_ptr<Texture> texs[5] = {
            albedo_map, normal_map,
            roughness_map, metallic_map,
            ao_map
          };

        for (int unit = 0; unit < 5; ++unit) {
            // tell GLSL this sampler lives in texture unit `unit`
            shader->SetUniform1i(names[unit], unit);
            glActiveTexture(GL_TEXTURE0 + unit);
            if (texs[unit]) {
                texs[unit]->Bind(unit);
            } else {
                // if it’s the normal slot, bind default normal; otherwise bind white
                if (unit == 1) Texture::BindDefaultNormal();
                else          Texture::BindWhite();
            }
        }

        // Backface culling
        if (cull_backfaces) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
}
