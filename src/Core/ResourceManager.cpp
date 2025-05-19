#include "pch.h"

#include "Core/ResourceManager.h"

// Third-party
#include <stb_image/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Hex
#include "Renderer/Data/Model.h"
#include "Renderer/Data/Mesh.h"
#include "Renderer/Shader.h"
#include "Renderer/Data/Material.h"

namespace Hex
{
    std::shared_ptr<Model> ResourceManager::LoadModel(const std::string &filepath)
    {
        return Load<Model>(filepath, filepath);
    }

    std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string &filepath, unsigned meshIndex)
    {
        std::string key = filepath + "#" + std::to_string(meshIndex);
        return LoadWith<Mesh>(key, [=]() {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(
                filepath,
                aiProcess_Triangulate |
                aiProcess_GenSmoothNormals |
                aiProcess_FlipUVs
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

    std::shared_ptr<Material> ResourceManager::LoadMaterial(const std::string &key, const std::string &vs,
        const std::string &fs, const std::string &albedoTex, const std::string &normalTex,
        const std::string &roughnessTex)
    {
        return LoadWith<Material>(key, [=]() {
            auto mat = std::make_shared<Material>();
            mat->shader = LoadShader(vs, fs);
            if (!albedoTex.empty())
                mat->albedo_map   = LoadTexture(albedoTex);
            if (!normalTex.empty())
                mat->normal_map   = LoadTexture(normalTex);
            if (!roughnessTex.empty())
                mat->roughness_map= LoadTexture(roughnessTex);
            return mat;
        });
    }

    std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string &filepath, bool srgb)
    {
        return LoadWith<Texture>(filepath, [=]() {
            int w,h,channels;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);
            if (!data) throw std::runtime_error("Failed to load texture: " + filepath);
            const GLenum fmt = (channels == 3) ? GL_RGB : GL_RGBA;
            const GLenum internalFmt = (srgb && channels==3) ? GL_SRGB8 : fmt;

            auto tex = std::make_shared<Texture>();
            tex->Bind();
            tex->SetWrap(GL_REPEAT, GL_REPEAT);
            tex->SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(data);
            return tex;
        });
    }

    std::shared_ptr<Shader> ResourceManager::LoadShader(const std::string &vsPath, const std::string &fsPath)
    {
        std::string key = vsPath + "|" + fsPath;
        return LoadWith<Shader>(key, [=]() {
            return std::make_shared<Shader>(vsPath.c_str(), fsPath.c_str());
        });
    }
}
