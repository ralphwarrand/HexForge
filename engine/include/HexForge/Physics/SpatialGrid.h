#pragma once

namespace Hex
{
    class SpatialGrid
    {
    public:
        SpatialGrid() = default;

        // Clears and rebuilds the grid with all particles for the current frame.
        void Build(entt::registry& registry, float cellSize);

        // Finds all entities within a given radius of a position.
        std::vector<entt::entity> QueryNeighbors(const glm::vec3& position, float radius);

    private:
        // Converts a 3D world position to a single integer hash key.
        int GetCellHash(const glm::ivec3& cellCoords) const;

        float m_cellSize = 1.0f;
        std::unordered_map<int, std::vector<entt::entity>> m_grid;
    };
}