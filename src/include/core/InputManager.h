#pragma once
#include "core/IInputProvider.h"
#include <memory>

class KeyboardInput;
class MouseInput;
struct GLFWwindow;

/**
 * Centralized Input Manager that implements IInputProvider.
 * It coordinates KeyboardInput, MouseInput and UI-based input blocking.
 */
class InputManager : public IInputProvider {
public:
    InputManager(GLFWwindow* window);
    ~InputManager() override;

    void Update();

    // IInputProvider implementation
    bool IsKeyDown(int keyCode) const override;
    bool IsKeyPressed(int keyCode) const override;

    bool IsMouseButtonDown(int button) const override;
    bool IsMouseButtonPressed(int button) const override;
    float GetMouseDeltaX() const override;
    float GetMouseDeltaY() const override;
    float GetMouseScrollDeltaY() const override;
    bool IsMouseCaptured() const override;
    void SetMouseCaptured(bool captured) override;

    bool ShouldBlockKeyboard() const override;
    bool ShouldBlockMouse() const override;

    // Direct access if needed for low-level or legacy code
    KeyboardInput& GetKeyboard() { return *m_keyboard; }
    MouseInput& GetMouse() { return *m_mouse; }

private:
    std::unique_ptr<KeyboardInput> m_keyboard;
    std::unique_ptr<MouseInput> m_mouse;
};
