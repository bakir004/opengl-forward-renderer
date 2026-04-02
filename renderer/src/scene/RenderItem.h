#pragma once

#include "core/Transform.h"

class MeshBuffer;
class ShaderProgram;

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
    bool visible = true;
    bool castShadow = true;
    bool receiveShadow = true;
};

struct RenderItem {
    const MeshBuffer* mesh = nullptr;
    const ShaderProgram* shader = nullptr;
    Transform transform;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    DrawMode drawMode = DrawMode::Fill;
    RenderFlags flags;
};
