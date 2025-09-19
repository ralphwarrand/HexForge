#pragma once

// STL
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <iostream>
#include <fmt/format.h>

// ----------------- Third-party libs
// Windowing and render context
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// Imgui
#define IMGUI_ENABLE_DOCKING
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// GLM
#define GLM_FORCE_SILENT_WARNINGS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

// Hex
#include "Core/Logger.h"
#include "Gameplay/EntityComponents.h"
#include "Core/Application.h"
#include "Core/Console.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Shader.h"
#include "Renderer/Camera.h"
#include "Renderer/Data/Material.h"
#include "Renderer/Data/ScreenQuad.h"