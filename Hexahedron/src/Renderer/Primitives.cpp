//Hex
#include "Renderer/Shader.h"
#include "Renderer/Primitives.h"
#include "Engine/Logger.h"

//Lib
#include <iostream>
#include <glm/glm.hpp>

namespace Hex
{
	void Primitive::SetModelMatrix(const glm::mat4& model_matrix)
	{
		m_model_matrix = model_matrix;
	}

	const glm::mat4& Primitive::GetModelMatrix() const
	{
		return m_model_matrix;
	}

	void Primitive::SetShaderProgram(Shader* shader)
	{
		m_shader = shader;
	}

	Shader* Primitive::GetShaderProgram() const
	{
		return m_shader;
	}

	LineBatch::LineBatch()
	{
		m_model_matrix = glm::mat4(1.0f);
	}

	LineBatch::~LineBatch()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_cbo);
	}

	void LineBatch::AddLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
	{
		m_vertices.push_back(start);
		m_vertices.push_back(end);

		m_colors.push_back(color);
		m_colors.push_back(color);
	}

	void LineBatch::InitBuffers()
	{
		// Clear old buffers
		if (m_vao != 0 || m_vbo != 0 || m_cbo != 0) {
			Log(LogLevel::Debug, "Old buffers were deleted");
			glDeleteVertexArrays(1, &m_vao);
			glDeleteBuffers(1, &m_vbo);
			glDeleteBuffers(1, &m_cbo);
		}
		
		// Gen buffers
		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);
		if (glGetError() != GL_NO_ERROR) {
			Log(LogLevel::Error, "Failed to upload buffer");
		}
		
		glGenBuffers(1, &m_cbo); // Color buffer

		if (m_vao == 0 || m_vbo == 0 || m_cbo == 0) {
			Log(LogLevel::Error, "VAO or VBO/CBO generation failed!");
		}
		
		// Bind VAO before setting attributes
		glBindVertexArray(m_vao); 

		// Initialize vertex buffer 
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER,
			m_vertices.size() * sizeof(glm::vec3),
			m_vertices.data(),
			GL_STATIC_DRAW
		);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(0);

		// Initialize color buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_cbo);
		glBufferData(GL_ARRAY_BUFFER, m_colors.size() * sizeof(glm::vec3), m_colors.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(1);
		
		// Unbind VAO and color buffer
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); 
	}

	void LineBatch::Draw()
	{
		// Initialize buffers only once
		if (!m_vertices.empty() && !m_buffers_initialised)
		{
			InitBuffers();
			m_buffers_initialised = true;
		}

		m_shader->Bind();

		m_shader->SetUniformMat4("model", m_model_matrix);

		// Bind the VAO
		glBindVertexArray(m_vao);

		// Draw the lines
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

		// Unbind the VAO and shader program
		glBindVertexArray(0);
	}
}
