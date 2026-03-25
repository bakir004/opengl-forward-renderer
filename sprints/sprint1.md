# Sprint 1 — Project Setup, Platform Layer, and OpenGL Bootstrap

**Status:** Complete
**Objective:** Establish the engineering baseline — a reproducible build, working git workflow, and a GLFW window with an active OpenGL 4.6 Core Profile context.

---

## What Was Done

### Repository and Toolchain Setup
- Initialized the Git repository with a branching strategy (`feature/`, `bugfix/`, `docs/`, `refactor/`, `hotfix/`, `test/` prefixes from `dev`)
- Created `GITFLOW.md` documenting all branch naming rules, commit message conventions (Conventional Commits), and forbidden operations
- Installed git hooks via `build.sh` / `build.bat` that enforce those rules automatically on every commit and push
- Set up `CMakeLists.txt` with CMake 3.26+ and FetchContent for reproducible third-party dependency acquisition

### Third-Party Libraries Integrated
| Library | Version | Purpose |
|---|---|---|
| GLFW | 3.4 | Window creation, OpenGL context, input polling |
| GLAD | local (external/glad/) | OpenGL 4.6 function pointer loading |
| spdlog | v1.15.3 | Structured logging with severity levels |
| nlohmann/json | v3.11.3 | JSON config file parsing |

GLM, Assimp, Dear ImGui, and stb_image are planned but not yet integrated.

### Files Added
| File | Purpose |
|---|---|
| `src/app/main.cpp` | Entry point. Creates `Application`, calls `Initialize()` then `Run()`. |
| `src/app/Application.h` | Declares the top-level `Application` class. Owns `GLFWwindow*` and `unique_ptr<Renderer>`. |
| `src/app/Application.cpp` | Implements GLFW init, window creation, context setup, framebuffer resize callback, and the main loop (`PollEvents → RenderFrame → SwapBuffers`). Reads config via `Options`. |
| `src/core/Renderer.h` | Declares `Renderer` with `Initialize`, `RenderFrame`, `Resize`, `Shutdown`. |
| `src/core/Renderer.cpp` | Loads GL via GLAD, logs vendor/GPU/version info, sets up GL debug callback (debug builds only), implements resize and clear-only render frame. |
| `src/utils/Options.h` | `WindowOpts` struct + `Options` struct. Declares JSON deserialization for window settings. |
| `src/utils/Options.cpp` | Reads and parses `config/settings.json`. Falls back to defaults on missing file, logs on malformed JSON. |
| `config/settings.json` | Runtime config: window size (1280×720), vsync, title, log level, asset root path. |
| `CMakeLists.txt` | Build definition: fetches dependencies, compiles all sources, links against glad/glfw/OpenGL/spdlog/json. |
| `build.sh` / `build.bat` | Build scripts for Linux/macOS/Windows. Also install git hooks. |
| `GITFLOW.md` | Full git workflow guide. |
| `.githooks/` | Hook scripts enforcing branch naming and commit format. |

### What Each Piece Does (How It Fits Together)

**Startup sequence:**
1. `main()` constructs `Application` (which constructs `Renderer` via `unique_ptr`)
2. `app.Initialize()` runs:
   - `glfwInit()`
   - Reads `config/settings.json` via `Options`
   - Sets OpenGL hints (4.6 Core Profile, debug context in debug builds)
   - `glfwCreateWindow(width, height, title, ...)`
   - `glfwMakeContextCurrent(window)`
   - Registers the framebuffer resize callback
   - Sets vsync via `glfwSwapInterval`
   - Calls `renderer->Initialize()` which loads GL function pointers via GLAD and logs GPU info
3. `app.Run()` enters the main loop: poll events → render frame (just a clear) → swap buffers
4. When the window closes, `Application::~Application()` calls `renderer->Shutdown()`, destroys the window, and terminates GLFW

**Framebuffer resize:** GLFW calls the static `framebuffer_size_callback`, which retrieves the `Application*` from the window's user pointer and forwards the new size to `Renderer::Resize()`, which calls `glViewport`.

**Debug context:** In debug builds (`#ifndef NDEBUG`), `GLFW_OPENGL_DEBUG_CONTEXT` is requested and a `glDebugMessageCallback` is registered. High-severity GL errors log as `spdlog::error`, medium as `spdlog::warn`. This catches many common GL mistakes at the call site.

---

## Definition of Done — Checklist

- [x] All required libraries compile under the documented build path
- [x] Window opens, clears to a dark background, resizes correctly, and closes cleanly
- [x] Startup log reports GPU vendor, renderer name, and GL version
- [x] GL debug context active in debug builds
- [x] `config/settings.json` drives window dimensions, title, and vsync
- [x] Git hooks enforce branch naming and commit format
- [x] Build and run instructions documented and tested

---

## What Is Missing / Not Done

Nothing critical is missing from Sprint 1's own scope. However, several items were deferred to Sprint 2:

- No `ShaderProgram` abstraction — `Renderer::RenderFrame()` only clears the screen
- No geometry drawn yet
- GLM, Assimp, Dear ImGui, stb_image not yet integrated into CMake
- `Options` only reads `window` config; `logging.level`, `paths.asset_root`, and `debug.gl_errors` fields exist in `settings.json` but are not yet consumed by any code
