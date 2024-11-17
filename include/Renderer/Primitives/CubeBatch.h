#pragma once
// Hex
#include "Renderer/Primitives/PrimitiveBase.h"

namespace Hex
{
	class CubeBatch final : public Primitive
	{
	public:
		explicit CubeBatch();
		~CubeBatch() override;

		CubeBatch(const CubeBatch&) = default;
		CubeBatch(CubeBatch&&) = default;

		CubeBatch& operator=(const CubeBatch&) = default;
		CubeBatch& operator=(CubeBatch&&) = default;

		void AddCube(const glm::vec3& position = glm::vec3(0.f), const float& size = 1.f,
					 const glm::vec3& color = glm::vec3(1.0f));
		void InitBuffers() override;
		void Draw() override;

	private:
		GLuint m_ebo{};
		std::vector<Vertex> m_vertices;
		std::vector<unsigned int> m_indices;

		bool m_cube_buffers_initialized = false;
	};
}