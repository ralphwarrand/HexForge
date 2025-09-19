#pragma once

// Third-party
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace Hex
{
	// Forward declarations
	struct AppSpecification;
	class Camera;
	class Shader;
	class Mesh;
	struct ScreenQuad;

	struct alignas(16) RenderData
	{
		glm::mat4 view;           // 64 bytes (16-byte alignment)
		glm::mat4 projection;     // 64 bytes (16-byte alignment)

		glm::vec3 view_pos;       // 12 bytes
		float     padding1;       // 4 bytes (to align to 16)

		glm::vec3 light_dir;      // 12 bytes
		float     padding2;       // 4 bytes to align to 16)

		glm::vec3 light_color;    // 12 bytes
		float     padding3;       // 4 bytes (to align to 16)

		bool      wireframe;      // 4 bytes (std140 bool→int)
		float     padding4[3];    // 12 bytes (to align struct size to 16)

		bool operator==(const RenderData& other) const = default;
	};

	struct RenderItem
	{
		Material*      material;
		Mesh*          mesh;
		glm::mat4      modelMatrix;
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