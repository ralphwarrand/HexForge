#pragma once

//Hex
#include "Renderer.h"

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
		[[nodiscard]] const RenderData GetRenderData() const;

		// Render defs
		[[nodiscard]] bool ShouldCullBackFaces() const;
		[[nodiscard]] bool ShouldShade() const;
	
	protected:
		GLuint m_vao{}, m_vbo{}, m_ubo{};
		std::shared_ptr<Shader> m_shader{ nullptr };
		Material* m_material{ nullptr };
		RenderData m_render_data;
		glm::mat4 m_model_matrix;

		bool m_buffers_initialized{false};
		bool m_cull_back_face{false};
		bool m_shaded{true};
	};

	class LineBatch final : public Primitive
	{
	public:
		LineBatch();

		LineBatch(const LineBatch&) = default;
		LineBatch(LineBatch&&) = default;

		LineBatch& operator = (const LineBatch&) = default;
		LineBatch& operator = (LineBatch&&) = default;

		void AddLine(const glm::vec3& start, const glm::vec3& end,  const glm::vec3& color = glm::vec3(1.0f));
		void InitBuffers() override;
		void Draw() override;
		
	private:
		std::vector<Vertex> m_vertices;
	};

	class SphereBatch final : public Primitive
	{
	public:
		explicit SphereBatch();
		~SphereBatch() override;

		SphereBatch(const SphereBatch&) = default;
		SphereBatch(SphereBatch&&) = default;

		SphereBatch& operator = (const SphereBatch&) = default;
		SphereBatch& operator = (SphereBatch&&) = default;

		void AddSphere(const glm::vec3& position = glm::vec3(0.f), const float& radius = 1.f,
			const glm::vec3& color = glm::vec3(1.0f), unsigned int sector_count = 36, unsigned int stack_count = 18
		);
		void InitBuffers() override;
		void Draw() override;

	private:
		GLuint m_ebo{};
		std::vector<Vertex> m_vertices;
		std::vector<unsigned int> m_indices;
	};
}
