#pragma once

#include <glm/glm.hpp>

/// Pure data transform for renderable objects.
///
/// Conventions:
/// - Rotation is stored as Euler angles in degrees.
/// - Euler components are interpreted as:
///     x = pitch (X axis), y = yaw (Y axis), z = roll (Z axis).
/// - Rotation composition order is fixed to Y -> X -> Z.
/// - Model matrix uses column-major OpenGL convention: M = T * R * S.
class Transform {
public:
    /// Constructs identity transform:
    /// translation=(0,0,0), rotation=(0,0,0), scale=(1,1,1).
    Transform() = default;

    /// Restores identity TRS values and invalidates cached model matrix.
    void Reset();

    /// Sets absolute world/object translation.
    void SetTranslation(glm::vec3 translation);
    [[nodiscard]] glm::vec3 GetTranslation() const { return m_translation; }

    /// Adds delta translation to current value.
    void Translate(glm::vec3 delta);

    /// Sets absolute Euler rotation in degrees (pitch, yaw, roll).
    void SetRotationEulerDegrees(glm::vec3 eulerDegrees);
    [[nodiscard]] glm::vec3 GetRotationEulerDegrees() const { return m_rotationEulerDegrees; }

    /// Adds Euler delta (degrees) to current rotation.
    void RotateEuler(glm::vec3 deltaEulerDegrees);

    /// Sets absolute non-uniform scale.
    void SetScale(glm::vec3 scale);
    [[nodiscard]] glm::vec3 GetScale() const { return m_scale; }

    /// Component-wise scale multiplication (non-uniform supported).
    void ScaleBy(glm::vec3 scaleMultiplier);

    /// Returns cached model matrix, rebuilding only when TRS changed.
    [[nodiscard]] const glm::mat4& GetModelMatrix() const;

private:
    /// Rebuilds model matrix using documented TRS and Euler conventions.
    void RebuildModelMatrix() const;

    glm::vec3 m_translation{0.0f, 0.0f, 0.0f};
    glm::vec3 m_rotationEulerDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};

    mutable glm::mat4 m_model{1.0f};
    mutable bool m_dirty = true;
};
