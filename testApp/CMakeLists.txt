cmake_minimum_required(VERSION 3.26.0)
project(TestApp)

set(CMAKE_CXX_STANDARD 20)

# Test application executable
add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        RendererLib
)

# Copy shaders to test app output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMENT "Copying shaders to test app output directory"
)