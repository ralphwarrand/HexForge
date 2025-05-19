#include "Renderer/Data/Material.h"

namespace Hex {

    void Material::Apply() const
    {
        shader->Bind();

        // Material flags
        shader->SetUniform1i("hasAlbedoMap",    albedo_map  ? 1 : 0);
        shader->SetUniform1i("hasNormalMap",    normal_map  ? 1 : 0);
        shader->SetUniform1i("hasRoughnessMap", roughness_map ? 1 : 0);
        //shader->SetUniform1i("should_shade",    shouldShade ? 1 : 0);

        // Bind samplers
        int unit = 0;
        if (albedo_map) {
            glActiveTexture(GL_TEXTURE0 + unit);
            albedo_map->Bind();
            shader->SetUniform1i("albedoMap", unit++);
        }
        if (normal_map) {
            glActiveTexture(GL_TEXTURE0 + unit);
            normal_map->Bind();
            shader->SetUniform1i("normalMap", unit++);
        }
        if (roughness_map) {
            glActiveTexture(GL_TEXTURE0 + unit);
            roughness_map->Bind();
            shader->SetUniform1i("roughnessMap", unit++);
        }
    }
}
