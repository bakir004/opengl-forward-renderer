cmake_minimum_required(VERSION 3.26.0)
project(RendererLib)

set(CMAKE_CXX_STANDARD 20)

# Renderer static library
add_library(${PROJECT_NAME} STATIC
        src/app/Application.cpp
        src/core/Renderer.cpp
        src/core/Buffer.cpp
        src/core/VertexArray.cpp
        src/core/VertexLayout.cpp
        src/core/MeshBuffer.cpp
        src/core/Primitives.cpp
        src/core/ShaderProgram.cpp
        src/core/Transform.h
        src/core/Transform.cpp
        src/utils/Options.cpp
        src/core/Camera.h
        src/core/Camera.cpp
        src/core/UniformBuffer.h
        src/core/UniformBuffer.cpp
        src/core/KeyboardInput.h
        src/core/KeyboardInput.cpp
        src/core/MouseInput.h
        src/core/MouseInput.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC src)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
        glad
        glfw
        OpenGL::GL
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glm::glm
)