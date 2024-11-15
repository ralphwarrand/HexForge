//Hex
#include "Renderer/Shader.h"
#include "Renderer/Primitives.h"
#include "Engine/Logger.h"

//Lib
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

namespace Hex
{
	Primitive::~Primitive()
	{
		delete m_material;
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ubo);
	}

	void Primitive::InitBuffers()
	{
		if (m_buffers_initialized) return;
		
		// Clear old buffers
		if (m_vao != 0 || m_vbo != 0 || m_ubo != 0) {
			Log(LogLevel::Debug, "Old buffers were deleted");
			glDeleteVertexArrays(1, &m_vao);
			glDeleteBuffers(1, &m_vbo);
			glDeleteBuffers(1, &m_ubo);
		}
		
		// Gen buffers
		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);
		glGenBuffers(1, &m_ubo);

		if (m_vao == 0 || m_vbo == 0 || m_ubo == 0) {
			Log(LogLevel::Error, "VAO or VBO/UBO generation failed!");
		}

		// Bind VAO before setting attributes
		glBindVertexArray(m_vao);
		glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(RenderData), nullptr, GL_DYNAMIC_DRAW); // Allocate memory
		
		constexpr GLuint binding_point = 0;
		const GLuint block_index = glGetUniformBlockIndex(m_shader->GetProgramID(), "RenderData");
		glUniformBlockBinding(m_shader->GetProgramID(), block_index, binding_point);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, m_ubo);
		
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderData), &m_render_data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind when done
	}

	void Primitive::Draw()
	{
		if (!m_buffers_initialized) {
			InitBuffers();
			m_buffers_initialized = true;
		}

		// Bind the VAO
		glBindVertexArray(m_vao);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo);
	}

	void Primitive::SetModelMatrix(const glm::mat4& model_matrix)
	{
		m_model_matrix = model_matrix;
	}

	const glm::mat4& Primitive::GetModelMatrix() const
	{
		return m_model_matrix;
	}

	void Primitive::SetShaderProgram(const std::shared_ptr<Shader>& shader)
	{
		m_shader = shader;
	}

	Shader* Primitive::GetShaderProgram() const
	{
		return m_shader.get();
	}

	void Primitive::SetMaterial(Material* material)
	{
		m_material = material;
	}

	const Material* Primitive::GetMaterial() const
	{
		return m_material;
	}

	void Primitive::SetRenderData(const RenderData& render_data)
	{
		if (!m_buffers_initialized) {
			InitBuffers();
			m_buffers_initialized = true;
		}
		
		m_render_data = render_data;

		glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(RenderData), nullptr, GL_DYNAMIC_DRAW); // Orphan old data
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderData), &m_render_data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	const RenderData Primitive::GetRenderData() const
	{
		return m_render_data;
	}

	bool Primitive::ShouldCullBackFaces() const
	{
		return m_cull_back_face;
	}

	bool Primitive::ShouldShade() const
	{
		return m_shaded;
	}

	LineBatch::LineBatch()
	{
		m_cull_back_face = false;
		m_shaded = false; //shading on lines is not working yet
	}

	void LineBatch::AddLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
	{
		const glm::vec3 direction = glm::normalize(end - start);
		const auto normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default normal (arbitrary)
		const glm::vec3 tangent = direction;                 // Tangent aligns with direction

		// Add start vertex
		m_vertices.push_back({start, color, normal, tangent});
		// Add end vertex
		m_vertices.push_back({end, color, normal, tangent});
	}

	void LineBatch::InitBuffers()
	{
		Primitive::InitBuffers();

		if (m_buffers_initialized) return;

		glGenBuffers(1, &m_vbo);

		// Initialize vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER,
					 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
					 m_vertices.data(),
					 GL_STATIC_DRAW);

		// Set up vertex attributes
		glBindVertexArray(m_vao);

		// Position attribute (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
		glEnableVertexAttribArray(0);

		// Color attribute (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
		glEnableVertexAttribArray(1);

		// Normal attribute (location = 2)
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));
		glEnableVertexAttribArray(2);

		// Tangent attribute (location = 3)
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, tangent)));
		glEnableVertexAttribArray(3);

		// Unbind VAO and buffer
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		m_buffers_initialized = true;
	}

	void LineBatch::Draw()
	{
		Primitive::Draw();

		// Draw the lines
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

		// Unbind the VAO
		glBindVertexArray(0);
	}

	SphereBatch::SphereBatch()
	{
		m_cull_back_face = false;
		m_shaded = true;
	}

	SphereBatch::~SphereBatch()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);
	}

	void SphereBatch::AddSphere(const glm::vec3& position, const float& radius,
							const glm::vec3& color, const unsigned int sector_count, const unsigned int stack_count)
	{
		auto pi = glm::pi<float>();

		const float sector_step = 2.f * pi / static_cast<float>(sector_count);
		const float stack_step = pi / static_cast<float>(stack_count);

		// Track current vertex offset
		unsigned int vertex_offset = static_cast<unsigned int>(m_vertices.size());

		for (unsigned int i = 0; i <= stack_count; ++i) {
			const float stack_angle = pi / 2 - static_cast<float>(i) * stack_step;
			const float xy = radius * cosf(stack_angle);
			const float z = radius * sinf(stack_angle);

			for (unsigned int j = 0; j <= sector_count; ++j) {
				const float sector_angle = static_cast<float>(j) * sector_step;

				float x = xy * cosf(sector_angle);
				float y = xy * sinf(sector_angle);

				glm::vec3 local_position = {x, y, z};
				glm::vec3 world_position = local_position + position;
				glm::vec3 normal = glm::normalize(world_position - position);
				glm::vec3 tangent = glm::normalize(glm::vec3(-y, x, 0.0f)); // Arbitrary tangent

				// Add vertex to global list
				m_vertices.push_back({world_position, color, normal, tangent});
			}
		}

		// Generate indices
		for (unsigned int i = 0; i < stack_count; ++i) {
			unsigned int k1 = i * (sector_count + 1) + vertex_offset;
			unsigned int k2 = k1 + sector_count + 1;

			for (unsigned int j = 0; j < sector_count; ++j, ++k1, ++k2) {
				if (i != 0) {
					m_indices.push_back(k1);
					m_indices.push_back(k1 + 1);
					m_indices.push_back(k2);
				}

				if (i != (stack_count - 1)) {
					m_indices.push_back(k1 + 1);
					m_indices.push_back(k2 + 1);
					m_indices.push_back(k2);
				}
			}
		}
	}

	void SphereBatch::InitBuffers()
	{
		Primitive::InitBuffers();

		if (m_buffers_initialized) return;

		glGenBuffers(1, &m_ebo);

		// Initialize VBO for vertex data
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_STATIC_DRAW);

		glBindVertexArray(m_vao);

		// Position attribute (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
		glEnableVertexAttribArray(0);

		// Color attribute (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
		glEnableVertexAttribArray(1);

		// Normal attribute (location = 2)
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(2);

		// Tangent attribute (location = 3)
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
		glEnableVertexAttribArray(3);

		// Initialize EBO for indices
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)), m_indices.data(), GL_STATIC_DRAW);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		m_buffers_initialized = true;
	}

	void SphereBatch::Draw()
	{
		Primitive::Draw();

		glBindVertexArray(m_vao);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
	}
}
