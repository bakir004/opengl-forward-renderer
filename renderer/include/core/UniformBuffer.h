#pragma once
#include "Buffer.h"
#include <glad/glad.h>

/// GPU-side Uniform Buffer Object (UBO).
///
/// Wraps a Buffer(UNIFORM) and exposes the two UBO-specific operations that
/// Buffer does not: uploading a typed CPU struct and binding to a numbered slot.
///
/// ── How to use ──────────────────────────────────────────────────────────────
///
///  STEP 1 — Define a std140-compatible struct on the CPU side.
///           std140 rules that matter most:
///             • vec3 is padded to 16 bytes — add a float pad after it.
///             • Every mat4 is 64 bytes, already aligned.
///             • Scalars (float, int) are 4 bytes with 4-byte alignment.
///
///       struct CameraBlock {
///           glm::mat4 view;           // 64 bytes
///           glm::mat4 projection;     // 64 bytes
///           glm::vec3 cameraPos;      // 12 bytes
///           float     _pad;           //  4 bytes — keeps the block 16-byte aligned
///       };                            // total: 144 bytes
///
///  STEP 2 — Declare the matching block in the GLSL shader.
///           The binding index must match the slot used in BindToSlot().
///
///       layout(std140, binding = 0) uniform Camera {
///           mat4 view;
///           mat4 projection;
///           vec3 cameraPos;
///       };
///
///  STEP 3 — Create the UniformBuffer once (sized to the struct).
///
///       UniformBuffer cameraUBO(sizeof(CameraBlock));
///
///  STEP 4 — Each frame: fill the struct, upload it, bind to the slot.
///
///       CameraBlock block { camera.GetView(), camera.GetProjection(),
///                           camera.GetPosition(), 0.f };
///       cameraUBO.Upload(&block, sizeof(block));
///       cameraUBO.BindToSlot(0);   // slot 0 → binding = 0 in the shader
///
///  STEP 5 — Draw. The shader reads the block automatically with no per-field
///           SetUniform() calls needed.
///
/// ────────────────────────────────────────────────────────────────────────────
class UniformBuffer {
public:
    /// Allocates a GPU-side uniform buffer of @p size bytes.
    /// @param size   Must exactly match the std140 layout size declared in the shader block.
    ///               Use sizeof(YourBlock) — do not guess.
    /// @param usage  GL_DYNAMIC_DRAW for data updated every frame (camera, lights).
    ///               GL_STATIC_DRAW for data set once (material constants, projection only).
    explicit UniformBuffer(GLsizeiptr size, GLenum usage = GL_DYNAMIC_DRAW);

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    UniformBuffer(UniformBuffer&&) noexcept = default;
    UniformBuffer& operator=(UniformBuffer&&) noexcept = default;

    /// Writes CPU data into the GPU buffer.
    /// Delegates to Buffer::UpdateData (glBufferSubData).
    /// @param data    Pointer to the source data — typically the address of a std140 struct.
    /// @param size    Bytes to write. Must be <= the size passed to the constructor.
    /// @param offset  Byte offset into the GPU buffer (default 0).
    void Upload(const void* data, GLsizeiptr size, GLintptr offset = 0);

    /// Binds this buffer to a numbered UBO binding point via glBindBufferBase.
    /// This is a global binding — it persists until another buffer is bound to the same slot.
    /// Call this once per frame before issuing draw calls that read from this block.
    /// @param bindingPoint  Slot index N matching the shader's `binding = N` declaration.
    ///                      Must be < GL_MAX_UNIFORM_BUFFER_BINDINGS (at least 36 on GL 4.x).
    void BindToSlot(GLuint bindingPoint) const;

private:
    Buffer m_buffer;
};
