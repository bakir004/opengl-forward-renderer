#pragma once

#include "core/AABB.h"
#include <glm/glm.hpp>

/// View frustum represented by six normalized clip planes.
/// Plane order: left, right, bottom, top, near, far.
struct Frustum {
    glm::vec4 planes[6]{};

    /// Extracts frustum planes from a column-major view-projection matrix
    /// using the Gribb–Hartmann method. Each plane is normalized.
    [[nodiscard]] static Frustum FromViewProjection(const glm::mat4& viewProjection)
    {
        Frustum frustum{};

        // GLM stores matrices in column-major order; extract rows for plane math.
        const glm::vec4 row0(viewProjection[0][0], viewProjection[1][0], viewProjection[2][0], viewProjection[3][0]);
        const glm::vec4 row1(viewProjection[0][1], viewProjection[1][1], viewProjection[2][1], viewProjection[3][1]);
        const glm::vec4 row2(viewProjection[0][2], viewProjection[1][2], viewProjection[2][2], viewProjection[3][2]);
        const glm::vec4 row3(viewProjection[0][3], viewProjection[1][3], viewProjection[2][3], viewProjection[3][3]);

        frustum.planes[0] = row3 + row0; // left
        frustum.planes[1] = row3 - row0; // right
        frustum.planes[2] = row3 + row1; // bottom
        frustum.planes[3] = row3 - row1; // top
        frustum.planes[4] = row3 + row2; // near
        frustum.planes[5] = row3 - row2; // far

        for (glm::vec4& plane : frustum.planes)
        {
            const float length = glm::length(glm::vec3(plane));
            if (length > 0.0001f)
                plane /= length;
        }

        return frustum;
    }

    /// Conservative AABB-vs-frustum test using the positive-vertex (p-vertex)
    /// method. Returns false only when the box is fully outside any plane.
    [[nodiscard]] bool IntersectsAABB(const AABB& box) const
    {
        if (!box.IsValid())
            return true;

        for (const glm::vec4& plane : planes)
        {
            glm::vec3 positiveVertex = box.min;
            if (plane.x >= 0.0f) positiveVertex.x = box.max.x;
            if (plane.y >= 0.0f) positiveVertex.y = box.max.y;
            if (plane.z >= 0.0f) positiveVertex.z = box.max.z;

            if (glm::dot(glm::vec3(plane), positiveVertex) + plane.w < 0.0f)
                return false;
        }

        return true;
    }
};
