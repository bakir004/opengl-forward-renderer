#include "core/Transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// Converts Euler angles (degrees, x=pitch/y=yaw/z=roll) to a quaternion
/// using Y→X→Z composition order, matching the previous matrix convention.
///
/// WHY explicit axis-by-axis construction instead of glm::quat(vec3)?
///   glm::quat(euler) uses an XYZ intrinsic order that does NOT match this
///   project's Y→X→Z convention. Building each axis quaternion separately
///   and multiplying guarantees the intended order regardless of GLM version.
static glm::quat EulerDegreesToQuat(glm::vec3 eulerDegrees) {
    // Equivalent rotation matrix: Ry * Rx * Rz (column vectors applied right-to-left)
    const glm::quat qY = glm::angleAxis(glm::radians(eulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qX = glm::angleAxis(glm::radians(eulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat qZ = glm::angleAxis(glm::radians(eulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    return qY * qX * qZ;
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

void Transform::Reset() {
    m_translation = glm::vec3(0.0f);
    m_rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // identity
    m_scale       = glm::vec3(1.0f);
    m_dirty       = true;
}

// ---------------------------------------------------------------------------
//  Translation
// ---------------------------------------------------------------------------

void Transform::SetTranslation(glm::vec3 translation) {
    if (m_translation == translation) return;
    m_translation = translation;
    m_dirty = true;
}

void Transform::Translate(glm::vec3 delta) {
    if (delta == glm::vec3(0.0f)) return;
    m_translation += delta;
    m_dirty = true;
}

// ---------------------------------------------------------------------------
//  Rotation — primary quaternion API
// ---------------------------------------------------------------------------

void Transform::SetRotation(glm::quat rotation) {
    m_rotation = rotation;
    m_dirty    = true;
}

void Transform::Rotate(glm::quat delta) {
    // Right-multiply so `delta` is applied in the object's local space —
    // e.g., spinning a planet further around its own (already-tilted) pole.
    m_rotation = m_rotation * delta;
    m_dirty    = true;
}

// ---------------------------------------------------------------------------
//  Rotation — Euler convenience wrappers
// ---------------------------------------------------------------------------

void Transform::SetRotationEulerDegrees(glm::vec3 eulerDegrees) {
    // Convert to quaternion so the stored representation is always a quat.
    // This avoids the need to carry Euler state and recompose the same matrix
    // from potentially accumulated floating-point drift.
    m_rotation = EulerDegreesToQuat(eulerDegrees);
    m_dirty    = true;
}

void Transform::RotateEuler(glm::vec3 deltaEulerDegrees) {
    if (deltaEulerDegrees == glm::vec3(0.0f)) return;
    // Convert the Euler delta to a quaternion and right-multiply (local space).
    // This is equivalent to composing a fresh Euler rotation on top of the
    // current one without converting back to angles — safe from gimbal lock.
    m_rotation = m_rotation * EulerDegreesToQuat(deltaEulerDegrees);
    m_dirty    = true;
}

// ---------------------------------------------------------------------------
//  Scale
// ---------------------------------------------------------------------------

void Transform::SetScale(glm::vec3 scale) {
    if (m_scale == scale) return;
    m_scale = scale;
    m_dirty = true;
}

void Transform::ScaleBy(glm::vec3 scaleMultiplier) {
    if (scaleMultiplier == glm::vec3(1.0f)) return;
    m_scale *= scaleMultiplier;
    m_dirty = true;
}

// ---------------------------------------------------------------------------
//  Matrix output
// ---------------------------------------------------------------------------

const glm::mat4& Transform::GetModelMatrix() const {
    if (m_dirty) RebuildModelMatrix();
    return m_model;
}

void Transform::RebuildModelMatrix() const {
    // M = T * R * S  (column-major OpenGL convention).
    //
    // WHY glm::mat4_cast(m_rotation) instead of three glm::rotate calls:
    //   Converting a quaternion directly to a rotation matrix is a single
    //   closed-form expression (9 multiplies, 15 adds). The old three-call
    //   approach chained three 4×4 matrix multiplications and required the
    //   caller to remember the Y→X→Z composition order — a silent footgun.
    //   mat4_cast is both faster and order-agnostic.
    m_model = glm::translate(glm::mat4(1.0f), m_translation)
            * glm::mat4_cast(m_rotation)
            * glm::scale(glm::mat4(1.0f), m_scale);

    m_dirty = false;
}
