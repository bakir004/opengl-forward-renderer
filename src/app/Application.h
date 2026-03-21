#pragma once
#include <memory>

struct GLFWwindow;
class Renderer;

class Application {
    public:
        Application();
        ~Application();
        bool Initialize();
        void Run();
        void Shutdown();
        Renderer* GetRenderer() const { return m_renderer.get(); }

    private:
        GLFWwindow* m_window = nullptr;
        std::unique_ptr<Renderer> m_renderer;
};