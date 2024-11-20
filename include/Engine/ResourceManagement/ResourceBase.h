#pragma once

// STL
#include <string>
#include <Renderer/Primitives/PrimitiveBase.h>

namespace Hex
{

	class ResourceBase
	{
		public:

		virtual ~ResourceBase() = default;

		virtual bool load(const std::string& filePath) = 0;

		std::string path;
	};

	class ModelResource : ResourceBase
	{
	public:
		std::vector<Vertex> m_vertices;
		std::vector<unsigned int> m_indices;
	};
}
