#pragma once
#include <glm/glm.hpp>

struct Viewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const Viewport& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    bool operator!=(const Viewport& o) const { return !(*this == o); }
};

/// Bitmask controlling which GL buffers are cleared at the start of a frame.
enum class ClearFlags : unsigned int {
    None       = 0u,
    Color      = 1u << 0u,
    Depth      = 1u << 1u,
    Stencil    = 1u << 2u,
    ColorDepth = (1u << 0u) | (1u << 1u),
    All        = (1u << 0u) | (1u << 1u) | (1u << 2u)
};

inline ClearFlags operator|(ClearFlags a, ClearFlags b) {
    return static_cast<ClearFlags>(
        static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}
inline ClearFlags operator&(ClearFlags a, ClearFlags b) {
    return static_cast<ClearFlags>(
        static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}
inline bool operator!(ClearFlags f) {
    return static_cast<unsigned int>(f) == 0u;
}

/// Per-frame clear parameters applied once at the top of BeginFrame.
struct FrameClearInfo {
    Viewport   viewport;
    glm::vec4  clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
    ClearFlags clearFlags = ClearFlags::ColorDepth;

    /// Applies viewport, clear colour, depth mask, and glClear.
    /// Called once per frame — no caching needed here.
    void Apply() const;
};
