#pragma once
#include <cstdint>
#include <vector>
#include "scene/RenderItem.h"

struct SubmissionContext;

struct RenderQueueFrameStats {
    uint32_t processedItemCount   = 0;
    uint32_t drawCallCount        = 0;
    uint64_t approxTriangleCount  = 0;
};

/// Collects draw calls for a single frame, sorts them by shader to minimise
/// shader-switch overhead, then executes them all in one Flush() call.
///
/// Typical per-frame usage:
///   1. Application calls SubmitDraw() for each visible object → Add()
///   2. Renderer::EndFrame() calls Sort() → Flush() → Clear()
class RenderQueue {
public:
    /// Enqueues one item and returns true if it made it into the queue.
    /// Items with visible=false, no geometry, or no usable shader path are silently dropped.
    [[nodiscard]] bool Add(const RenderItem& item);

    /// Sorts the queue by shader pointer to minimise shader-program switches.
    /// Items without a shader (defensive; Add already filters them) sort to the end.
    void Sort();

    /// Iterates the sorted queue and issues all draw calls:
    ///   - Tracks the last bound shader; rebinds only on change.
    ///   - Sets glPolygonMode per DrawMode; resets to GL_FILL after the last item.
    ///   - Writes u_Model and u_NormalMatrix uniforms per object.
    [[nodiscard]] RenderQueueFrameStats Flush(SubmissionContext& current);

    /// Clears all queued items (keeps reserved capacity for the next frame).
    void Clear();

    [[nodiscard]] bool IsEmpty() const;

    /// Sets the fallback shader used when a RenderItem has neither a material
    /// nor a shader assigned.  The object is still drawn (visibly wrong) and a
    /// warning is logged so the missing assignment is easy to spot.
    void SetErrorShader(const ShaderProgram* shader);

private:
    std::vector<RenderItem>  m_items;
    const ShaderProgram*     m_errorShader = nullptr;
};
