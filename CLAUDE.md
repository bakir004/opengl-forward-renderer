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
# Linux/macOS/Git Bash
./build.sh

# Windows CMD/PowerShell
build.bat
```

The build script also installs git hooks from `.githooks/`.

**Dependencies (auto-fetched via CMake):**
- `glfw 3.4` — windowing and OpenGL context
- `glad` (local, in `external/glad/`) — OpenGL function loader
- `spdlog v1.15.3` — structured logging
- `nlohmann/json v3.11.3` — JSON config parsing
- `GLM 1.0.1` — math (vectors, matrices, transforms)

**Not yet integrated (planned):**
- `Assimp` — mesh import
- `Dear ImGui` — debug UI
- `stb_image` — texture loading

---

## Repository Layout

```
src/
  app/
    main.cpp              # Entry point — creates Application, calls Initialize + Run
    Application.h/.cpp    # Top-level owner: GLFW window, GL context, main loop
    SampleScene.h/.cpp    # Sprint 2 demo scene: loads shader + draws triangle/quad/cube
  core/
    Renderer.h/.cpp       # OpenGL pipeline: BeginFrame/EndFrame, SubmitDraw, pipeline state
    Buffer.h/.cpp         # RAII wrapper for VBO/EBO (GL buffer objects)
    VertexArray.h/.cpp    # RAII wrapper for VAO (GL vertex array objects)
    VertexLayout.h/.cpp   # Describes vertex attribute layout; drives glVertexAttribPointer
    MeshBuffer.h/.cpp     # GPU mesh abstraction: owns VAO + VBO + EBO, exposes Bind/Draw
    ShaderProgram.h/.cpp  # Shader file loading, compilation, linking, uniform cache
    Primitives.h/.cpp     # CPU-side geometry generators: triangle, quad, cube
  utils/
    Options.h/.cpp        # Reads config/settings.json; provides window size, vsync, etc.
shaders/
  basic.vert              # Pass-through vertex shader (clip-space, per-vertex color)
  basic.frag              # Simple color interpolation fragment shader
config/
  settings.json           # Runtime config: window dimensions, vsync, log level, asset root
external/                 # FetchContent downloads land here; do not edit manually
.githooks/                # Git hooks enforcing branch naming and commit format
GITFLOW.md                # Branch/commit conventions (read this before committing)
SRS.pdf                   # Full Software Requirements Specification (19 pages)
```

---

## Architecture

### Ownership Chain

```
main()
  └── Application          (owns window + renderer lifetime)
        ├── Renderer        (owns GL state: viewport, clear, pipeline, draw submission)
        └── SampleScene     (owns ShaderProgram + MeshBuffer instances for demo)
```

`Application` handles GLFW lifecycle and the main loop. `Renderer` handles everything OpenGL. `SampleScene` is a temporary demonstration harness that will be replaced by a proper scene/camera system in Sprint 3.

### Per-Frame Flow (current)

```
glfwPollEvents()
  → Renderer::BeginFrame()    [clear color + depth buffers]
  → SampleScene::Render()     [SubmitDraw × 3 — triangle, quad, cube]
  → Renderer::EndFrame()      [assert frame state]
  → glfwSwapBuffers()
```

### Per-Frame Flow (planned per SRS §6.3)

```
begin frame
  → consume config toggles
  → receive camera / lights / render items
  → visibility / frustum culling
  → shadow pass
  → forward opaque scene
  → skybox
  → HDR / tone-map / bloom
  → debug UI
  → present
```

### Key Design Principles

- **No raw GL calls in application code.** `main.cpp` and `Application` should never call `glXxx`. That belongs in `Renderer` and the core layer.
- **RAII for all GPU resources.** `Buffer`, `VertexArray`, `MeshBuffer`, and `ShaderProgram` delete their GL objects in destructors. Copy is disabled; move is allowed.
- **VAO/EBO binding order matters.** The EBO is VAO state in OpenGL — never unbind `GL_ELEMENT_ARRAY_BUFFER` while a VAO is bound. This is documented in `Buffer.cpp` and `MeshBuffer.cpp` and is a frequent source of bugs.
- **Explicit public headers.** `Application` depends on `Renderer.h`, not internal implementation files. This boundary is enforced by design.
- **State caching in Renderer.** Pipeline state (depth test, blend mode, cull mode, clear color) is cached to avoid redundant GL calls. Always mutate state through `Renderer` setters, not raw GL.

---

## Core Classes (Sprint 2 complete)

### `Buffer` (`src/core/Buffer.h`)
RAII wrapper for a single OpenGL buffer object (VBO or EBO). Takes `Type::VERTEX` or `Type::ELEMENT`. Calls `glGenBuffers` on construction, `glDeleteBuffers` on destruction. Exposes `Bind()`, `Unbind()`, `UpdateData()`.

### `VertexArray` (`src/core/VertexArray.h`)
RAII wrapper for a VAO. Calls `glGenVertexArrays`/`glDeleteVertexArrays`. Exposes `Bind()` and `Unbind()`.

### `VertexLayout` (`src/core/VertexLayout.h`)
Describes the memory layout of one vertex (which attributes, in what order, of what type). Automatically computes stride and per-attribute byte offsets. Call `Apply()` while the correct VAO and VBO are bound — it calls `glVertexAttribPointer` and `glEnableVertexAttribArray` for each attribute.

Example:
```cpp
VertexLayout layout({
    {0, 3, GL_FLOAT, GL_FALSE},  // position: vec3 at location 0
    {1, 3, GL_FLOAT, GL_FALSE},  // color:    vec3 at location 1
});
```

### `MeshBuffer` (`src/core/MeshBuffer.h`)
Combines VAO + VBO + optional EBO into a single uploadable, drawable object. Supports both indexed (`glDrawElements`) and non-indexed (`glDrawArrays`) rendering. Constructor takes raw CPU vertex/index data and a `VertexLayout`, uploads everything to GPU, and configures the VAO. Exposes `Bind()`, `Unbind()`, `Draw()`, `IsIndexed()`. Does not know about shaders, materials, or transforms.

### `ShaderProgram` (`src/core/ShaderProgram.h`)
Loads GLSL source from disk, compiles vertex and fragment stages, links the program, and reports errors via spdlog. Caches uniform locations for efficient per-frame updates. Provides typed `SetUniform()` overloads for `float`, `int`, `bool`, `glm::vec2/3/4`, `glm::mat3/4`. Exposes `Bind()`, `static Unbind()`, `IsValid()`, `GetID()`.

### `Primitives` (`src/core/Primitives.h`)
Factory functions returning CPU-side geometry as `PrimitiveMeshData` (contains `std::vector<VertexPC>` + `std::vector<uint32_t>` indices). `VertexPC` is `{glm::vec3 position, glm::vec3 color}` at locations 0 and 1. Call `data.CreateMeshBuffer()` to upload to GPU. Available generators:
- `GenerateTriangle()` — 3 vertices, non-indexed
- `GenerateQuad()` — 4 vertices, 6 indices
- `GenerateCube()` — 24 vertices, 36 indices

### `Renderer` (`src/core/Renderer.h`)
Core OpenGL pipeline manager. `Initialize()` loads GLAD and sets up the GL debug callback. Frame lifecycle: `BeginFrame()` clears buffers, `EndFrame()` validates state. `SubmitDraw(ShaderProgram&, MeshBuffer&)` binds both objects, calls `Draw()`, then unbinds. Pipeline state setters (all cached):
- `SetDepthTest(enable, func)`
- `SetBlendMode(BlendMode)` — `Disabled`, `Alpha`, `Additive`, `Multiply`
- `SetCullMode(CullMode)` — `Disabled`, `Back`, `Front`, `FrontAndBack`
- `SetClearColor(glm::vec4)`
- `SetViewport(x, y, w, h)` or `SetViewport(Viewport&)`

### `SampleScene` (`src/app/SampleScene.h`)
Temporary demo harness. `Setup(vertexPath, fragmentPath)` loads a shader and uploads three primitive meshes. `Render(Renderer&, timeSeconds)` issues three `SubmitDraw()` calls with appropriate pipeline state. Uses clip-space vertex positions (no MVP transforms). Will be replaced by a proper camera/scene system in Sprint 3.

### `Options` (`src/utils/Options.h`)
Reads `config/settings.json` on construction. Falls back to defaults (1280×720, vsync on) silently if the file is missing; logs on malformed JSON. Currently provides only `window` config via `WindowOpts`.

---

## Git Workflow

Read `GITFLOW.md` for the full rules. Summary:

- Branch from `dev`. Never commit directly to `dev` or `main`.
- Branch names: `feature/<name>`, `bugfix/<name>`, `docs/<name>`, `refactor/<name>`, `hotfix/<name>`, `test/<name>`
- Commit messages: `type(scope): description` (Conventional Commits)
- Open PRs targeting `dev`. Merge via PR only.
- No local merge commits. No rebasing on protected branches.

---

## Current Sprint Status (as of Sprint 2 — complete)

**Done:**
- GLFW window, GLAD loading, OpenGL 4.6 Core Profile context
- GL debug callback (high/medium severity → spdlog)
- Application/Renderer lifecycle (Initialize → Run → ~Application)
- Framebuffer resize → `Renderer::Resize()` → `glViewport`
- JSON config loading (`Options`)
- `Buffer`, `VertexArray`, `VertexLayout`, `MeshBuffer` GPU abstractions
- `ShaderProgram` — file loading, compilation, linking, typed uniform API
- `shaders/basic.vert` and `shaders/basic.frag` — clip-space, per-vertex color
- Built-in primitive geometry (`GenerateTriangle`, `GenerateQuad`, `GenerateCube`)
- `SampleScene` rendering all three primitives via `SubmitDraw`
- Pipeline state management (depth test, blending, face culling) with caching
- `BeginFrame`/`EndFrame` frame lifecycle replacing old `RenderFrame`
- GLM integrated for math types

**Coming next (Sprint 3):**
- Perspective camera with view/projection matrices (GLM)
- Transform system (model matrix generation)
- `RenderItem` / `FrameSubmission` scene submission API
- Per-frame uniform blocks (camera data via UBOs)
- Initial ImGui debug overlay

---

## Conventions

- C++20, PascalCase for classes and public methods, `m_` prefix for private members
- Header guards: `#pragma once`
- Logging: always use `spdlog::info/warn/error(...)`, never `printf` or `std::cout`
- GL object IDs stored as `GLuint m_id = 0`; `0` means uninitialized/moved-from
- `GLsizeiptr` for byte sizes passed to GL (avoids 64-bit truncation)
- Doc comments on all public methods in headers (Doxygen-style `///`)
- Use `glm::` types for all math (vec2/3/4, mat3/4) — never roll your own
