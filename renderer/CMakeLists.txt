cmake_minimum_required(VERSION 3.26.0)
project(RendererLib)

set(CMAKE_CXX_STANDARD 20)

# Renderer static library — OpenGL backend
add_library(${PROJECT_NAME} STATIC
        src/opengl/app/Application.cpp
        src/opengl/core/Renderer.cpp
        src/opengl/core/Buffer.cpp
        src/opengl/core/VertexArray.cpp
        src/opengl/core/VertexLayout.cpp
        src/opengl/core/MeshBuffer.cpp
        src/opengl/core/Primitives.cpp
        src/opengl/core/ShaderProgram.cpp
        src/opengl/core/CameraController.cpp
        src/opengl/core/Transform.cpp
        src/opengl/utils/Options.cpp
        src/opengl/core/Camera.cpp
        src/opengl/core/UniformBuffer.cpp
        src/opengl/core/KeyboardInput.cpp
        src/opengl/core/MouseInput.cpp
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