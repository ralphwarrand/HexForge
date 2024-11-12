#pragma once

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
		virtual ~Primitive() = default;

		Primitive(const Primitive&) = default;
		Primitive(Primitive&&) = default;

		Primitive& operator = (const Primitive&) = default;
		Primitive& operator = (Primitive&&) = default;
		
		virtual void Draw() = 0;

		// Getters and setters
		void SetModelMatrix(const glm::mat4& model_matrix);
		[[nodiscard]] const glm::mat4& GetModelMatrix() const;
		void SetShaderProgram(Shader* shader);
		[[nodiscard]] Shader* GetShaderProgram() const;
	
	protected:
		Shader* m_shader{ nullptr };
		glm::mat4 m_model_matrix;
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
		void InitBuffers() const;
		void Draw() override;
		
	private:
		GLuint m_vao, m_vbo, m_cbo;
		std::vector<glm::vec3> m_vertices;
		std::vector<glm::vec3> m_colors;
	};

	class Sphere: public Primitive
	{
	
	};
}