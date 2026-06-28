#pragma once

#include "core/AABB.h"
#include "core/Frustum.h"
#include <glm/mat4x4.hpp>

struct RenderItem;

/// Returns the mesh-local bounds for a render item, or an invalid AABB when
/// bounds are unavailable.
[[nodiscard]] AABB GetRenderItemLocalBounds(const RenderItem& item);

/// Builds the same model matrix used for drawing and shadow depth rendering.
[[nodiscard]] glm::mat4 BuildRenderItemModelMatrix(const RenderItem& item);

/// Conservative camera-frustum visibility test for forward-pass submission.
/// Invalid bounds are treated as visible so objects are never culled by mistake.
[[nodiscard]] bool IsRenderItemVisibleInCameraFrustum(const RenderItem& item,
                                                      const Frustum& cameraFrustum);
