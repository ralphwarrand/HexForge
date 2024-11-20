//Hex
#include "Renderer/Primitives/SphereBatch.h"

//Lib
#include <iostream>
#include <glm/ext/scalar_constants.hpp>

namespace Hex
{
	SphereBatch::SphereBatch()
	{
		m_buffers_initialized = false;
		m_cull_back_face = true;
		m_shaded = true;
	}

	SphereBatch::~SphereBatch()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);
	}

	void SphereBatch::AddSphere(const glm::vec3& position, const float& radius,
	                            const glm::vec3& color, const unsigned int sector_count, const unsigned int stack_count)
	{
	    const auto pi = glm::pi<float>();

	    const float sector_step = 2.f * pi / static_cast<float>(sector_count);
	    const float stack_step = pi / static_cast<float>(stack_count);

	    // Temporary storage for vertex indices
	    std::vector<std::vector<unsigned int>> vertex_indices(stack_count + 1, std::vector<unsigned int>(sector_count + 1));

	    for (unsigned int i = 0; i <= stack_count; ++i) {
	        const float stack_angle = pi / 2 - static_cast<float>(i) * stack_step; // From pi/2 to -pi/2
	        const float xy = radius * cosf(stack_angle); // Radius of circle at stack
	        const float z = radius * sinf(stack_angle); // Z position

	        for (unsigned int j = 0; j <= sector_count; ++j) {
	            const float sector_angle = static_cast<float>(j) * sector_step; // From 0 to 2pi

	            // Compute local position on the sphere
	            const float x = xy * cosf(sector_angle);
	            const float y = xy * sinf(sector_angle);

	            glm::vec3 local_position = {x, y, z};
	            glm::vec3 world_position = local_position + position;
	            const glm::vec3 normal = glm::normalize(local_position);
	            glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal));
	            if (glm::length(tangent) < 0.0001f) {
	                tangent = glm::normalize(glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), normal));
	            }

	            // Check if this vertex already exists, else add it
	            if (vertex_indices[i][j] == 0) {
	                vertex_indices[i][j] = static_cast<unsigned int>(m_vertices.size());
	                m_vertices.push_back({world_position, color, normal, tangent});
	            }
	        }
	    }

	    // Generate indices
	    for (unsigned int i = 0; i < stack_count; ++i) {
	        for (unsigned int j = 0; j < sector_count; ++j) {
	            unsigned int k1 = vertex_indices[i][j];
	            unsigned int k2 = vertex_indices[i + 1][j];
	            unsigned int k3 = vertex_indices[i][j + 1];
	            unsigned int k4 = vertex_indices[i + 1][j + 1];

	            if (i != 0) { // Top triangle
	                m_indices.push_back(k1);
	                m_indices.push_back(k2);
	                m_indices.push_back(k3);
	            }

	            if (i != stack_count - 1) { // Bottom triangle
	                m_indices.push_back(k3);
	                m_indices.push_back(k2);
	                m_indices.push_back(k4);
	            }
	        }
	    }
	}

	void SphereBatch::InitBuffers()
	{
		Primitive::InitBuffers();

		if (m_sphere_buffers_initialized) return;

		glBindVertexArray(m_vao);

		glGenBuffers(1, &m_ebo);
		if (m_ebo == 0) {
			std::cerr << "Error: Failed to generate Element Buffer Object (EBO)" << std::endl;
		}

		// Initialize VBO for vertex data
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_STATIC_DRAW);


		// Position attribute (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
		glEnableVertexAttribArray(0);

		// Color attribute (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
		glEnableVertexAttribArray(1);

		// Normal attribute (location = 2)
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(2);

		// Tangent attribute (location = 3)
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
		glEnableVertexAttribArray(3);

		if (m_indices.empty()) {
			std::cerr << "Error: No indices available to initialize EBO" << std::endl;
			return;
		}
		// Initialize EBO for indices
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)), m_indices.data(), GL_STATIC_DRAW);

		if (const GLenum error = glGetError(); error != GL_NO_ERROR) {
			std::cerr << "OpenGL Error: " << error << " during EBO initialization" << std::endl;
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		m_sphere_buffers_initialized = true;
	}

	void SphereBatch::Draw()
	{
		// Call to the parent class's Draw method.
		Primitive::Draw();

		try {
			// Initialize buffers if not already done
			if (!m_buffers_initialized) {
				InitBuffers();
				m_buffers_initialized = true;
			}

			// Debugging messages
			//std::cout << "VAO: " << m_vao << ", Indices size: " << m_indices.size() << std::endl;

			// Ensure VAO is valid
			if (m_vao == 0) {
				throw std::runtime_error("Invalid VAO");
			}

			// Ensure EBO is valid
			if (m_ebo == 0) {
				throw std::runtime_error("Invalid EBO");
			}

			// Ensure indices are not empty
			if (m_indices.empty()) {
				throw std::runtime_error("Indices are empty");
			}

			glBindVertexArray(m_vao);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
			glBindVertexArray(0);
		} catch (const std::exception& e) {
			std::cerr << "Exception caught in SphereBatch::Draw: " << e.what() << std::endl;
		}
	}
}