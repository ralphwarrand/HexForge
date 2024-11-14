//Hex
#include "Renderer/Shader.h"
#include "Renderer/Primitives.h"
#include "Engine/Logger.h"

//Lib
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace Hex
{
	Primitive::~Primitive()
	{
		delete m_material;
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

		const glm::vec3 direction = glm::normalize(end - start);
		m_dirs.push_back(direction);
		m_dirs.push_back(direction);

		m_colors.push_back(color);
		m_colors.push_back(color);
	}

	void LineBatch::InitBuffers()
	{
		Primitive::InitBuffers();

		if (m_buffers_initialized) return;

		glGenBuffers(1, &m_cbo);
		glGenBuffers(1, &m_dbo);

		// Initialize vertex buffer 
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(m_vertices.size() * sizeof(glm::vec3)),
			m_vertices.data(),
			GL_STATIC_DRAW
		);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(0);

		// Initialize color buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_cbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_colors.size() * sizeof(glm::vec3)), m_colors.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(1);

		// Initialize dirs buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_dbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_dirs.size() * sizeof(glm::vec3)), m_dirs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(2);
		
		// Unbind VAO and color buffer
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); 
	}

	void LineBatch::Draw()
	{
		Primitive::Draw();

		// Draw the lines
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

		// Unbind the VAO
		glBindVertexArray(0);
	}

	UVSphere::UVSphere(const float radius, const unsigned int sector_count, const unsigned int stack_count,
		glm::vec3 position)
	{
		m_cull_back_face = true;
		m_shaded = true;
		GenerateSphereVertices(radius, sector_count, stack_count);

		m_model_matrix = glm::translate(m_model_matrix, position);
	}

	UVSphere::~UVSphere()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);
	}

	void UVSphere::InitBuffers()
	{
		Primitive::InitBuffers();

		if (m_buffers_initialized) return;

		glGenBuffers(1, &m_ebo);

		// Generate VBO for vertex positions
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(glm::vec3)), m_vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(0);

		// Generate EBO for indices
		glGenBuffers(1, &m_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size()) * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

		// Generate VBO for normals (optional)
		GLuint normal_vbo;
		glGenBuffers(1, &normal_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, normal_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_normals.size() * sizeof(glm::vec3)), m_normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
		glEnableVertexAttribArray(1);

		glBindVertexArray(0); // Unbind the VAO
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
	}

	void UVSphere::Draw()
	{
		Primitive::Draw();

		// Draw the triangles
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);

		// Unbind the VAO
		glBindVertexArray(0);
	}

	void UVSphere::GenerateSphereVertices(const float radius, const unsigned int sector_count, const unsigned int stack_count)
	{
		auto pi = glm::pi<float>();

		const float length_inv = 1.0f / radius;

		const float sector_step = 2.f * pi / static_cast<float>(sector_count);
		const float stack_step = pi / static_cast<float>(stack_count);

		for (unsigned int i = 0; i <= stack_count; ++i) {
			const float stack_angle = pi / 2 - static_cast<float>(i) * stack_step;
			const float xy = radius * cosf(stack_angle);
			float z = radius * sinf(stack_angle);

			for (unsigned int j = 0; j <= sector_count; ++j) {
				const float sector_angle = static_cast<float>(j) * sector_step;

				float x = xy * cosf(sector_angle);
				float y = xy * sinf(sector_angle);
				m_vertices.emplace_back(x, y, z);

				const float nx = x * length_inv;
				const float ny = y * length_inv;
				const float nz = z * length_inv;
				glm::vec3 normal = {nx, ny, nz};
				normal = glm::normalize(normal);
				m_normals.emplace_back(normal);
			}
		}

		for (unsigned int i = 0; i < stack_count; ++i) {
			unsigned int k1 = i * (sector_count + 1);
			unsigned int k2 = k1 + sector_count + 1;

			for (unsigned int j = 0; j < sector_count; ++j, ++k1, ++k2) {
				if (i != 0) {
					m_indices.push_back(k1);
					m_indices.push_back(k2);
					m_indices.push_back(k1 + 1);
				}

				if (i != (stack_count - 1)) {
					m_indices.push_back(k1 + 1);
					m_indices.push_back(k2);
					m_indices.push_back(k2 + 1);
				}
			}
		}
	}
}
