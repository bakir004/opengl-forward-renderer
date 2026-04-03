cmake_minimum_required(VERSION 3.26.0)
project(RendererLib)

set(CMAKE_CXX_STANDARD 20)

# Renderer static library — OpenGL backend
add_library(${PROJECT_NAME} STATIC
        opengl/app/Application.cpp
        opengl/core/Renderer.cpp
        opengl/core/FrameClearInfo.cpp
        opengl/core/SubmissionContext.cpp
        opengl/core/InitContext.cpp
        opengl/core/RenderQueue.cpp
        opengl/core/Buffer.cpp
        opengl/core/VertexArray.cpp
        opengl/core/VertexLayout.cpp
        opengl/core/MeshBuffer.cpp
        opengl/core/Primitives.cpp
        opengl/core/ShaderProgram.cpp
        opengl/core/CameraController.cpp
        opengl/core/Transform.cpp
        opengl/utils/Options.cpp
        opengl/core/Camera.cpp
        opengl/core/UniformBuffer.cpp
        opengl/core/KeyboardInput.cpp
        opengl/core/MouseInput.cpp
        opengl/scene/Scene.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
        glad
        glfw
        OpenGL::GL
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glm::glm
)