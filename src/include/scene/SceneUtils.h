#pragma once
#include "scene/RenderItem.h"
#include "core/Mesh.h"
#include "core/Material.h"
#include "core/ShaderProgram.h"
#include <memory>
#include <vector>

/// Splits a multi-submesh Mesh into one RenderItem per SubMesh.
///
/// Each SubMesh's materialIndex is used to look up the corresponding
/// MaterialInstance from the materials vector. If the slot is empty
/// or out of range, fallbackShader is used instead.
///
/// Returns an empty vector if mesh is null.
inline std::vector<RenderItem> SplitMeshIntoRenderItems(
    const std::shared_ptr<Mesh>&                          mesh,
    const std::vector<std::shared_ptr<MaterialInstance>>& materials      = {},
    const ShaderProgram*                                  fallbackShader = nullptr)
{
    std::vector<RenderItem> items;
    if (!mesh || !mesh->IsValid()) return items;

    items.reserve(mesh->SubMeshCount());

    for (uint32_t i = 0; i < mesh->SubMeshCount(); ++i) {
        const SubMesh& sm = mesh->GetSubMesh(i);

        RenderItem ri;
        ri.meshMulti    = mesh.get();
        ri.subMeshIndex = i;

        if (sm.materialIndex < materials.size() && materials[sm.materialIndex])
            ri.material = materials[sm.materialIndex].get();
        else
            ri.shader = fallbackShader;

        items.push_back(ri);
    }

    return items;
}