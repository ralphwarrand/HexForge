#pragma once

//Hex
#include "Renderer.h"

//LIB
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <gl/glew.h>

//STL
#include <vector>

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
		void SetShaderProgram(Shader* shader);
		[[nodiscard]] Shader* GetShaderProgram() const;
		void SetMaterial(Material* material);
		[[nodiscard]] const Material* GetMaterial() const;
		void SetRenderData(const RenderData& render_data);

		// Render defs
		[[nodiscard]] bool ShouldCullBackFaces() const;
		[[nodiscard]] bool ShouldShade() const;
	
	protected:
		GLuint m_vao{}, m_vbo{}, m_ubo{};
		Shader* m_shader{ nullptr };
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
		~LineBatch() override;

		LineBatch(const LineBatch&) = default;
		LineBatch(LineBatch&&) = default;

		LineBatch& operator = (const LineBatch&) = default;
		LineBatch& operator = (LineBatch&&) = default;

		void AddLine(const glm::vec3& start, const glm::vec3& end,  const glm::vec3& color = glm::vec3(1.0f));
		void InitBuffers() override;
		void Draw() override;
		
	private:
		GLuint m_cbo{}, m_dbo{};
		std::vector<glm::vec3> m_vertices;
		std::vector<glm::vec3> m_normals;
		std::vector<glm::vec3> m_colors;
		std::vector<glm::vec3> m_dirs;
	};

	class UVSphere: public Primitive
	{
	public:
		explicit UVSphere(float radius = 1.0f, unsigned int sector_count = 36, unsigned int stack_count = 18,
			glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));
		~UVSphere() override;

		UVSphere(const UVSphere&) = default;
		UVSphere(UVSphere&&) = default;

		UVSphere& operator = (const UVSphere&) = default;
		UVSphere& operator = (UVSphere&&) = default;

		void InitBuffers() override;
		void Draw() override;

	private:
		GLuint m_ebo{};
		std::vector<glm::vec3> m_vertices;
		std::vector<glm::vec3> m_normals;
		std::vector<unsigned int> m_indices;

		void GenerateSphereVertices(float radius, unsigned int sector_count, unsigned int stack_count);
	};
}
