#include "Engine/ResourceManagement/ResourceManager.h"

namespace Hex
{
	void ResourceManager::unloadResource(const std::string& path)
	{
		size_t key = generateHash(path);
		resources_.erase(key);
	}

	void ResourceManager::clearResources()
	{
		resources_.clear();
	}
}