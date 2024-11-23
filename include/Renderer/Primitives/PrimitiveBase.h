#pragma once

//Hex
#include "Renderer/Renderer.h"

//LIB
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glad/glad.h>

//STL
#include <vector>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec3 tangent;
};

namespace Hex
{
	// Forward declarations
	class Shader;
	
	class Primitive
	{
	public:
		Primitive() : m_model_matrix(glm::mat4(1.0f)) {}
		virtual ~Primitive();

		Primitive(const Primitive&) = default;
		Primitive(Primitive&&) = default;

		Primitive& operator = (const Primitive&) = default;
		Primitive& operator = (Primitive&&) = default;
		
		virtual void InitBuffers();
		virtual void Draw();

		// Getters and setters
		void SetModelMatrix(const glm::mat4& model_matrix);
		[[nodiscard]] const glm::mat4& GetModelMatrix() const;

		void SetShaderProgram(const std::shared_ptr<Shader>& shader);
		[[nodiscard]] Shader* GetShaderProgram() const;

		void SetMaterial(Material* material);
		[[nodiscard]] const Material* GetMaterial() const;

		void SetRenderData(const RenderData& render_data);
		[[nodiscard]] RenderData GetRenderData() const;

		// Render defs
		void SetShouldShade(const bool& should_shade);
		[[nodiscard]] bool ShouldShade() const;


		[[nodiscard]] bool ShouldCullBackFaces() const;

	
	protected:
		GLuint m_vao{}, m_vbo{}, m_ubo{};
		std::shared_ptr<Shader> m_shader{ nullptr };
		Material* m_material{ nullptr };
		RenderData m_render_data;
		glm::mat4 m_model_matrix;

		bool m_buffers_initialized{false};
		bool m_cull_back_face{false};
		bool m_shaded{true};
		bool m_render_data_needs_update{true};
	};
}