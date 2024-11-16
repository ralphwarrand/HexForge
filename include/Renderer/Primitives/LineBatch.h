#pragma once
#include "Renderer\Primitives\PrimitiveBase.h"

namespace Hex
{
	class LineBatch final : public Primitive
	{
	public:
		LineBatch();

		LineBatch(const LineBatch&) = default;
		LineBatch(LineBatch&&) = default;

		LineBatch& operator = (const LineBatch&) = default;
		LineBatch& operator = (LineBatch&&) = default;

		void AddLine(const glm::vec3& start, const glm::vec3& end,  const glm::vec3& color = glm::vec3(1.0f));
		void InitBuffers() override;
		void Draw() override;

	private:
		std::vector<Vertex> m_vertices;
	};
}