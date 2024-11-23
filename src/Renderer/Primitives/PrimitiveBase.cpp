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

		// Clear old buffers if they exist
		if (m_vao != 0 || m_vbo != 0 || m_ubo != 0) {
			Log(LogLevel::Debug, "Old buffers were deleted");
			glDeleteVertexArrays(1, &m_vao);
			glDeleteBuffers(1, &m_vbo);
			glDeleteBuffers(1, &m_ubo);
		}

		// Generate new buffers
		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);
		glGenBuffers(1, &m_ubo);

		if (m_vao == 0 || m_vbo == 0 || m_ubo == 0) {
			Log(LogLevel::Error, "VAO, VBO, or UBO generation failed!");
			return;
		}

		// Bind VAO
		glBindVertexArray(m_vao);

		// Set up the UBO
		glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(RenderData), nullptr, GL_DYNAMIC_DRAW); // Allocate memory

		constexpr GLuint binding_point = 0;

		// Retrieve uniform block index
		const GLuint block_index = glGetUniformBlockIndex(m_shader->GetProgramID(), "RenderData");
		if (block_index == GL_INVALID_INDEX) {
			Log(LogLevel::Error, "Uniform block 'RenderData' not found in shader!");
			return;
		}

		// Bind the UBO to the uniform block
		glUniformBlockBinding(m_shader->GetProgramID(), block_index, binding_point);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, m_ubo);

		// Upload initial data
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderData), &m_render_data);

		// Unbind UBO and VAO
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindVertexArray(0);

		m_buffers_initialized = true;
	}

	void Primitive::Draw()
	{
		// Initialize buffers if not already done
		if (!m_buffers_initialized) {
			InitBuffers();
			m_buffers_initialized = true;
		}

		if (m_ubo == 0) {
			Log(LogLevel::Error, "Uniform Buffer Object (UBO) is not initialized!");
			return;
		}

		// Bind the VAO (vertex array object) for rendering
		glBindVertexArray(m_vao);

		// Update the UBO with the latest RenderData
		// Check if UBO needs updating
		if (m_ubo != 0 && m_render_data_needs_update) {
			glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderData), &m_render_data);
			m_render_data_needs_update = false;
		}
		//glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind UBO after updating (optional)

		// If needed, bind the UBO to the binding point (done during initialization usually)
		//constexpr GLuint binding_point = 0;
		//glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, m_ubo); // Uncomment only if necessary
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

	void Primitive::SetRenderData(const RenderData& render_data) {
		m_render_data = render_data;
		m_render_data_needs_update = true; // Flag to update UBO in Draw
	}

	RenderData Primitive::GetRenderData() const
	{
		return m_render_data;
	}

	void Primitive::SetShouldShade(const bool &should_shade)
	{
		m_shaded = should_shade;
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