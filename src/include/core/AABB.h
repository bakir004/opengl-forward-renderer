#pragma once

#include <glm/glm.hpp>
#include <limits>

/// Axis-aligned bounding box in local or world space.
struct AABB {
    glm::vec3 min{ std::numeric_limits<float>::max() };
    glm::vec3 max{ -std::numeric_limits<float>::max() };

    /// Returns true when the box has been expanded by at least one point.
    [[nodiscard]] bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    /// Expands the box to include @p point.
    void Expand(const glm::vec3& point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    /// Returns an empty (invalid) box.
    [[nodiscard]] static AABB Empty()
    {
        return {};
    }

    /// Transforms a local-space AABB into a conservative world-space AABB
    /// using the absolute-matrix (Arvo) method so the result stays axis-aligned.
    [[nodiscard]] static AABB Transform(const AABB& local, const glm::mat4& matrix)
    {
        if (!local.IsValid())
            return Empty();

        const glm::vec3 center  = (local.min + local.max) * 0.5f;
        const glm::vec3 extents = (local.max - local.min) * 0.5f;

        const glm::mat3 absMatrix(
            glm::abs(glm::vec3(matrix[0])),
            glm::abs(glm::vec3(matrix[1])),
            glm::abs(glm::vec3(matrix[2])));

        const glm::vec3 worldExtents = absMatrix * extents;
        const glm::vec3 worldCenter  = glm::vec3(matrix * glm::vec4(center, 1.0f));

        AABB result;
        result.min = worldCenter - worldExtents;
        result.max = worldCenter + worldExtents;
        return result;
    }
};
