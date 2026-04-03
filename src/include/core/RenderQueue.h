#pragma once
#include <vector>
#include "scene/RenderItem.h"

struct SubmissionContext;

/// Collects draw calls for a single frame, sorts them by shader to minimise
/// shader-switch overhead, then executes them all in one Flush() call.
///
/// Typical per-frame usage:
///   1. Application calls SubmitDraw() for each visible object → Add()
///   2. Renderer::EndFrame() calls Sort() → Flush() → Clear()
class RenderQueue {
public:
    /// Enqueues one item.  Items with visible=false, null mesh, or null shader are silently dropped.
    void Add(const RenderItem& item);

    /// Sorts the queue by shader pointer to minimise shader-program switches.
    /// Items without a shader (defensive; Add already filters them) sort to the end.
    void Sort();

    /// Iterates the sorted queue and issues all draw calls:
    ///   - Tracks the last bound shader; rebinds only on change.
    ///   - Sets glPolygonMode per DrawMode; resets to GL_FILL after the last item.
    ///   - Writes u_Model and u_NormalMatrix uniforms per object.
    void Flush(SubmissionContext& current);

    /// Clears all queued items (keeps reserved capacity for the next frame).
    void Clear();

    [[nodiscard]] bool IsEmpty() const;

private:
    std::vector<RenderItem> m_items;
};
