#include "scene/VisibilityCulling.h"

#include "core/Mesh.h"
#include "core/MeshBuffer.h"
#include "scene/RenderItem.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

AABB GetRenderItemLocalBounds(const RenderItem& item)
{
    if (item.meshMulti)
    {
        if (item.subMeshIndex < item.meshMulti->SubMeshCount())
            return item.meshMulti->GetSubMeshBounds(item.subMeshIndex);
        return item.meshMulti->GetLocalBounds();
    }

    if (item.mesh)
        return item.mesh->GetLocalBounds();

    return AABB::Empty();
}

glm::mat4 BuildRenderItemModelMatrix(const RenderItem& item)
{
    glm::mat4 model = item.transform.GetModelMatrix();

    const glm::vec3& translationOffset = item.translationOffset;
    if (translationOffset != glm::vec3(0.0f))
        model = model * glm::translate(glm::mat4(1.0f), translationOffset);

    static const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
    if (item.rotationOffset != kIdentity)
        model = model * glm::mat4_cast(item.rotationOffset);

    return model;
}

bool IsRenderItemVisibleInCameraFrustum(const RenderItem& item,
                                        const Frustum& cameraFrustum)
{
    const AABB localBounds = GetRenderItemLocalBounds(item);
    if (!localBounds.IsValid())
        return true;

    const AABB worldBounds = AABB::Transform(localBounds, BuildRenderItemModelMatrix(item));
    return cameraFrustum.IntersectsAABB(worldBounds);
}
