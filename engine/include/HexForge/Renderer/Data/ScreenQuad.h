#pragma once

// Third-party
#include <glad/glad.h>

namespace Hex
{

	struct ScreenQuad
  	{
    	ScreenQuad()
		{
    		InitBuffers();
    	}

		~ScreenQuad()
		{
    		glDeleteVertexArrays(1, &vao);
    		glDeleteBuffers(1, &vbo);
		}

    	float quad_vertices[24] = {
    	  // Positions   // Texture Coords
    	  -1.0f,  1.0f,  0.0f, 1.0f, // Top-left
    	  -1.0f, -1.0f,  0.0f, 0.0f, // Bottom-left
    	   1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right

    	  -1.0f,  1.0f,  0.0f, 1.0f, // Top-left
    	   1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right
    	   1.0f,  1.0f,  1.0f, 1.0f  // Top-right
 		};

		unsigned int vao{0}, vbo{0};

		void InitBuffers()
		{
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);

			glBindVertexArray(vao);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

			// Position attribute
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

			// Texture coordinate attribute
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);

		};
  	};
}