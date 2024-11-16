//Hex
#include "Renderer/Shader.h"
#include "Renderer/Primitives/PrimitiveBase.h"
#include "Engine/Logger.h"

//Lib
#define GLM_ENABLE_EXPERIMENTAL
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
}