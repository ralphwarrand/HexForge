#include "pch.h"

#include "Core/Application.h"
#include "Gameplay/EntityComponents.h"
#include "Gameplay/EntityManager.h"

int main()
{
    Hex::AppSpecification spec;
    spec.name = "Sandbox";
    spec.width = 1920;
    spec.height = 1080;
    spec.fullscreen = false;
    spec.vsync = false;

    auto scene = [&](Hex::EntityManager& em)
    {
        auto testMat = Hex::ResourceManager::LoadMaterial(
            RESOURCES_PATH "shaders/debug.vert", RESOURCES_PATH "shaders/debug.frag",
            RESOURCES_PATH "textures/debug/test.bmp",  // albedo
            RESOURCES_PATH "textures/debug/test.bmp"   // normal map
        );

        auto bunnyMat = Hex::ResourceManager::LoadMaterial(
            RESOURCES_PATH "shaders/debug.vert",
            RESOURCES_PATH "shaders/debug.frag",
            RESOURCES_PATH "textures/Rock061_2K-JPG/Rock061_2K-JPG_Color.jpg",
            RESOURCES_PATH "textures/Rock061_2K-JPG/Rock061_2K-JPG_NormalGL.jpg",
            RESOURCES_PATH "textures/Rock061_2K-JPG/Rock061_2K-JPG_Roughness.jpg",
            "",
            RESOURCES_PATH "textures/Rock061_2K-JPG/Rock061_2K-JPG_AmbientOcclusion.jpg"
        );

        for (int i = -10; i < 10; i++)
        {
            for (int j = -10; j < 10; j++)
            {
                auto e = em.CreateEntity("entity" + i + j);
                em.AddComponent<Hex::TransformComponent>(e, Hex::TransformComponent{
                    {i * 0.5f, 0.0f, j * 0.5f},
                    glm::quat{},
                    {2.f, 2.f, 2.f}
                });

                auto bunnyMesh = Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/bunny.obj");
                em.AddComponent<Hex::ModelComponent>(
                    e, Hex::ModelComponent{ bunnyMesh }
                );

                em.AddComponent<Hex::MaterialComponent>(
                    e, Hex::MaterialComponent{ bunnyMat }
                );

                em.AddComponent<Hex::RotatingComponent>(
                    e, Hex::RotatingComponent{ glm::linearRand(10.f, 20.f), {0.f, 1.f, 0.f}}
                );
            }
        }

        auto e = em.CreateEntity("floor");
        em.AddComponent<Hex::TransformComponent>(e, Hex::TransformComponent{
            {0.0f, -2.f, 0.0f},
            glm::quat{},
            {15.0f, 1.0f, 15.0f}
        });

        auto cubeMesh = Hex::ResourceManager::LoadModel(RESOURCES_PATH "models/cube.obj");
        em.AddComponent<Hex::ModelComponent>(
            e, Hex::ModelComponent{ cubeMesh }
        );

        em.AddComponent<Hex::MaterialComponent>(
            e, Hex::MaterialComponent{ bunnyMat }
        );
    };

    const auto application = Hex::Application(spec, scene);
    application.Run();

    return 0;
}
