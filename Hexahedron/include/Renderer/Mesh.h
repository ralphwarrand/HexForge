#pragma once

// Lib
#include <vector>
#include <glm/glm.hpp>
#include <gl/glew.h>

namespace Hex
{
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv_coords;
	};
	
	class Mesh {
	public:
		Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
		~Mesh();

		Mesh(const Mesh&) = delete;
		Mesh(Mesh&&) = delete;

		Mesh& operator = (const Mesh&) = delete;
		Mesh& operator = (Mesh&&) = delete;
	
		// Bind and render the mesh
		void Draw() const;
	
	private:
		GLuint m_vao, m_vbo, m_ebo;
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
	
		// Set up the mesh
		void InitBuffers();
	};
}