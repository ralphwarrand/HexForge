#pragma once

//Lib
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace Hex
{
	// Forward declarations
	struct AppSpecification;
	class Camera;
	class Shader;
	class Mesh;

	// Primitives
	class Primitive;
	class LineBatch;
	class SphereBatch;
	class CubeBatch;
	struct ScreenQuad;

	struct Material
	{
		glm::vec3 ambient_color;
		glm::vec3 diffuse_color;
		glm::vec3 specular_color;
		float shininess;
	};

	struct alignas(16) RenderData
	{
		glm::mat4 view;         // 64 bytes (16-byte alignment)
		glm::mat4 projection;   // 64 bytes (16-byte alignment)
		glm::vec3 view_pos;     // 12 bytes
		float padding1;         // 4 bytes (to align to 16 bytes)
		glm::vec3 light_pos;    // 12 bytes
		float padding2;         // 4 bytes (to align to 16 bytes)
		bool wireframe;         // 4 bytes (std140 treats bool as 4-byte int)
		float padding3[3];      // 12 bytes (to align struct size to 16 bytes)
		bool operator==(const RenderData & render_data) const = default;
	};

	struct ShadowMap
	{
		GLuint fbo{0};         // Framebuffer for shadow mapping
		GLuint texture{0};     // Texture to store the depth information
		glm::mat4 light_view{glm::mat4(1.f)};          // View matrix for the light
		glm::mat4 light_projection{glm::mat4(1.f)};    // Projection matrix for the light
		int shadow_width{4096}, shadow_height{4096};
	};

	struct FrameBuffer
	{
		GLuint frame_buffer{0};
		GLuint texture{0};
		GLuint depth_render_buffer{0};
		unsigned int render_width{100}, render_height{100};
	};
}