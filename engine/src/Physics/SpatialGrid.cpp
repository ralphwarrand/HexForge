#include "HexForge/pch.h"
#include "HexForge/Physics/SpatialGrid.h"
#include "HexForge/Gameplay/EntityComponents.h"

namespace Hex
{
    void SpatialGrid::Build(entt::registry& registry, float cellSize)
    {
        m_cellSize = cellSize;
        m_grid.clear();

        auto view = registry.view<ParticleComponent>();
        for (auto entity : view)
        {
            auto& particle = view.get<ParticleComponent>(entity);
            
            // Calculate which grid cell the particle belongs to
            glm::ivec3 cellCoords = glm::ivec3(glm::floor(particle.predictedPosition / m_cellSize));
            
            // Add the entity to the grid cell
            m_grid[GetCellHash(cellCoords)].push_back(entity);
        }
    }

    std::vector<entt::entity> SpatialGrid::QueryNeighbors(const glm::vec3& position, float radius)
    {
        std::vector<entt::entity> neighbors;
        
        // Determine the range of cells to check based on the radius
        glm::ivec3 centerCell = glm::ivec3(glm::floor(position / m_cellSize));
        int searchRadius = static_cast<int>(glm::ceil(radius / m_cellSize));

        // Iterate through all surrounding cells (a 3D cube of cells)
        for (int x = centerCell.x - searchRadius; x <= centerCell.x + searchRadius; ++x)
        {
            for (int y = centerCell.y - searchRadius; y <= centerCell.y + searchRadius; ++y)
            {
                for (int z = centerCell.z - searchRadius; z <= centerCell.z + searchRadius; ++z)
                {
                    glm::ivec3 cellCoords(x, y, z);
                    int hash = GetCellHash(cellCoords);
                    
                    // If a cell with this hash exists in our grid...
                    if (m_grid.count(hash))
                    {
                        // ...add all entities from that cell to our potential neighbors list.
                        neighbors.insert(neighbors.end(), m_grid.at(hash).begin(), m_grid.at(hash).end());
                    }
                }
            }
        }
        
        // Note: This returns potential neighbors. A final distance check is still needed
        // in the fluid solver to be fully accurate, but this is a massive optimization.
        return neighbors;
    }

    int SpatialGrid::GetCellHash(const glm::ivec3& cellCoords) const
    {
        // A common spatial hash function using large prime numbers
        return (cellCoords.x * 73856093) ^ (cellCoords.y * 19349663) ^ (cellCoords.z * 83492791);
    }
}