#pragma once

#include "core/Transform.h"
#include <glm/vec3.hpp>
class MeshBuffer;
class ShaderProgram;
class MaterialInstance;
class Mesh;

enum class PrimitiveTopology {
    Triangles,
    Lines,
    Points,
    TriangleStrip,
    LineStrip
};

enum class DrawMode {
    Fill,
    Wireframe,
    Points
};

struct RenderFlags {
    bool visible      = true;
    bool castShadow   = true;
    bool receiveShadow = true;
};

/// One renderable object for a single frame.
///
/// Material priority:
///   1. If `material` is set, it is used (provides both shader and parameters).
///   2. Otherwise `shader` is used directly (legacy / primitive path).
///
/// Both fields are non-owning raw/weak references — the scene owns the resources.
struct RenderItem {
    const MeshBuffer*       mesh     = nullptr;
    const Mesh*   meshMulti    = nullptr;  ///< Multi-submesh path; takes priority over mesh
    uint32_t      subMeshIndex = 0;        ///< Which submesh to draw
    const ShaderProgram*    shader   = nullptr;   ///< Used when material is null
    const MaterialInstance* material = nullptr;   ///< Takes priority over shader
    Transform               transform;
    glm::vec3               rotationOffsetDeg  = {0.0f, 0.0f, 0.0f}; ///< Local-space mesh orientation fix, applied before transform rotation.
    glm::vec3               translationOffset  = {0.0f, 0.0f, 0.0f}; ///< Local-space mesh origin fix, applied before transform translation.
    PrimitiveTopology       topology = PrimitiveTopology::Triangles;
    DrawMode                drawMode = DrawMode::Fill;
    RenderFlags             flags;

    /// Returns whichever shader will actually be used at draw time.
    [[nodiscard]] const ShaderProgram* ResolvedShader() const;
};
