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

**Not yet integrated (planned):**
- `GLM` — math (vectors, matrices, transforms)
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
  core/
    Renderer.h/.cpp       # OpenGL initialization, RenderFrame, Resize, Shutdown
    Buffer.h/.cpp         # RAII wrapper for VBO/EBO (GL buffer objects)
    VertexArray.h/.cpp    # RAII wrapper for VAO (GL vertex array objects)
    VertexLayout.h/.cpp   # Describes vertex attribute layout; drives glVertexAttribPointer
    MeshBuffer.h/.cpp     # GPU mesh abstraction: owns VAO + VBO + EBO, exposes Bind/Draw
  utils/
    Options.h/.cpp        # Reads config/settings.json; provides window size, vsync, etc.
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
        └── Renderer       (owns GL state: viewport, clear, future: draw calls)
              └── [future: ShaderProgram, MeshBuffer instances, etc.]
```

`Application` handles GLFW lifecycle and the main loop. `Renderer` handles everything OpenGL. They are intentionally separated so the renderer layer can grow without touching the application layer.

### Per-Frame Flow (current)

```
glfwPollEvents()
  → Renderer::RenderFrame()   [clear screen only — no geometry yet]
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
- **RAII for all GPU resources.** `Buffer`, `VertexArray`, and `MeshBuffer` delete their GL objects in destructors. Copy is disabled; move is allowed.
- **VAO/EBO binding order matters.** The EBO is VAO state in OpenGL — never unbind `GL_ELEMENT_ARRAY_BUFFER` while a VAO is bound. This is documented in `Buffer.cpp` and `MeshBuffer.cpp` and is a frequent source of bugs.
- **Explicit public headers.** `Application` depends on `Renderer.h`, not internal implementation files. This boundary is enforced by design.

---

## Core Classes (Sprint 2 state)

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
Combines VAO + VBO + EBO into a single uploadable, drawable object. Constructor takes raw CPU vertex/index data and a `VertexLayout`, uploads everything to GPU, and configures the VAO. Exposes `Bind()`, `Unbind()`, `Draw()` (calls `glDrawElements`). Does not know about shaders, materials, or transforms.

### `Options` (`src/utils/Options.h`)
Reads `config/settings.json` on construction. Falls back to defaults (1280×720, vsync on) silently if the file is missing; logs on malformed JSON. Currently provides only `window` config.

---

## Git Workflow

Read `GITFLOW.md` for the full rules. Summary:

- Branch from `dev`. Never commit directly to `dev` or `main`.
- Branch names: `feature/<name>`, `bugfix/<name>`, `docs/<name>`, `refactor/<name>`, `hotfix/<name>`, `test/<name>`
- Commit messages: `type(scope): description` (Conventional Commits)
- Open PRs targeting `dev`. Merge via PR only.
- No local merge commits. No rebasing on protected branches.

---

## Current Sprint Status (as of Sprint 2)

**Done:**
- GLFW window, GLAD loading, OpenGL 4.6 Core Profile context
- GL debug callback (high/medium severity → spdlog)
- Application/Renderer lifecycle (Initialize → Run → ~Application)
- Framebuffer resize → `Renderer::Resize()` → `glViewport`
- JSON config loading (`Options`)
- `Buffer`, `VertexArray`, `VertexLayout`, `MeshBuffer` GPU abstractions

**Not yet done (Sprint 2 gaps):**
- `ShaderProgram` abstraction — no shader loading/compilation/linking system exists
- No `.glsl` shader files
- Built-in primitive geometry (triangle, quad, cube) not yet implemented
- `Renderer::RenderFrame()` only clears — `MeshBuffer` is not yet wired into draw calls
- No fallback/error shader strategy

**Coming next (Sprint 3):**
- Perspective camera with view/projection matrices
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
