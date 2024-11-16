//Hex
#include "Renderer/Primitives/SphereBatch.h"

//Lib
#include <glm/ext/scalar_constants.hpp>

namespace Hex
{
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
		const auto pi = glm::pi<float>();

		const float sector_step = 2.f * pi / static_cast<float>(sector_count);
		const float stack_step = pi / static_cast<float>(stack_count);

		// Track current vertex offset
		const unsigned int vertex_offset = static_cast<unsigned int>(m_vertices.size());

		for (unsigned int i = 0; i <= stack_count; ++i) {
			const float stack_angle = pi / 2 - static_cast<float>(i) * stack_step;
			const float xy = radius * cosf(stack_angle);
			const float z = radius * sinf(stack_angle);

			for (unsigned int j = 0; j <= sector_count; ++j) {
				const float sector_angle = static_cast<float>(j) * sector_step;

				const float x = xy * cosf(sector_angle);
				const float y = xy * sinf(sector_angle);

				glm::vec3 local_position = {x, y, z};
				glm::vec3 world_position = local_position + position;
				const glm::vec3 normal = glm::normalize(world_position - position);
				const glm::vec3 tangent = glm::normalize(glm::vec3(-y, x, 0.0f)); // Arbitrary tangent

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