//Hex
#include "Renderer/ShaderManager.h"

namespace Hex
{
	std::unordered_map<std::string, std::shared_ptr<Shader>> ShaderManager::s_shader_cache;

	std::string ShaderManager::GenerateKey(const std::string& vertex_path, const std::string& fragment_path)
	{
		return vertex_path + "|" + fragment_path; // Combine paths into a unique key
	}

	std::shared_ptr<Shader> ShaderManager::GetOrCreateShader(const std::string& vertex_path, const std::string& fragment_path)
	{
		const std::string key = GenerateKey(vertex_path, fragment_path);

		// Check if shader already exists
		if (const auto it = s_shader_cache.find(key); it != s_shader_cache.end())
		{
			return it->second; // Return existing shader
		}

		// Create and store the shader if not found
		auto shader = std::make_shared<Shader>(vertex_path, fragment_path);
		s_shader_cache[key] = shader;
		return shader;
	}
}