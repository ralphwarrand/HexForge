#pragma once

//Hex
#include "Renderer/Shader.h"

//STL
#include <unordered_map>
#include <memory>
#include <string>

namespace Hex
{
	class ShaderManager
	{
	public:
		static std::shared_ptr<Shader> GetOrCreateShader(const std::string& vertex_path, const std::string& fragment_path);

	private:
		static std::unordered_map<std::string, std::shared_ptr<Shader>> s_shader_cache;
		static std::string GenerateKey(const std::string& vertex_path, const std::string& fragment_path);
	};
}