#pragma once

#include <memory>

class MeshBuffer;

/// RAII owner of a fullscreen quad mesh.
///
/// Used by post-process passes to render effects that cover the entire viewport.
/// The quad is a simple 2-vertex strip (-1,-1) to (+1,+1) in clip space.
///
/// Usage:
///   FullscreenQuad quad;
///   quad.Draw();  // Draws 6 vertices (or 4 if using triangle strip)
class FullscreenQuad
{
public:
    FullscreenQuad();
    ~FullscreenQuad();

    FullscreenQuad(const FullscreenQuad &) = delete;
    FullscreenQuad &operator=(const FullscreenQuad &) = delete;

    FullscreenQuad(FullscreenQuad &&) = default;
    FullscreenQuad &operator=(FullscreenQuad &&) = default;

    /// Renders the fullscreen quad (4 vertices as triangle strip).
    void Draw() const;

private:
    std::shared_ptr<MeshBuffer> m_mesh;
};
