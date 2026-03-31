#pragma once
 
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
 
/// Movement direction enum used by Camera::Move().
/// Decoupled from keyboard scancodes — the input layer maps keys to these.
enum class CameraDirection {
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down,
};
 
/// Camera operating modes.
/// Affects how orientation is computed and which constraints are enforced.
enum class CameraMode {
    FreeFly,      ///< Six-DoF: unconstrained movement and look (no world-up lock)
    FirstPerson,  ///< Pitch clamped, world-up enforced; no roll
    ThirdPerson,  ///< Orbits around a target point at a configurable arm length
};
 
/// Per-frame data block that Camera produces each frame.
/// Matches the std140 layout of the CameraUBO expected by Sprint 3 shaders.
/// @note vec3 members are padded to vec4 alignment under std140 — keep the
///       padding field; it also serves as a placeholder for future data.
struct CameraData {
    glm::mat4 view       = glm::mat4(1.0f);  ///< World-to-view transform
    glm::mat4 projection = glm::mat4(1.0f);  ///< View-to-clip transform
    glm::mat4 viewProj   = glm::mat4(1.0f);  ///< Combined view * projection
    glm::vec3 position   = glm::vec3(0.0f);  ///< World-space camera position
    float     _pad0      = 0.0f;             ///< std140 padding (vec3 → vec4)
};
 
/// Pure-math perspective camera with lazy matrix recomputation.
///
/// Responsibilities:
///   - Owns position, orientation (yaw/pitch), FOV, aspect, near/far clip planes.
///   - Computes view and projection matrices on demand, caching until invalidated.
///   - Enforces pitch clamping (FreeFly / FirstPerson) and FOV bounds.
///   - Provides a frame-rate-independent Move() API — callers supply delta time.
///
/// Out of scope (handled by the input layer):
///   - GLFW key/mouse polling
///   - Raw mouse delta accumulation
///   - Window focus tracking
class Camera {
public:
    // -----------------------------------------------------------------------
    //  Construction
    // -----------------------------------------------------------------------
 
    /// Constructs a camera with sensible defaults:
    ///   - Position at (0, 1, 5), looking toward -Z
    ///   - FOV 60°, aspect 16:9, near 0.1, far 1000
    ///   - FreeFly mode
    explicit Camera(CameraMode mode = CameraMode::FreeFly);
 
    /// Fully specified constructor — useful for restoring camera state.
    Camera(glm::vec3 position,
           float yawDegrees,
           float pitchDegrees,
           float fovDegrees,
           float aspectRatio,
           float nearClip,
           float farClip,
           CameraMode mode = CameraMode::FreeFly);
 
    // -----------------------------------------------------------------------
    //  Position
    // -----------------------------------------------------------------------
 
    /// Teleports the camera to a world-space position (marks view dirty).
    void SetPosition(glm::vec3 position);
 
    /// Orients the camera to look at a world-space target from its current position.
    /// Updates yaw and pitch; does not change position.
    /// @param target World-space point to look at.
    /// @param up     World up hint used to resolve the right vector (default: +Y).
    void SetTarget(glm::vec3 target, glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));
 
    [[nodiscard]] glm::vec3 GetPosition() const { return m_position; }
 
    // -----------------------------------------------------------------------
    //  Orientation
    // -----------------------------------------------------------------------
 
    /// Sets absolute yaw and pitch (degrees).  Pitch is clamped to ±89°.
    void SetOrientation(float yawDegrees, float pitchDegrees);
 
    /// Applies incremental yaw and pitch deltas (degrees/frame) — call from mouse-look.
    /// Pitch is clamped after accumulation.
    /// @param dYaw   Positive = turn right.
    /// @param dPitch Positive = look up.
    void Rotate(float dYaw, float dPitch);
 
    [[nodiscard]] float GetYaw()   const { return m_yaw;   }
    [[nodiscard]] float GetPitch() const { return m_pitch; }
 
    // -----------------------------------------------------------------------
    //  Movement
    // -----------------------------------------------------------------------
 
    /// Moves the camera in a local-space direction, frame-rate-independent.
    ///
    /// The displacement vector is built from the camera's current basis vectors:
    ///   - Forward/Backward: along m_forward (FreeFly) or horizontal projection (FP)
    ///   - Left/Right:       along m_right
    ///   - Up/Down:          along world +Y (FirstPerson) or m_up (FreeFly)
    ///
    /// @param direction Which local axis to move along.
    /// @param speed     World-units per second.
    /// @param deltaTime Frame duration in seconds.
    void Move(CameraDirection direction, float speed, float deltaTime);
 
    // -----------------------------------------------------------------------
    //  Projection parameters
    // -----------------------------------------------------------------------
 
    /// Sets the vertical field of view.  Clamped to [10°, 120°].
    void SetFOV(float fovDegrees);
 
    /// Sets the aspect ratio (width / height).  Must be > 0.
    void SetAspectRatio(float aspect);
 
    /// Sets near and far clip distances.  Both must be > 0 and near < far.
    void SetClipPlanes(float nearClip, float farClip);
 
    /// Convenience overload called from the framebuffer-resize callback.
    /// Recomputes aspect ratio and marks projection dirty.
    /// @param width  New framebuffer width  (pixels, must be > 0)
    /// @param height New framebuffer height (pixels, must be > 0)
    void OnResize(int width, int height);
 
    [[nodiscard]] float GetFOV()    const { return m_fov;    }
    [[nodiscard]] float GetAspect() const { return m_aspect; }
    [[nodiscard]] float GetNear()   const { return m_near;   }
    [[nodiscard]] float GetFar()    const { return m_far;    }
 
    // -----------------------------------------------------------------------
    //  Mode
    // -----------------------------------------------------------------------
 
    void SetMode(CameraMode mode);
    [[nodiscard]] CameraMode GetMode() const { return m_mode; }
 
    // -----------------------------------------------------------------------
    //  Third-person orbit (only active in CameraMode::ThirdPerson)
    // -----------------------------------------------------------------------
 
    /// Sets the world-space point the camera orbits around.
    void SetOrbitTarget(glm::vec3 target);
 
    /// Sets the distance from orbit target to camera eye.  Clamped to [0.5, 1000].
    void SetOrbitRadius(float radius);
 
    [[nodiscard]] glm::vec3 GetOrbitTarget() const { return m_orbitTarget; }
    [[nodiscard]] float     GetOrbitRadius() const { return m_orbitRadius; }
 
    // -----------------------------------------------------------------------
    //  Derived basis vectors (always up-to-date after any getter)
    // -----------------------------------------------------------------------
 
    [[nodiscard]] glm::vec3 GetForward() const;
    [[nodiscard]] glm::vec3 GetRight()   const;
    [[nodiscard]] glm::vec3 GetUp()      const;
 
    // -----------------------------------------------------------------------
    //  Matrix access — lazy, cached, const-correct
    // -----------------------------------------------------------------------
 
    /// Returns the view matrix (world → camera space).
    /// Rebuilds if position or orientation changed since last call.
    [[nodiscard]] const glm::mat4& GetView()       const;
 
    /// Returns the projection matrix (camera → clip space).
    /// Rebuilds if FOV, aspect, or clip planes changed since last call.
    [[nodiscard]] const glm::mat4& GetProjection() const;
 
    // -----------------------------------------------------------------------
    //  Composite output
    // -----------------------------------------------------------------------
 
    /// Fills a CameraData struct for upload to the per-frame UBO.
    /// Triggers matrix rebuilds only if state has changed.
    [[nodiscard]] CameraData BuildCameraData() const;
 
private:
    // -----------------------------------------------------------------------
    //  Internal recomputation helpers
    // -----------------------------------------------------------------------
 
    /// Rebuilds m_forward / m_right / m_up from current yaw/pitch.
    void RebuildBasis() const;
 
    /// Rebuilds m_view from position and basis vectors.  Calls RebuildBasis().
    void RebuildView() const;
 
    /// Rebuilds m_proj from FOV / aspect / clip planes.
    void RebuildProjection() const;
 
    // -----------------------------------------------------------------------
    //  State
    // -----------------------------------------------------------------------
 
    glm::vec3  m_position    = glm::vec3(0.0f, 1.0f, 5.0f);
    float      m_yaw         = -90.0f;   ///< Degrees; -90 = looking toward -Z
    float      m_pitch       = 0.0f;     ///< Degrees; clamped to [-89, +89]
 
    float      m_fov         = 60.0f;    ///< Vertical FOV in degrees
    float      m_aspect      = 16.0f / 9.0f;
    float      m_near        = 0.1f;
    float      m_far         = 1000.0f;
 
    CameraMode m_mode        = CameraMode::FreeFly;
 
    // ThirdPerson orbit state
    glm::vec3  m_orbitTarget = glm::vec3(0.0f);
    float      m_orbitRadius = 5.0f;

    // Cached derived state (mutable so const getters can rebuild lazily)
    mutable glm::vec3  m_forward    = glm::vec3(0.0f, 0.0f, -1.0f);
    mutable glm::vec3  m_right      = glm::vec3(1.0f, 0.0f,  0.0f);
    mutable glm::vec3  m_up         = glm::vec3(0.0f, 1.0f,  0.0f);
    mutable glm::mat4  m_view       = glm::mat4(1.0f);
    mutable glm::mat4  m_projection = glm::mat4(1.0f);

    // Dirty flags
    mutable bool m_viewDirty = true;
    mutable bool m_projDirty = true;
};