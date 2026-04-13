#pragma once

#include "core/Mesh.h"
#include <memory>
#include <string>
#include <vector>

/// Per-material information extracted from a model file.
/// The diffusePath is relative to the working directory (ready to pass to
/// AssetImporter::LoadTexture). Empty string means no diffuse texture.
struct ModelMaterialInfo {
    std::string diffusePath;
    std::string name;
};

/// Result of AssetImporter::LoadModel.
///
/// mesh      — GPU mesh with one SubMesh per Assimp aiMesh.
///             SubMesh::materialIndex indexes into materials below.
/// materials — one entry per unique material found in the source file.
struct ModelData {
    std::shared_ptr<Mesh>           mesh;
    std::vector<ModelMaterialInfo>  materials;

    [[nodiscard]] bool IsValid() const { return mesh && mesh->IsValid(); }
};
