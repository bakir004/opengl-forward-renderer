#pragma once

struct SubmissionContext;

/// Handles OpenGL initialisation: GLAD loading, debug callback, and default pipeline state.
///
/// Owned by Renderer. Call Initialize() once after the GL context is current.
/// All GL calls live in the .cpp (OpenGL backend) — this header is backend-agnostic.
class InitContext {
public:
    /// Loads GL function pointers via GLAD, enables the debug callback (debug builds),
    /// logs vendor/GPU/version strings, and pushes the default SubmissionContext to the GPU.
    /// @param outApplied  Receives the default SubmissionContext that was pushed to the GPU.
    /// @return true on success; false if GLAD fails to load function pointers.
    bool Initialize(SubmissionContext& outApplied);

    /// Logs shutdown; no GL teardown needed (context is destroyed by the caller).
    void Shutdown();
};
