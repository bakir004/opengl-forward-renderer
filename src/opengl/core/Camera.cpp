#include "core/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static constexpr float kPitchMax = 89.0f; ///< Degrees — hard ceiling to prevent gimbal flip
static constexpr float kPitchMin = -89.0f; ///< Degrees
static constexpr float kFOVMin = 10.0f; ///< Degrees
static constexpr float kFOVMax = 120.0f; ///< Degrees
static constexpr float kOrbitMin = 0.5f; ///< World units
static constexpr float kOrbitMax = 1000.0f; ///< World units
static constexpr glm::vec3 kWorldUp = {0.0f, 1.0f, 0.0f};

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

Camera::Camera(CameraMode mode) : m_mode(mode) {
    m_viewDirty = true;
    m_projDirty = true;
}

Camera::Camera(glm::vec3 position,
               float yawDegrees,
               float pitchDegrees,
               float fovDegrees,
               float aspectRatio,
               float nearClip,
               float farClip,
               CameraMode mode)
    : m_position(position)
      , m_yaw(yawDegrees)
      , m_pitch(std::clamp(pitchDegrees, kPitchMin, kPitchMax))
      , m_fov(std::clamp(fovDegrees, kFOVMin, kFOVMax))
      , m_aspect(aspectRatio)
      , m_near(nearClip)
      , m_far(farClip)
      , m_mode(mode) {
    m_viewDirty = true;
    m_projDirty = true;
}

// ---------------------------------------------------------------------------
//  Position
// ---------------------------------------------------------------------------

void Camera::SetPosition(glm::vec3 position) {
    if (m_position == position) return;
    m_position = position;
    m_viewDirty = true;
}

void Camera::SetTarget(glm::vec3 target, glm::vec3 up) {
    glm::vec3 dir = glm::normalize(target - m_position);

    // Derive yaw and pitch from the direction vector.
    m_pitch = glm::degrees(std::asin(dir.y));
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));

    m_pitch = std::clamp(m_pitch, kPitchMin, kPitchMax);

    m_viewDirty = true;
}

[[nodiscard]] glm::vec3 Camera::GetPosition() const {
    if (m_mode == CameraMode::ThirdPerson) {
        RebuildBasis();
        return m_orbitTarget - m_forward * m_orbitRadius;
    }
    return m_position;
}

[[nodiscard]] glm::mat4 Camera::GetViewProjection() const {
    return GetProjection() * GetView();
}

// ---------------------------------------------------------------------------
//  Orientation
// ---------------------------------------------------------------------------

void Camera::SetOrientation(float yawDegrees, float pitchDegrees) {
    m_yaw = yawDegrees;
    m_pitch = std::clamp(pitchDegrees, kPitchMin, kPitchMax);
    m_viewDirty = true;
}

void Camera::Rotate(float dYaw, float dPitch) {
    m_yaw += dYaw;
    m_yaw = std::fmod(m_yaw, 360.);
    m_pitch = std::clamp(m_pitch + dPitch, kPitchMin, kPitchMax);
    m_viewDirty = true;
}

// ---------------------------------------------------------------------------
//  Movement
// ---------------------------------------------------------------------------

void Camera::Move(CameraDirection direction, float speed, float deltaTime) {
    // Ensure basis vectors are current before computing movement.
    RebuildBasis();

    const float dist = speed * deltaTime;

    // For FirstPerson, forward/backward movement ignores the vertical component
    // (no flying). For FreeFly, full 3-DoF movement is allowed.
    glm::vec3 horizontalForward = m_forward;
    if (m_mode == CameraMode::FirstPerson) {
        // Project forward onto the XZ plane and renormalize.
        horizontalForward = glm::normalize(glm::vec3(m_forward.x, 0.0f, m_forward.z));
    }

    // Up/Down always moves along world Y axis, regardless of camera orientation.
    constexpr glm::vec3 moveUp = kWorldUp;

    switch (direction) {
        case CameraDirection::Forward:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget += horizontalForward * dist;
            else m_position += horizontalForward * dist;
            break;
        case CameraDirection::Backward:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget -= horizontalForward * dist;
            else m_position -= horizontalForward * dist;
            break;
        case CameraDirection::Right:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget += m_right * dist;
            else m_position += m_right * dist;
            break;
        case CameraDirection::Left:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget -= m_right * dist;
            else m_position -= m_right * dist;
            break;
        case CameraDirection::Up:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget += moveUp * dist;
            else m_position += moveUp * dist;
            break;
        case CameraDirection::Down:
            if (m_mode == CameraMode::ThirdPerson) m_orbitTarget -= moveUp * dist;
            else m_position -= moveUp * dist;
            break;
    }

    m_viewDirty = true;
}

// ---------------------------------------------------------------------------
//  Projection parameters
// ---------------------------------------------------------------------------

void Camera::SetFOV(float fovDegrees) {
    const float clamped = std::clamp(fovDegrees, kFOVMin, kFOVMax);
    if (m_fov == clamped) return;
    m_fov = clamped;
    m_projDirty = true;
}

void Camera::SetAspectRatio(float aspect) {
    if (aspect <= 0.0f) {
        spdlog::warn("[Camera] SetAspectRatio called with non-positive value ({:.3f}) - ignored", aspect);
        return;
    }
    if (m_aspect == aspect) return;
    m_aspect = aspect;
    m_projDirty = true;
}

void Camera::SetClipPlanes(float nearClip, float farClip) {
    if (nearClip <= 0.0f || farClip <= 0.0f || nearClip >= farClip) {
        spdlog::warn("[Camera] SetClipPlanes invalid values (near={:.3f}, far={:.3f}) — ignored",
                     nearClip, farClip);
        return;
    }
    m_near = nearClip;
    m_far = farClip;
    m_projDirty = true;
}

void Camera::OnResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        spdlog::warn("[Camera] OnResize called with non-positive dimensions ({}x{}) - ignored",
                     width, height);
        return;
    }
    float newAspect = static_cast<float>(width) / static_cast<float>(height);
    if (newAspect == m_aspect) return;
    SetAspectRatio(newAspect);
    spdlog::debug("[Camera] Aspect updated to {:.4f} ({}x{})", m_aspect, width, height);
}

// ---------------------------------------------------------------------------
//  Mode
// ---------------------------------------------------------------------------

void Camera::SetMode(CameraMode mode) {
    if (m_mode == mode) return;

    // Leaving ThirdPerson: sync m_position to the actual eye position so the
    // next mode starts exactly where the camera was visually — no jump or rotate.
    if (m_mode == CameraMode::ThirdPerson) {
        RebuildBasis();
        m_position = m_orbitTarget - m_forward * m_orbitRadius;
    }

    m_mode = mode;
    m_viewDirty = true;
}

// ---------------------------------------------------------------------------
//  Third-person orbit
// ---------------------------------------------------------------------------

void Camera::SetOrbitTarget(glm::vec3 target) {
    m_orbitTarget = target;
    if (m_mode == CameraMode::ThirdPerson) m_viewDirty = true;
}

void Camera::SetOrbitRadius(float radius) {
    m_orbitRadius = std::clamp(radius, kOrbitMin, kOrbitMax);
    if (m_mode == CameraMode::ThirdPerson) m_viewDirty = true;
}

// ---------------------------------------------------------------------------
//  Derived basis vectors
// ---------------------------------------------------------------------------

glm::vec3 Camera::GetForward() const {
    RebuildBasis();
    return m_forward;
}

glm::vec3 Camera::GetRight() const {
    RebuildBasis();
    return m_right;
}

glm::vec3 Camera::GetUp() const {
    RebuildBasis();
    return m_up;
}

// ---------------------------------------------------------------------------
//  Matrix access
// ---------------------------------------------------------------------------

const glm::mat4 &Camera::GetView() const {
    if (m_viewDirty) RebuildView();
    return m_view;
}

const glm::mat4 &Camera::GetProjection() const {
    if (m_projDirty) RebuildProjection();
    return m_projection;
}

// ---------------------------------------------------------------------------
//  Composite output
// ---------------------------------------------------------------------------

CameraData Camera::BuildCameraData(float time, float deltaTime) const {
    CameraData data;
    data.view = GetView();
    data.projection = GetProjection();
    data.viewProj = data.projection * data.view; // column-major: P * V
    data.position = GetPosition();
    data._pad0 = 0.0f;
    data.time = time;
    data.deltaTime = deltaTime;
    return data;
}

// ---------------------------------------------------------------------------
//  Private helpers
// ---------------------------------------------------------------------------

void Camera::RebuildBasis() const {
    if (!m_viewDirty) return; // basis is always rebuilt with the view

    const float yawRad = glm::radians(m_yaw);
    const float pitchRad = glm::radians(m_pitch);

    m_forward = glm::normalize(glm::vec3(
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    ));

    m_right = glm::normalize(glm::cross(m_forward, kWorldUp));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

void Camera::RebuildView() const {
    RebuildBasis();

    if (m_mode == CameraMode::ThirdPerson) {
        // Eye position is derived from the orbit target + offset along -forward.
        const glm::vec3 eye = m_orbitTarget - m_forward * m_orbitRadius;
        m_view = glm::lookAt(eye, m_orbitTarget, kWorldUp);
    } else {
        // FreeFly and FirstPerson: eye is at m_position, looking along m_forward.
        m_view = glm::lookAt(m_position, m_position + m_forward, kWorldUp);
    }

    m_viewDirty = false;
}

void Camera::RebuildProjection() const {
    // glm::perspective expects vertical FOV in radians.
    m_projection = glm::perspective(
        glm::radians(m_fov),
        m_aspect,
        m_near,
        m_far
    );

    m_projDirty = false;
}
