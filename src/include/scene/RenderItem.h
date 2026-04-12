#pragma once

#include "core/Transform.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

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
    bool visible       = true;
    bool castShadow    = true;
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
    const MeshBuffer*       mesh        = nullptr;
    const Mesh*             meshMulti   = nullptr;  ///< Multi-submesh path; takes priority over mesh
    uint32_t                subMeshIndex = 0;        ///< Which submesh to draw
    const ShaderProgram*    shader      = nullptr;   ///< Used when material is null
    const MaterialInstance* material    = nullptr;   ///< Takes priority over shader
    Transform               transform;

    /// Local-space mesh orientation fix, applied after the object transform.
    ///
    /// WHY a quaternion instead of three Euler angles:
    ///   A fixed mesh orientation correction (e.g. rotating a glTF asset whose
    ///   forward axis differs from the game's convention) is a single, constant
    ///   rotation. Storing it as a quaternion avoids having to reconstruct three
    ///   intermediate rotation matrices every draw call and eliminates axis-order
    ///   ambiguity — the intent is explicit in glm::angleAxis(angle, axis).
    ///
    /// Default: identity (no correction applied).
    /// To set: rotationOffset = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0,1,0));
    glm::quat rotationOffset   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    glm::vec3 translationOffset = {0.0f, 0.0f, 0.0f}; ///< Local-space mesh origin fix.
    PrimitiveTopology topology  = PrimitiveTopology::Triangles;
    DrawMode          drawMode  = DrawMode::Fill;
    RenderFlags       flags;

    /// Returns whichever shader will actually be used at draw time.
    [[nodiscard]] const ShaderProgram* ResolvedShader() const;
};
