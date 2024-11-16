#pragma once
//Hex
#include "Renderer/Primitives/PrimitiveBase.h"

namespace Hex
{
	class SphereBatch final : public Primitive
	{
	public:
		explicit SphereBatch();
		~SphereBatch() override;

		SphereBatch(const SphereBatch&) = default;
		SphereBatch(SphereBatch&&) = default;

		SphereBatch& operator = (const SphereBatch&) = default;
		SphereBatch& operator = (SphereBatch&&) = default;

		void AddSphere(const glm::vec3& position = glm::vec3(0.f), const float& radius = 1.f,
			const glm::vec3& color = glm::vec3(1.0f), unsigned int sector_count = 36, unsigned int stack_count = 18
		);
		void InitBuffers() override;
		void Draw() override;

	private:
		GLuint m_ebo{};
		std::vector<Vertex> m_vertices;
		std::vector<unsigned int> m_indices;
	};
}