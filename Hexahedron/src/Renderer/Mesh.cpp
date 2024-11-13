#include "Renderer/Mesh.h"

namespace Hex
{
	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	{
		
	}

	Mesh::~Mesh()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);
	}

	void Mesh::Draw() const
	{
	
	}

	void Mesh::InitBuffers()
	{
		
	}
}
