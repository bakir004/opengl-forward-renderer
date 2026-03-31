#include "Transform.h"

#include <glm/gtc/matrix_transform.hpp>

void Transform::Reset() {
    m_translation = glm::vec3(0.0f);
    m_rotationEulerDegrees = glm::vec3(0.0f);
    m_scale = glm::vec3(1.0f);
    m_dirty = true;
}

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

void Transform::SetRotationEulerDegrees(glm::vec3 eulerDegrees) {
    if (m_rotationEulerDegrees == eulerDegrees) return;
    m_rotationEulerDegrees = eulerDegrees;
    m_dirty = true;
}

void Transform::RotateEuler(glm::vec3 deltaEulerDegrees) {
    if (deltaEulerDegrees == glm::vec3(0.0f)) return;
    m_rotationEulerDegrees += deltaEulerDegrees;
    m_dirty = true;
}

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

const glm::mat4& Transform::GetModelMatrix() const {
    if (m_dirty) RebuildModelMatrix();
    return m_model;
}

void Transform::RebuildModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);

    // M = T * R * S, with Euler rotation order Y -> X -> Z.
    model = glm::translate(model, m_translation);
    model = glm::rotate(model, glm::radians(m_rotationEulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotationEulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotationEulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, m_scale);

    m_model = model;
    m_dirty = false;
}
