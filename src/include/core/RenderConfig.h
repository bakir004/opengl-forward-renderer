#pragma once

/// Runtime renderer options toggled from the debug UI.
struct RenderConfig {
    /// When true, render items fully outside the camera frustum are skipped
    /// in the forward opaque pass. Shadow-caster submission is unaffected.
    bool frustumCullingEnabled = true;
};
