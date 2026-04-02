#pragma once

#include "Camera.h"
#include "KeyboardInput.h"
#include "MouseInput.h"

/// Abstract camera controller base.
class CameraController {
public:
    explicit CameraController(Camera& camera);
    virtual ~CameraController() = default;

    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

    [[nodiscard]] float GetMoveSpeed() const { return m_moveSpeed; }
    [[nodiscard]] float GetMouseSensitivity() const { return m_mouseSensitivity; }

    virtual void Update(float deltaTime, const KeyboardInput& keyboard, const MouseInput& mouse) = 0;

protected:
    Camera& m_camera;
    float   m_moveSpeed = 5.0f;
    float   m_mouseSensitivity = 0.15f;
};

/// Free-fly camera controller (six degrees of freedom).
class FreeFlyController : public CameraController {
public:
    explicit FreeFlyController(Camera& camera);
    void Update(float deltaTime, const KeyboardInput& keyboard, const MouseInput& mouse) override;
};

/// First-person camera controller (grounded movement, pitch clamp).
class FirstPersonController : public CameraController {
public:
    explicit FirstPersonController(Camera& camera);
    void Update(float deltaTime, const KeyboardInput& keyboard, const MouseInput& mouse) override;
};
