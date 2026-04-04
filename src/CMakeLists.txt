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
        opengl/scene/RenderItem.cpp
        opengl/core/Texture2D.cpp
        opengl/core/Material.cpp
        opengl/assets/AssetImporter.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)

find_package(OpenGL REQUIRED)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

target_link_libraries(imgui PUBLIC
    glad
    glfw
    OpenGL::GL
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        glad
        glfw
        OpenGL::GL
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glm::glm
)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui)

# stb_image — header-only, expose include dir
if(TARGET stb)
    target_link_libraries(${PROJECT_NAME} PUBLIC stb)
else()
    # Fallback: if FetchContent populated stb, add its include directory directly.
    target_include_directories(${PROJECT_NAME} PUBLIC ${stb_SOURCE_DIR})
endif()

# Assimp (enabled once Task 3 is wired)
# if(TARGET assimp::assimp)
#     target_link_libraries(${PROJECT_NAME} PUBLIC assimp::assimp)
# endif()