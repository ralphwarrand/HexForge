#pragma once
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Renderer/Shader.h"

namespace Hex
{

	class Primitive
	{
	public:
		Primitive() : m_model_matrix(glm::mat4(1.0f)) {}
		virtual ~Primitive() = default;
		virtual void Draw() = 0;

		void SetModelMatrix(const glm::mat4& model_matrix)
		{
			m_model_matrix = model_matrix;
		}

		[[nodiscard]] const glm::mat4& GetModelMatrix() const
		{
			return m_model_matrix;
		}

		void SetShaderProgram(Shader* shader)
		{
			this->m_shader = shader;
		}
	
		[[nodiscard]] Shader* GetShaderProgram()
		{
			return m_shader;
		}
	
	protected:
		Shader* m_shader;
		glm::mat4 m_model_matrix;
	};

	class LineBatch: public Primitive
	{
	public:
		LineBatch()
		{
			m_model_matrix = glm::mat4(1.0f);
			//gen buffers
			glGenVertexArrays(1, &m_vao);
			glGenBuffers(1, &m_vbo);
			glGenBuffers(1, &m_cbo); // Color buffer

			m_shader = new Shader("resources/shaders/line.vert", "resources/shaders/line.frag");
		}

		~LineBatch() override {
			glDeleteVertexArrays(1, &m_vao);
			glDeleteBuffers(1, &m_vbo);
			glDeleteBuffers(1, &m_cbo);
		}

		void AddLine(const glm::vec3& start, const glm::vec3& end,  const glm::vec3& color = glm::vec3(1.0f))
		{
			m_vertices.push_back(start);
			m_vertices.push_back(end);

			m_colors.push_back(color);
			m_colors.push_back(color);

			InitBuffers();
		}

		void InitBuffers() const
		{
			glBindVertexArray(m_vao);

			glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
			glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(glm::vec3), m_vertices.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
			glEnableVertexAttribArray(0);

			// Initialize color buffer
			glBindBuffer(GL_ARRAY_BUFFER, m_cbo);
			glBufferData(GL_ARRAY_BUFFER, m_colors.size() * sizeof(glm::vec3), m_colors.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
			glEnableVertexAttribArray(1);

			// Unbind VAO
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}

		void Draw() override
		{
			m_shader->SetUniformMat4("model", m_model_matrix);

			// Bind the VAO
			glBindVertexArray(m_vao);

			// Draw the lines
			glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

			// Unbind the VAO and shader program
			glBindVertexArray(0);
			glUseProgram(0);
		}

	
	private:
		GLuint m_vao, m_vbo, m_cbo;
		std::vector<glm::vec3> m_vertices;
		std::vector<glm::vec3> m_colors;
	};

	class Sphere: public Primitive
	{
	
	};
}