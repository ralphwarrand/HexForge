#include "Renderer/Primitives/LineBatch.h"

namespace Hex
{

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

		if (m_line_batch_buffers_initialized) return;

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

		m_line_batch_buffers_initialized = true;
	}

	void LineBatch::Draw()
	{
		Primitive::Draw();

		// Draw the lines
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

		// Unbind the VAO
		glBindVertexArray(0);
	}
}