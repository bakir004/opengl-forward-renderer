#pragma once
#include <glm/glm.hpp>
#include <string>

/**
 * Interface for input providers.
 * Decouples the application logic from specific windowing/input libraries (like GLFW).
 */
class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    // --- Keyboard ---
    virtual bool IsKeyDown(int keyCode) const = 0;
    virtual bool IsKeyPressed(int keyCode) const = 0;

    // --- Mouse ---
    virtual bool IsMouseButtonDown(int button) const = 0;
    virtual bool IsMouseButtonPressed(int button) const = 0;
    virtual float GetMouseDeltaX() const = 0;
    virtual float GetMouseDeltaY() const = 0;
    virtual float GetMouseScrollDeltaY() const = 0;
    virtual bool IsMouseCaptured() const = 0;
    virtual void SetMouseCaptured(bool captured) = 0;

    // --- Interaction / Blocking ---
    virtual bool ShouldBlockKeyboard() const = 0;
    virtual bool ShouldBlockMouse() const = 0;
};
