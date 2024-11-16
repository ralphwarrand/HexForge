#include "Engine/ResourceManagement/ResourceBase.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

namespace Hex
{
	class TextureResource : public ResourceBase
	{
	public:
		int width{0};
		int height{0};
		int channels{0};
		unsigned char* data{nullptr};

		~TextureResource()
		{
			if (data)
			{
				stbi_image_free(data);
			}
		}

		bool load(const std::string& filePath) override
		{
			path = filePath;

			// Use STB Image to load texture data
			data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
			if (!data)
			{
				std::cerr << "Failed to load texture: " << filePath << std::endl;
				return false;
			}

			std::cout << "Texture loaded: " << filePath << " (" << width << "x" << height << ")" << std::endl;
			return true;
		}
	};
}
