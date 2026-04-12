#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/// Pure data transform for renderable objects.
///
/// Rotation is stored as a unit quaternion (glm::quat).
///
/// WHY QUATERNIONS:
///   Euler angles are convenient for authoring (a 90° pitch is readable) but
///   cause two well-known problems at runtime:
///     1. Gimbal lock — when two rotation axes align, a degree of freedom is lost
///        and interpolation produces unnatural flips. Quaternions never lock.
///     2. Representation ambiguity — many Euler triples describe the same
///        orientation, and the "correct" one depends on the decomposition order.
///        A quaternion uniquely represents every orientation (up to sign).
///
///   Convenience wrappers (SetRotationEulerDegrees, RotateEuler) are kept so
///   that scene setup code can still write human-readable angles. They convert
///   to quaternions internally using a fixed Y→X→Z composition order that
///   matches the previous matrix convention.
///
/// Model matrix convention: M = T * R * S  (column-major, OpenGL).
class Transform {
public:
    /// Constructs identity transform: translation=0, rotation=identity, scale=1.
    Transform() = default;

    /// Restores identity TRS values and invalidates the cached model matrix.
    void Reset();

    // -----------------------------------------------------------------------
    //  Translation
    // -----------------------------------------------------------------------

    /// Sets absolute world/object translation.
    void SetTranslation(glm::vec3 translation);
    [[nodiscard]] glm::vec3 GetTranslation() const { return m_translation; }

    /// Adds delta translation to current value.
    void Translate(glm::vec3 delta);

    // -----------------------------------------------------------------------
    //  Rotation — primary quaternion API
    // -----------------------------------------------------------------------

    /// Sets the rotation directly from a unit quaternion.
    ///
    /// Prefer this over SetRotationEulerDegrees for runtime/dynamic rotations
    /// (animations, facing direction, orbital tilt + spin) because it is a
    /// single normalised store — no axis-order ambiguity, no gimbal risk.
    void SetRotation(glm::quat rotation);

    /// Returns the current rotation quaternion.
    [[nodiscard]] glm::quat GetRotation() const { return m_rotation; }

    /// Post-multiplies the current rotation by `delta` (local-space spin).
    ///
    /// Equivalent to: rotation = rotation * delta
    /// Use glm::angleAxis to build `delta` — e.g., one tick of a planet's
    /// axial spin: Rotate(glm::angleAxis(spinRad, glm::vec3(0,1,0))).
    void Rotate(glm::quat delta);

    // -----------------------------------------------------------------------
    //  Rotation — Euler convenience wrappers
    // -----------------------------------------------------------------------

    /// Sets rotation from Euler angles (degrees, pitch/yaw/roll = x/y/z).
    ///
    /// Internally converts to a quaternion using Y→X→Z composition order
    /// (matches the previous matrix-based convention). Use for static scene
    /// setup where degree angles are more readable than angle/axis pairs.
    /// For per-frame animations prefer SetRotation(glm::angleAxis(...)).
    void SetRotationEulerDegrees(glm::vec3 eulerDegrees);

    /// Applies an incremental Euler rotation (degrees) on top of the current
    /// orientation. Converts the delta to a quaternion and right-multiplies
    /// (local-space), so accumulated rotations never suffer gimbal lock even
    /// though the input is specified as Euler deltas.
    void RotateEuler(glm::vec3 deltaEulerDegrees);

    // -----------------------------------------------------------------------
    //  Scale
    // -----------------------------------------------------------------------

    /// Sets absolute non-uniform scale.
    void SetScale(glm::vec3 scale);
    [[nodiscard]] glm::vec3 GetScale() const { return m_scale; }

    /// Component-wise scale multiplication (non-uniform supported).
    void ScaleBy(glm::vec3 scaleMultiplier);

    // -----------------------------------------------------------------------
    //  Matrix output
    // -----------------------------------------------------------------------

    /// Returns the cached model matrix, rebuilding only when TRS has changed.
    [[nodiscard]] const glm::mat4& GetModelMatrix() const;

private:
    /// Rebuilds m_model from current TRS state.
    void RebuildModelMatrix() const;

    glm::vec3 m_translation{0.0f, 0.0f, 0.0f};
    /// Stored as a unit quaternion — avoids Euler decomposition order issues
    /// and makes glm::mat4_cast(m_rotation) the only matrix rebuild needed.
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};  // identity
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};

    mutable glm::mat4 m_model{1.0f};
    mutable bool      m_dirty = true;
};
