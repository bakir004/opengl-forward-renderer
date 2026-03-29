cmake_minimum_required(VERSION 3.26.0)
project(ForwardRenderer)

set(CMAKE_CXX_STANDARD 20)

set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
include(FetchContent)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLAD_PROFILE "core" CACHE STRING "")
set(GLAD_API "gl=4.6" CACHE STRING "")

FetchContent_MakeAvailable(glad glfw spdlog json glm)

add_executable(${PROJECT_NAME}
    src/app/main.cpp
    src/app/Application.cpp
    src/app/SampleScene.cpp
    src/core/Renderer.cpp
    src/core/Buffer.cpp
    src/core/VertexArray.cpp
    src/core/VertexLayout.cpp
    src/core/MeshBuffer.cpp
    src/core/Primitives.cpp
    src/core/ShaderProgram.cpp
    src/utils/Options.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    glfw
    OpenGL::GL
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    glm::glm
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)
