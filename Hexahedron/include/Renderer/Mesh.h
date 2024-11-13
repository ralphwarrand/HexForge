#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

class Mesh {
public:
	// Constructor
	Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

	// Destructor
	~Mesh();

	// Bind and render the mesh
	void Draw() const;

private:
	// Render data
	unsigned int VAO, VBO, EBO;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	// Set up the mesh
	void setupMesh();
};
