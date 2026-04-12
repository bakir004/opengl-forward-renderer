# CLAUDE.md — Forward Renderer Project Context

This file is for AI assistants (Claude Code and similar tools). It describes the project, its current state, architecture, conventions, and how to navigate the codebase effectively.

---

## Project Overview

This is a **modern OpenGL forward renderer** written in C++20, built as a one-term academic project. The goal is to implement a credible, modular graphics subsystem — not a game engine. It targets Windows desktop (OpenGL 4.6 Core Profile) and is structured to grow sprint-by-sprint toward PBR lighting, shadow mapping, HDR post-processing, and a debug UI.

The renderer is the only engine subsystem in scope. There is no physics, audio, gameplay logic, or scripting.

---

## Build System

**CMake 3.26+** with FetchContent for all third-party dependencies.

```bash
# Build (Release by default; pass "Debug" for debug info)
./build.sh          # Linux/macOS/Git Bash
build.bat           # Windows CMD/PowerShell

# Re-run already-compiled binary without rebuilding
./run.sh
run.bat
```

The build script also installs git hooks from `.githooks/`.

**Dependencies (auto-fetched via CMake):**
- `glfw 3.4` — windowing and OpenGL context
- `glad` (local, in `external/glad/`) — OpenGL function loader
- `spdlog v1.15.3` — structured logging
- `nlohmann/json v3.11.3` — JSON config parsing
- `GLM 1.0.1` — math (vectors, matrices, transforms)
- `stb` (master) — texture loading via stb_image
- `Dear ImGui v1.90.8` — debug UI overlay
- `Assimp v5.4.3` — mesh import (OBJ, glTF, FBX, DAE)

---

## Repository Layout

```
src/
  include/              # Public headers (backend-agnostic)
    app/
      Application.h     # Top-level owner: GLFW window, GL context, main loop
    core/
      Buffer.h          # RAII VBO/EBO wrapper
      Camera.h          # Perspective camera (FreeFly / FirstPerson / ThirdPerson)
      CameraController.h# Input → Camera adapters (FreeFly, FirstPerson, Standard)
      FrameClearInfo.h  # Viewport + clear color data passed each frame
      InitContext.h     # One-time GL initialization (GLAD, debug callback)
      KeyboardInput.h   # Keyboard state snapshot
      Material.h        # Material + MaterialInstance (shader + params + textures)
      Mesh.h            # Multi-submesh GPU mesh (Assimp pipeline)
      MeshBuffer.h      # Simple single-mesh GPU object (VAO + VBO + EBO)
      MeshData.h        # CPU-side mesh data before GPU upload
      MouseInput.h      # Mouse state snapshot (cursor delta, buttons, scroll)
      Primitives.h      # CPU geometry generators: triangle, quad, cube
      RenderQueue.h     # Sorted draw queue flushed each frame
      Renderer.h        # OpenGL pipeline orchestrator
      ShaderProgram.h   # Shader compilation, linking, typed uniform API
      SubMesh.h         # One draw range within a Mesh
      SubmissionContext.h # Per-frame pipeline state snapshot
      Texture2D.h       # RAII 2D texture (stb_image, sRGB/Linear, mipmaps)
      Transform.h       # TRS transform with cached model matrix
      UniformBuffer.h   # UBO wrapper (Upload + BindToSlot)
      VertexArray.h     # RAII VAO wrapper
      VertexLayout.h    # Vertex attribute layout descriptor
      shadows/
        ShadowMap.h     # Depth-only FBO for directional shadow map
    scene/
      FrameSubmission.h # Per-frame data bundle: camera + lights + objects
      GpuLightData.h    # std140-packed light structs for GPU upload
      Light.h           # CPU light types: DirectionalLight, PointLight, SpotLight
      LightBlock.h      # std140 UBO block struct for all lights
      LightBuilder.h    # Fluent light construction helpers
      LightEnvironment.h# Scene-facing light container (1 dir + 16 point + 8 spot)
      LightUtils.h      # Light packing / validation utilities
      RenderItem.h      # One renderable object for a frame
      SceneUtils.h      # Scene-level helper utilities
      Scene.h           # Base class for all scenes
    utils/
      Options.h         # Reads config/settings.json
  opengl/               # OpenGL backend implementations
    app/Application.cpp
    assets/
      AssetImporter.cpp
      MeshImporter.cpp
    core/               # .cpp files matching each header above
    scene/
    utils/
  include/assets/
    AssetImporter.h     # Centralised asset loading + caching hub

test-app/               # Executable target — demo scenes
  src/
    main.cpp            # Entry point; sets up app and runs scenes
    SampleScene.h/.cpp  # Primitive demo (triangle, quad, cube)
    SolarSystemScene.h/.cpp  # Orbital animation scene
    DioramaScene.h/.cpp # Combined interior + exterior scene

assets/
  shaders/
    basic.vert/.frag    # Per-vertex color, clip-space (no MVP)
    mesh.vert/.frag     # Main forward shader: MVP + Blinn-Phong + shadow sampling
    shadow_depth.vert/.frag  # Depth-only pass for shadow map generation
    light_block.glsl    # Shared GLSL include: LightBlock UBO declaration
    error.vert/.frag    # Fallback shader (magenta) for missing/broken shaders
  models/               # Assimp-importable model files (OBJ, glTF)
  materials/
    default.mat         # JSON material descriptor

config/
  settings.json         # Runtime config: window, shadow_map, logging, paths, debug
external/               # FetchContent downloads — do not edit manually
.githooks/              # Git hooks enforcing branch naming and commit format
GITFLOW.md              # Branch/commit conventions (read this before committing)
SRS_ForwardRenderer_3.pdf  # Full Software Requirements Specification
```

---

## Architecture

### Ownership Chain

```
main()  (test-app/src/main.cpp)
  └── Application          (owns GLFW window + Renderer + input lifetime)
        ├── Renderer        (owns GL pipeline: InitContext, RenderQueue, UBOs, ShadowMap)
        └── Scene (base)    (owns Camera, LightEnvironment, list of RenderItems)
              ├── SampleScene
              ├── SolarSystemScene
              └── DioramaScene
```

`Application::Run(scenes, index)` drives the loop with numeric-key scene switching (1–9). Each `Scene` subclass overrides `OnUpdate()` to handle input and update state; `Scene::BuildSubmission()` (called by Application) packages camera + lights + objects into a `FrameSubmission` for the Renderer.

### Per-Frame Flow (current)

```
glfwPollEvents() + input update
  → Scene::InternalUpdate()         [update camera aspect + call OnUpdate]
  → Scene::BuildSubmission()        [pack camera + lights + objects → FrameSubmission]
  → Renderer::BeginFrame(submission) [shadow pass + clear + camera UBO + light UBO]
  → Renderer::SubmitDraw(item) × N  [enqueue RenderItems]
  → Renderer::EndFrame()            [sort queue, flush draws, reset queue]
  → ImGui frame render
  → glfwSwapBuffers()
```

### Key Design Principles

- **No raw GL calls in application code.** `main.cpp`, `Application`, and `Scene` subclasses never call `glXxx`. That belongs in the `src/opengl/` backend.
- **RAII for all GPU resources.** `Buffer`, `VertexArray`, `MeshBuffer`, `Texture2D`, `ShadowMap`, and `ShaderProgram` delete their GL objects in destructors. Copy is disabled; move is allowed.
- **VAO/EBO binding order matters.** The EBO is VAO state in OpenGL — never unbind `GL_ELEMENT_ARRAY_BUFFER` while a VAO is bound. This is documented in `Buffer.cpp` and `MeshBuffer.cpp` and is a frequent source of bugs.
- **Header/implementation split.** All public headers in `src/include/` contain no GL types (`GLuint`, etc.). GL is only visible in `src/opengl/` implementations.
- **Renderer is a thin orchestrator.** It owns `InitContext`, `SubmissionContext`, and `RenderQueue` but contains no direct GL calls — those live in those helper classes.
- **Asset caching via AssetImporter.** Never construct `ShaderProgram`, `Texture2D`, or `MeshBuffer` directly in scene code — use `AssetImporter::Import<T>()` or the typed loaders so resources are deduplicated.

---

## Core Classes

### GPU Primitives

**`Buffer`** (`src/include/core/Buffer.h`)  
RAII wrapper for a single OpenGL buffer object (VBO, EBO, or UBO). Exposes `Bind()`, `Unbind()`, `UpdateData()`.

**`VertexArray`** (`src/include/core/VertexArray.h`)  
RAII VAO wrapper. Exposes `Bind()` and `Unbind()`.

**`VertexLayout`** (`src/include/core/VertexLayout.h`)  
Describes vertex attribute memory layout. Computes stride and byte offsets automatically. Call `Apply()` while the correct VAO and VBO are bound.

**`MeshBuffer`** (`src/include/core/MeshBuffer.h`)  
Simple single-mesh GPU object: VAO + VBO + optional EBO. Supports indexed (`glDrawElements`) and non-indexed (`glDrawArrays`) rendering. Does not know about shaders, materials, or transforms.

**`Mesh`** (`src/include/core/Mesh.h`)  
Multi-submesh GPU mesh produced by `AssetImporter`/Assimp. Exposes `DrawSubMesh(index)`, `DrawAll()`, `SubMeshCount()`. Uses a pImpl to keep GL types out of the header.

**`UniformBuffer`** (`src/include/core/UniformBuffer.h`)  
UBO wrapper. `Upload(data, size)` writes CPU data via `glBufferSubData`; `BindToSlot(N)` calls `glBindBufferBase`. Requires std140-compatible CPU structs (vec3 padded to vec4, etc.).

**`Texture2D`** (`src/include/core/Texture2D.h`)  
RAII 2D texture. Loads via stb_image, uploads to GPU, generates mipmaps. Supports `TextureColorSpace::sRGB` (GL_SRGB8_ALPHA8) and `Linear` (GL_RGBA8). Static helpers: `CreateFallback(r,g,b,a)` and `CreateCheckerboard()` for missing-asset indicators.

**`ShadowMap`** (`src/include/core/shadows/ShadowMap.h`)  
RAII depth-only FBO + depth texture for directional shadow mapping. `Bind()` activates it for the shadow pass; `Unbind()` restores the default FBO.

**`ShaderProgram`** (`src/include/core/ShaderProgram.h`)  
Loads GLSL from disk, compiles, links, caches uniform locations. Typed `SetUniform()` for `float`, `int`, `bool`, `glm::vec2/3/4`, `glm::mat3/4`. Exposes `Bind()`, `static Unbind()`, `IsValid()`, `GetID()`.

### Math / Transform

**`Transform`** (`src/include/core/Transform.h`)  
TRS transform with lazy-cached model matrix. Euler angles in degrees, composition order Y→X→Z, column-major `M = T * R * S`. Exposes `SetTranslation`, `SetRotationEulerDegrees`, `SetScale`, `GetModelMatrix()`.

### Camera

**`Camera`** (`src/include/core/Camera.h`)  
Pure-math perspective camera with lazy matrix recomputation. Three modes: `FreeFly` (six DoF), `FirstPerson` (pitch clamped, world-up locked), `ThirdPerson` (orbits a target). Key API: `Move(direction, speed, deltaTime)`, `Rotate(dYaw, dPitch)`, `GetView()`, `GetProjection()`, `BuildCameraData()` (fills the per-frame UBO struct). `OnResize(w, h)` recomputes aspect ratio.

**`CameraController`** (`src/include/core/CameraController.h`)  
Abstract base (`Update(deltaTime, keyboard, mouse)`). Concrete subclasses: `FreeFlyController`, `FirstPersonController`. `StandardSceneCameraController` handles the full scene-level WASD + TAB + F1/F2/F3 + RMB-hold-to-look input mapping and is owned by the `Scene` base class.

### Materials

**`Material`** (`src/include/core/Material.h`)  
Immutable type: owns a `ShaderProgram` plus default float/vec3/vec4/texture parameters. `Bind()` binds the shader and uploads defaults.

**`MaterialInstance`** (`src/include/core/Material.h`)  
Per-object override layer. Inherits from a `Material` and overrides any subset of its parameters. `Bind()` calls the parent's `Bind()` then applies instance overrides with consistent GL texture unit assignment.

Texture slot names are defined in the `TextureSlot` namespace (`u_AlbedoMap`, `u_NormalMap`, `u_MetallicMap`, `u_RoughnessMap`, `u_AOMap`, `u_EmissiveMap`).

### Scene / Submission API

**`Scene`** (`src/include/scene/Scene.h`)  
Base class for all scenes. Owns a `Camera`, `LightEnvironment`, and `std::vector<RenderItem>`. Subclasses declare content in their setup method via `AddObject()`, `SetCamera()`, `SetClearColor()`, and override `OnUpdate(deltaTime, input, mouse)` for per-frame logic. Calls `UpdateStandardCameraAndPlayer()` for shared input handling. `Application` calls `BuildSubmission()` to get a `FrameSubmission` each frame.

**`RenderItem`** (`src/include/scene/RenderItem.h`)  
One renderable object for a frame. Holds `Transform`, references to `MeshBuffer` (or `Mesh` + submesh index), and either a `ShaderProgram*` (legacy) or a `MaterialInstance*` (takes priority). Also carries `RenderFlags` (`visible`, `castShadow`, `receiveShadow`), `DrawMode` (Fill/Wireframe/Points), and `PrimitiveTopology`.

**`FrameSubmission`** (`src/include/scene/FrameSubmission.h`)  
Per-frame data bundle passed from `Scene` to `Renderer`: camera pointer, `FrameClearInfo`, `SubmissionContext`, `LightEnvironment`, and the object list.

**`LightEnvironment`** (`src/include/scene/LightEnvironment.h`)  
Scene-facing light container. Holds at most one `DirectionalLight`, up to `kMaxPointLights` (16) `PointLight`s, and up to `kMaxSpotLights` (8) `SpotLight`s. Provides ambient color + intensity.

**`Light` types** (`src/include/scene/Light.h`)  
- `DirectionalLight` — direction, color, intensity, `LightShadowParams`
- `PointLight` — position, radius, `Attenuation`, color, intensity, `LightShadowParams`
- `SpotLight` — position, direction, `SpotCone` (inner/outer degrees), `Attenuation`, color, intensity, `LightShadowParams`

### Renderer

**`Renderer`** (`src/include/core/Renderer.h`)  
Thin orchestrator. `Initialize()` loads GLAD and sets up the GL debug callback. Per-frame: `BeginFrame(submission)` runs the directional shadow pass, clears, uploads camera + light UBOs; `SubmitDraw(item)` enqueues a `RenderItem`; `EndFrame()` sorts the queue by shader and flushes all draws. `GetDebugStats()` returns a `RendererDebugStats` snapshot for the ImGui debug panel (draw calls, triangle count, shadow caster/receiver counts, FPS, frame time, shadow map preview texture ID).

### Assets

**`AssetImporter`** (`src/include/assets/AssetImporter.h`)  
Centralised loading and caching hub. All resources are returned as `shared_ptr`; same canonical path returns the cached instance.
- `Import<T>(path)` — generic wildcard loader (extension-dispatched)
- `LoadShader(vert, frag)` — explicit shader pair
- `LoadTexture(path, colorSpace, sampler, flipY)` — explicit texture
- `LoadMesh(path)` — first mesh from an Assimp-imported file
- `LoadMaterial(path)` — JSON `.mat` descriptor
- `CollectUnused()` / `Clear()` — cache eviction

### Primitives

**`Primitives`** (`src/include/core/Primitives.h`)  
Factory functions returning CPU-side `PrimitiveMeshData` (`std::vector<VertexPC>` + indices). `VertexPC = {vec3 position, vec3 color}` at locations 0 and 1. Call `data.CreateMeshBuffer()` to upload. Available: `GenerateTriangle()`, `GenerateQuad()`, `GenerateCube()`.

### Input

**`KeyboardInput`** (`src/include/core/KeyboardInput.h`)  
Per-frame keyboard state snapshot. Polled by `Application` and passed to `Scene::OnUpdate`.

**`MouseInput`** (`src/include/core/MouseInput.h`)  
Per-frame mouse state: cursor delta, button states, scroll. Mouse capture toggled by Tab (standard scenes) or RMB hold.

### Config

**`Options`** (`src/include/utils/Options.h`)  
Reads `config/settings.json`. Falls back to defaults on missing file; logs on malformed JSON. Provides `window` (size, title, vsync) and `shadow_map` (width, height) config.

---

## Shaders (`assets/shaders/`)

| File | Purpose |
|------|---------|
| `basic.vert/.frag` | Per-vertex color, clip-space (no MVP) — used by SampleScene primitives |
| `mesh.vert/.frag` | Main forward shader: MVP transforms, Blinn-Phong lighting, shadow map sampling |
| `shadow_depth.vert/.frag` | Depth-only pass — renders scene geometry into the directional shadow map |
| `light_block.glsl` | Shared GLSL include: `LightBlock` UBO declaration (included by `mesh.frag`) |
| `error.vert/.frag` | Magenta fallback shader for broken/missing assets |

---

## Git Workflow

Read `GITFLOW.md` for the full rules. Summary:

- Branch from `dev`. Never commit directly to `dev` or `master`.
- Branch names: `feature/<name>`, `bugfix/<name>`, `docs/<name>`, `refactor/<name>`, `hotfix/<name>`, `test/<name>`
- Commit messages: `type(scope): description` (Conventional Commits)
- Open PRs targeting `dev`. Merge via PR only.
- No local merge commits. No rebasing on protected branches.

---

## Current Sprint Status (Sprints 1–6 complete)

**Done:**
- GLFW window, GLAD loading, OpenGL 4.6 Core Profile context
- GL debug callback (high/medium severity → spdlog)
- Application/Renderer lifecycle + framebuffer resize → viewport
- JSON config loading (`Options`), including `shadow_map` resolution config
- `Buffer`, `VertexArray`, `VertexLayout`, `MeshBuffer` GPU abstractions
- `ShaderProgram` — file loading, compilation, linking, typed uniform API
- Built-in primitive geometry (`GenerateTriangle`, `GenerateQuad`, `GenerateCube`)
- `SampleScene` rendering all three primitives
- Pipeline state management (depth test, blending, face culling)
- `Transform` — TRS with lazy cached model matrix
- `Camera` — perspective, FreeFly/FirstPerson/ThirdPerson modes, lazy matrix cache
- `CameraController` hierarchy — FreeFly, FirstPerson, StandardSceneCameraController
- Camera controls: WASD, Tab to toggle mouse capture, RMB hold-to-look, sprint boost, zoom, F1/F2/F3 mode switching, speed adjustment
- `RenderItem` / `FrameSubmission` scene submission API
- `Scene` base class; `SampleScene`, `SolarSystemScene`, `DioramaScene` subclasses
- Runtime scene switching via numeric keys 1–N
- `UniformBuffer` — UBO upload + slot binding
- Camera UBO uploaded per frame; shaders read via `layout(std140, binding=0)`
- `LightEnvironment` — directional + point + spot light containers
- `Light` types with shadow params and attenuation
- Light UBO (`LightBlock`) uploaded per frame via `binding=1`
- `Texture2D` — stb_image loading, sRGB/Linear, mipmaps, fallback textures
- `Material` / `MaterialInstance` — shader + parameter + texture management
- `AssetImporter` — centralized caching loader for shaders, textures, meshes, materials
- `Mesh` / `SubMesh` — multi-submesh GPU mesh via Assimp
- Assimp model import (OBJ, glTF, FBX, DAE)
- `ShadowMap` RAII depth FBO; directional shadow pass in `Renderer::BeginFrame`
- Blinn-Phong shading in `mesh.frag` with PCF shadow sampling
- Dear ImGui integrated: runtime stats panel (FPS, frame time, draw calls, triangle count, light counts, shadow caster/receiver counts, shadow map preview)
- `RendererDebugStats` populated each frame; exposed via `Renderer::GetDebugStats()`
- `run.sh` / `run.bat` for re-running compiled binary without rebuilding

**Coming next (Sprint 7+):**
- PBR (metallic/roughness) shader
- HDR framebuffer + tone mapping + bloom
- Frustum culling
- Skybox / environment map

---

## Conventions

- C++20, PascalCase for classes and public methods, `m_` prefix for private members
- Header guards: `#pragma once`
- Logging: always use `spdlog::info/warn/error(...)`, never `printf` or `std::cout`
- GL object IDs stored as `GLuint m_id = 0`; `0` means uninitialized/moved-from
- `GLsizeiptr` for byte sizes passed to GL (avoids 64-bit truncation)
- Doc comments on all public methods in headers (Doxygen-style `///`)
- Use `glm::` types for all math (vec2/3/4, mat3/4) — never roll your own
- No GL types in `src/include/` headers — GL is only visible in `src/opengl/` implementations
- All scene assets loaded through `AssetImporter`, never constructed directly
