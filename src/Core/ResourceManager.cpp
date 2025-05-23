#include "pch.h"

#include <filesystem>

// Third-party
#include <stb_image/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Hex
#include "Core/ResourceManager.h"
#include "Renderer/Data/Model.h"
#include "Renderer/Data/Mesh.h"
#include "Renderer/Shader.h"
#include "Renderer/Data/Material.h"

namespace fs = std::filesystem;

namespace Hex
{
    std::shared_ptr<Model> ResourceManager::LoadModel(const std::string &filepath)
    {
        auto abs = Canonical(filepath);
        return Load<Model>(abs, abs);
    }

    std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string &filepath, const unsigned int & meshIndex)
    {
        std::string abs = Canonical(filepath);
        std::string key = abs + "#" + std::to_string(meshIndex);
        return LoadWith<Mesh>(key, [=]() {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(
                filepath,
                aiProcess_Triangulate |
                aiProcess_GenSmoothNormals |
                aiProcess_JoinIdenticalVertices |
                aiProcess_FlipUVs           |
                aiProcess_CalcTangentSpace |
                aiProcess_GenUVCoords
            );
            if (!scene || meshIndex >= scene->mNumMeshes) {
                throw std::runtime_error("ResourceManager::LoadMesh failed: " + filepath);
            }
            aiMesh* am = scene->mMeshes[meshIndex];
            // extract data
            std::vector<Vertex> verts; verts.reserve(am->mNumVertices);
            for (unsigned i = 0; i < am->mNumVertices; ++i) {
                Vertex v;
                v.pos    = {am->mVertices[i].x, am->mVertices[i].y, am->mVertices[i].z};
                v.normal = {am->mNormals[i].x,  am->mNormals[i].y,  am->mNormals[i].z};
                if (am->mTextureCoords[0])
                    v.uv = {am->mTextureCoords[0][i].x, am->mTextureCoords[0][i].y};
                else
                    v.uv = {0,0};

                if (am->HasTangentsAndBitangents()) {
                    const auto& at = am->mTangents[i];
                    const auto& ab = am->mBitangents[i];

                    glm::vec3 T = { at.x, at.y, at.z };
                    glm::vec3 B = { ab.x, ab.y, ab.z };
                    glm::vec3 N = v.normal;

                    // compute “handedness” – +1 or –1
                    float handedness = glm::dot(glm::cross(N, T), B) < 0.0f ? -1.0f : +1.0f;
                    v.tangent = glm::vec4(T, handedness);
                } else {
                    // fallback
                    v.tangent = glm::vec4(1,0,0, 1);
                }

                verts.push_back(v);
            }
            std::vector<uint32_t> idx;
            for (unsigned f = 0; f < am->mNumFaces; ++f) {
                const auto& face = am->mFaces[f];
                idx.insert(idx.end(), face.mIndices, face.mIndices + face.mNumIndices);
            }
            return std::make_shared<Mesh>(std::move(verts), std::move(idx));
        });
    }

    std::shared_ptr<Material> ResourceManager::LoadMaterial(const std::string &vs,
        const std::string &fs, const std::string &albedoTex, const std::string &normalTex,
        const std::string &roughnessTex, const std::string& metallicTex,
        const std::string& aoTex)
    {
        // auto-generate a key from all inputs
        std::string key =
            Canonical(vs) + "|" +
            Canonical(fs) + "|" +
            (albedoTex.empty()   ? "none" : Canonical(albedoTex))       + "|" +
            (normalTex.empty()   ? "none" : Canonical(normalTex))       + "|" +
            (roughnessTex.empty()? "none" : Canonical(roughnessTex))    + "|" +
            (metallicTex.empty()? "none" : Canonical(metallicTex))      + "|" +
            (aoTex.empty()? "none" : Canonical(aoTex));

        return LoadWith<Material>(key, [=]() {
            auto mat = std::make_shared<Material>();
            mat->shader = LoadShader(vs, fs);
            if (!albedoTex.empty())    mat->albedo_map    = LoadTexture(albedoTex);
            if (!normalTex.empty())    mat->normal_map    = LoadTexture(normalTex);
            if (!roughnessTex.empty()) mat->roughness_map = LoadTexture(roughnessTex);
            if (!metallicTex.empty()) mat->metallic_map = LoadTexture(metallicTex);
            if (!aoTex.empty()) mat->ao_map = LoadTexture(aoTex);
            return mat;
        });
    }

    std::shared_ptr<Texture> ResourceManager::LoadTexture(
        const std::string &filepath,
        const bool& srgb            // true → albedo, false → normals/roughness/metal/AO
    )
    {
        // 1) Build a cache key off a canonical path + srgb flag
        std::string absPath = Canonical(filepath);
        std::string key     = absPath + (srgb ? ":srgb" : ":linear");

        // 2) Defer actual work until first use in the cache
        return LoadWith<Texture>(key, [absPath, srgb]() {
            if (!std::filesystem::exists(absPath))
                throw std::runtime_error("Texture not found: " + absPath);

            // 3) Load *always* as 4 channels
            stbi_set_flip_vertically_on_load(false);
            int w=0,h=0,origChannels=0;
            unsigned char* data = stbi_load(
                absPath.c_str(), &w, &h, &origChannels,
                STBI_rgb_alpha    // <— force 4 channels out
            );
            if (!data)
                throw std::runtime_error(std::string("stb_image failed: ")
                                         + stbi_failure_reason());

            // 4) Pick the right internal format
            GLenum internalFmt = srgb
                ? GL_SRGB8_ALPHA8    // for albedo maps
                : GL_RGBA8;          // for all your linear data

            // 5) Create & bind the GL texture
            auto tex = std::make_shared<Texture>();
            tex->Bind();

            // Avoid alignment padding issues on non-4-byte rows
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            tex->SetWrap   (GL_REPEAT, GL_REPEAT);
            tex->SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);

            // 6) Upload exactly w*h*4 bytes
            glTexImage2D(GL_TEXTURE_2D,
                         0,                // mip level
                         internalFmt,      // sized internal format
                         w, h,             // width, height
                         0,                // border
                         GL_RGBA,          // format of 'data'
                         GL_UNSIGNED_BYTE, // type of 'data'
                         data);

            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(data);
            return tex;
        });
    }

    std::shared_ptr<Shader> ResourceManager::LoadShader(const std::string &vsPath, const std::string &fsPath)
    {
        const auto vs = Canonical(vsPath);
        const auto fs = Canonical(fsPath);
        std::string key = vs + "|" + fs;
        return LoadWith<Shader>(key, [=]() {
            return std::make_shared<Shader>(vsPath.c_str(), fsPath.c_str());
        });
    }
}
