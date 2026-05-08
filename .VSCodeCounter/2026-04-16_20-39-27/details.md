# Details

Date : 2026-04-16 20:39:27

Directory c:\\Users\\Alibegovici\\Desktop\\ETF\\Treca godina\\Softverski inzinjering\\opengl-forward-renderer

Total : 113 files,  14701 codes, 1897 comments, 2271 blanks, all 18869 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [.githooks/commit-msg.sh](/.githooks/commit-msg.sh) | Shell Script | 47 | 3 | 12 | 62 |
| [.githooks/pre-commit.sh](/.githooks/pre-commit.sh) | Shell Script | 30 | 2 | 8 | 40 |
| [.githooks/pre-merge-commit.sh](/.githooks/pre-merge-commit.sh) | Shell Script | 23 | 1 | 7 | 31 |
| [.githooks/pre-rebase.sh](/.githooks/pre-rebase.sh) | Shell Script | 27 | 2 | 8 | 37 |
| [.github/workflows/cmake-multi-platform.yml](/.github/workflows/cmake-multi-platform.yml) | YAML | 44 | 0 | 9 | 53 |
| [CLAUDE.md](/CLAUDE.md) | Markdown | 274 | 0 | 80 | 354 |
| [CMakeLists.txt](/CMakeLists.txt) | CMake | 80 | 0 | 12 | 92 |
| [GITFLOW.md](/GITFLOW.md) | Markdown | 69 | 0 | 27 | 96 |
| [README.md](/README.md) | Markdown | 42 | 0 | 16 | 58 |
| [assets/shaders/basic.frag](/assets/shaders/basic.frag) | GLSL | 6 | 2 | 2 | 10 |
| [assets/shaders/basic.vert](/assets/shaders/basic.vert) | GLSL | 16 | 4 | 4 | 24 |
| [assets/shaders/error.frag](/assets/shaders/error.frag) | GLSL | 10 | 2 | 3 | 15 |
| [assets/shaders/error.vert](/assets/shaders/error.vert) | GLSL | 15 | 2 | 5 | 22 |
| [assets/shaders/light\_block.glsl](/assets/shaders/light_block.glsl) | GLSL | 47 | 12 | 7 | 66 |
| [assets/shaders/mesh.frag](/assets/shaders/mesh.frag) | GLSL | 213 | 27 | 50 | 290 |
| [assets/shaders/mesh.vert](/assets/shaders/mesh.vert) | GLSL | 24 | 1 | 7 | 32 |
| [assets/shaders/shadow\_depth.frag](/assets/shaders/shadow_depth.frag) | GLSL | 3 | 0 | 1 | 4 |
| [assets/shaders/shadow\_depth.vert](/assets/shaders/shadow_depth.vert) | GLSL | 7 | 0 | 3 | 10 |
| [build.bat](/build.bat) | Batch | 102 | 1 | 22 | 125 |
| [build.sh](/build.sh) | Shell Script | 138 | 9 | 34 | 181 |
| [config/settings.json](/config/settings.json) | JSON with Comments | 21 | 0 | 1 | 22 |
| [docs/ASSETS.md](/docs/ASSETS.md) | Markdown | 136 | 0 | 40 | 176 |
| [docs/core-classes.md](/docs/core-classes.md) | Markdown | 159 | 0 | 71 | 230 |
| [external/glad/include/KHR/khrplatform.h](/external/glad/include/KHR/khrplatform.h) | C++ | 117 | 165 | 30 | 312 |
| [external/glad/include/glad/glad.h](/external/glad/include/glad/glad.h) | C++ | 3,658 | 20 | 17 | 3,695 |
| [external/glad/src/glad.c](/external/glad/src/glad.c) | C | 1,762 | 25 | 47 | 1,834 |
| [external/spdlog-src/example/CMakeLists.txt](/external/spdlog-src/example/CMakeLists.txt) | CMake | 19 | 0 | 5 | 24 |
| [external/spdlog-src/example/example.cpp](/external/spdlog-src/example/example.cpp) | C++ | 270 | 82 | 52 | 404 |
| [run.bat](/run.bat) | Batch | 19 | 0 | 7 | 26 |
| [run.sh](/run.sh) | Shell Script | 18 | 1 | 6 | 25 |
| [sprints/sprint1.md](/sprints/sprint1.md) | Markdown | 65 | 0 | 19 | 84 |
| [sprints/sprint2.md](/sprints/sprint2.md) | Markdown | 317 | 0 | 118 | 435 |
| [sprints/sprint3.md](/sprints/sprint3.md) | Markdown | 315 | 0 | 107 | 422 |
| [src/CMakeLists.txt](/src/CMakeLists.txt) | CMake | 76 | 0 | 12 | 88 |
| [src/app/SampleScene.cpp](/src/app/SampleScene.cpp) | C++ | 51 | 7 | 17 | 75 |
| [src/include/app/Application.h](/src/include/app/Application.h) | C++ | 30 | 18 | 12 | 60 |
| [src/include/assets/AssetImporter.h](/src/include/assets/AssetImporter.h) | C++ | 71 | 67 | 27 | 165 |
| [src/include/assets/ModelData.h](/src/include/assets/ModelData.h) | C++ | 14 | 8 | 5 | 27 |
| [src/include/core/Buffer.h](/src/include/core/Buffer.h) | C++ | 19 | 21 | 11 | 51 |
| [src/include/core/Camera.h](/src/include/core/Camera.h) | C++ | 87 | 102 | 48 | 237 |
| [src/include/core/CameraController.h](/src/include/core/CameraController.h) | C++ | 50 | 7 | 12 | 69 |
| [src/include/core/FrameClearInfo.h](/src/include/core/FrameClearInfo.h) | C++ | 37 | 4 | 7 | 48 |
| [src/include/core/InitContext.h](/src/include/core/InitContext.h) | C++ | 7 | 9 | 4 | 20 |
| [src/include/core/KeyboardInput.h](/src/include/core/KeyboardInput.h) | C++ | 16 | 30 | 10 | 56 |
| [src/include/core/Material.h](/src/include/core/Material.h) | C++ | 51 | 28 | 25 | 104 |
| [src/include/core/Mesh.h](/src/include/core/Mesh.h) | C++ | 25 | 9 | 9 | 43 |
| [src/include/core/MeshBuffer.h](/src/include/core/MeshBuffer.h) | C++ | 36 | 25 | 11 | 72 |
| [src/include/core/MeshData.h](/src/include/core/MeshData.h) | C++ | 29 | 4 | 6 | 39 |
| [src/include/core/MouseInput.h](/src/include/core/MouseInput.h) | C++ | 24 | 35 | 12 | 71 |
| [src/include/core/Primitives.h](/src/include/core/Primitives.h) | C++ | 39 | 41 | 23 | 103 |
| [src/include/core/RenderQueue.h](/src/include/core/RenderQueue.h) | C++ | 37 | 21 | 11 | 69 |
| [src/include/core/Renderer.h](/src/include/core/Renderer.h) | C++ | 78 | 23 | 14 | 115 |
| [src/include/core/ShaderProgram.h](/src/include/core/ShaderProgram.h) | C++ | 35 | 12 | 11 | 58 |
| [src/include/core/SubMesh.h](/src/include/core/SubMesh.h) | C++ | 10 | 21 | 5 | 36 |
| [src/include/core/SubmissionContext.h](/src/include/core/SubmissionContext.h) | C++ | 31 | 10 | 6 | 47 |
| [src/include/core/Texture2D.h](/src/include/core/Texture2D.h) | C++ | 41 | 22 | 15 | 78 |
| [src/include/core/Transform.h](/src/include/core/Transform.h) | C++ | 27 | 65 | 22 | 114 |
| [src/include/core/UniformBuffer.h](/src/include/core/UniformBuffer.h) | C++ | 15 | 59 | 6 | 80 |
| [src/include/core/VertexArray.h](/src/include/core/VertexArray.h) | C++ | 15 | 8 | 8 | 31 |
| [src/include/core/VertexLayout.h](/src/include/core/VertexLayout.h) | C++ | 19 | 21 | 6 | 46 |
| [src/include/core/shadows/CascadedShadowMap.h](/src/include/core/shadows/CascadedShadowMap.h) | C++ | 29 | 19 | 13 | 61 |
| [src/include/scene/FrameSubmission.h](/src/include/scene/FrameSubmission.h) | C++ | 16 | 1 | 3 | 20 |
| [src/include/scene/GpuLightData.h](/src/include/scene/GpuLightData.h) | C++ | 47 | 56 | 6 | 109 |
| [src/include/scene/Light.h](/src/include/scene/Light.h) | C++ | 58 | 20 | 9 | 87 |
| [src/include/scene/LightBlock.h](/src/include/scene/LightBlock.h) | C++ | 29 | 42 | 11 | 82 |
| [src/include/scene/LightBuilder.h](/src/include/scene/LightBuilder.h) | C++ | 156 | 21 | 8 | 185 |
| [src/include/scene/LightEnvironment.h](/src/include/scene/LightEnvironment.h) | C++ | 33 | 15 | 11 | 59 |
| [src/include/scene/LightUtils.h](/src/include/scene/LightUtils.h) | C++ | 95 | 18 | 16 | 129 |
| [src/include/scene/RenderItem.h](/src/include/scene/RenderItem.h) | C++ | 39 | 19 | 10 | 68 |
| [src/include/scene/Scene.h](/src/include/scene/Scene.h) | C++ | 47 | 22 | 21 | 90 |
| [src/include/scene/SceneUtils.h](/src/include/scene/SceneUtils.h) | C++ | 28 | 7 | 7 | 42 |
| [src/include/utils/Options.h](/src/include/utils/Options.h) | C++ | 21 | 7 | 7 | 35 |
| [src/opengl/app/Application.cpp](/src/opengl/app/Application.cpp) | C++ | 522 | 26 | 92 | 640 |
| [src/opengl/assets/AssetImporter.cpp](/src/opengl/assets/AssetImporter.cpp) | C++ | 274 | 76 | 64 | 414 |
| [src/opengl/assets/MeshImporter.cpp](/src/opengl/assets/MeshImporter.cpp) | C++ | 88 | 25 | 27 | 140 |
| [src/opengl/assets/ModelImporter.cpp](/src/opengl/assets/ModelImporter.cpp) | C++ | 136 | 35 | 34 | 205 |
| [src/opengl/core/Buffer.cpp](/src/opengl/core/Buffer.cpp) | C++ | 54 | 4 | 10 | 68 |
| [src/opengl/core/Camera.cpp](/src/opengl/core/Camera.cpp) | C++ | 220 | 47 | 57 | 324 |
| [src/opengl/core/CameraController.cpp](/src/opengl/core/CameraController.cpp) | C++ | 158 | 3 | 24 | 185 |
| [src/opengl/core/FrameClearInfo.cpp](/src/opengl/core/FrameClearInfo.cpp) | C++ | 18 | 1 | 5 | 24 |
| [src/opengl/core/InitContext.cpp](/src/opengl/core/InitContext.cpp) | C++ | 48 | 1 | 9 | 58 |
| [src/opengl/core/KeyboardInput.cpp](/src/opengl/core/KeyboardInput.cpp) | C++ | 73 | 3 | 9 | 85 |
| [src/opengl/core/Material.cpp](/src/opengl/core/Material.cpp) | C++ | 86 | 9 | 24 | 119 |
| [src/opengl/core/Mesh.cpp](/src/opengl/core/Mesh.cpp) | C++ | 123 | 6 | 19 | 148 |
| [src/opengl/core/MeshBuffer.cpp](/src/opengl/core/MeshBuffer.cpp) | C++ | 75 | 2 | 14 | 91 |
| [src/opengl/core/MouseInput.cpp](/src/opengl/core/MouseInput.cpp) | C++ | 54 | 4 | 11 | 69 |
| [src/opengl/core/Primitives.cpp](/src/opengl/core/Primitives.cpp) | C++ | 228 | 31 | 29 | 288 |
| [src/opengl/core/RenderQueue.cpp](/src/opengl/core/RenderQueue.cpp) | C++ | 208 | 14 | 27 | 249 |
| [src/opengl/core/Renderer.cpp](/src/opengl/core/Renderer.cpp) | C++ | 382 | 48 | 69 | 499 |
| [src/opengl/core/ShaderProgram.cpp](/src/opengl/core/ShaderProgram.cpp) | C++ | 203 | 7 | 35 | 245 |
| [src/opengl/core/SubmissionContext.cpp](/src/opengl/core/SubmissionContext.cpp) | C++ | 92 | 5 | 7 | 104 |
| [src/opengl/core/Texture2D.cpp](/src/opengl/core/Texture2D.cpp) | C++ | 120 | 6 | 31 | 157 |
| [src/opengl/core/Transform.cpp](/src/opengl/core/Transform.cpp) | C++ | 62 | 45 | 22 | 129 |
| [src/opengl/core/UniformBuffer.cpp](/src/opengl/core/UniformBuffer.cpp) | C++ | 15 | 0 | 4 | 19 |
| [src/opengl/core/VertexArray.cpp](/src/opengl/core/VertexArray.cpp) | C++ | 32 | 0 | 7 | 39 |
| [src/opengl/core/VertexLayout.cpp](/src/opengl/core/VertexLayout.cpp) | C++ | 34 | 15 | 7 | 56 |
| [src/opengl/core/shadows/CascadedShadowMap.cpp](/src/opengl/core/shadows/CascadedShadowMap.cpp) | C++ | 132 | 12 | 17 | 161 |
| [src/opengl/scene/LightBlock.cpp](/src/opengl/scene/LightBlock.cpp) | C++ | 60 | 3 | 12 | 75 |
| [src/opengl/scene/LightEnvironment.cpp](/src/opengl/scene/LightEnvironment.cpp) | C++ | 66 | 0 | 16 | 82 |
| [src/opengl/scene/LightUtils.cpp](/src/opengl/scene/LightUtils.cpp) | C++ | 51 | 0 | 8 | 59 |
| [src/opengl/scene/RenderItem.cpp](/src/opengl/scene/RenderItem.cpp) | C++ | 8 | 0 | 1 | 9 |
| [src/opengl/scene/Scene.cpp](/src/opengl/scene/Scene.cpp) | C++ | 72 | 0 | 14 | 86 |
| [src/opengl/utils/Options.cpp](/src/opengl/utils/Options.cpp) | C++ | 41 | 0 | 13 | 54 |
| [test-app/CMakeLists.txt](/test-app/CMakeLists.txt) | CMake | 24 | 0 | 5 | 29 |
| [test-app/src/DioramaScene.cpp](/test-app/src/DioramaScene.cpp) | C++ | 262 | 30 | 42 | 334 |
| [test-app/src/DioramaScene.h](/test-app/src/DioramaScene.h) | C++ | 48 | 2 | 17 | 67 |
| [test-app/src/NeonCityScene.cpp](/test-app/src/NeonCityScene.cpp) | C++ | 112 | 8 | 23 | 143 |
| [test-app/src/NeonCityScene.h](/test-app/src/NeonCityScene.h) | C++ | 18 | 3 | 5 | 26 |
| [test-app/src/SampleScene.cpp](/test-app/src/SampleScene.cpp) | C++ | 245 | 45 | 38 | 328 |
| [test-app/src/SampleScene.h](/test-app/src/SampleScene.h) | C++ | 43 | 12 | 15 | 70 |
| [test-app/src/SolarSystemScene.cpp](/test-app/src/SolarSystemScene.cpp) | C++ | 442 | 68 | 66 | 576 |
| [test-app/src/SolarSystemScene.h](/test-app/src/SolarSystemScene.h) | C++ | 80 | 16 | 17 | 113 |
| [test-app/src/main.cpp](/test-app/src/main.cpp) | C++ | 31 | 0 | 11 | 42 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)