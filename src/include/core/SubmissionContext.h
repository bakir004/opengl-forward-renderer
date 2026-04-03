#pragma once

/// Depth test comparison functions.
enum class DepthFunc {
    Less,           ///< Pass if incoming < stored (default, standard 3-D)
    LessEqual,      ///< Pass if incoming <= stored
    Greater,        ///< Pass if incoming > stored (reverse Z)
    GreaterEqual,   ///< Pass if incoming >= stored
    Always,         ///< Always pass (disables depth testing logic)
    Never,          ///< Never pass
    Equal,          ///< Pass if incoming == stored
    NotEqual        ///< Pass if incoming != stored
};

/// Blending modes for transparency and effects.
enum class BlendMode {
    Disabled,   ///< No blending
    Alpha,      ///< Standard transparency: src_alpha, 1-src_alpha
    Additive,   ///< Light / particle effects: src + dst
    Multiply    ///< Darken / multiply: src * dst
};

/// Face culling modes.
enum class CullMode {
    Disabled,       ///< Render both front and back faces
    Back,           ///< Cull back faces (default for solid objects)
    Front,          ///< Cull front faces (useful for inside-out geometry)
    FrontAndBack    ///< Cull everything (depth-only passes)
};

/// Pipeline state description for one render pass.
///
/// Create a value, set the fields you care about, then call Apply(current) to
/// push only the changed state to the GPU. The caching lives inside Apply so
/// callers never need to track what GL has set.
struct SubmissionContext {
    bool      depthTestEnabled = true;
    DepthFunc depthFunc        = DepthFunc::Less;
    bool      depthWrite       = true;
    BlendMode blendMode        = BlendMode::Disabled;
    CullMode  cullMode         = CullMode::Back;

    /// Compares this context against @p current and makes GL calls only for
    /// fields that differ.  Updates @p current to reflect the new GPU state.
    void Apply(SubmissionContext& current) const;
};
