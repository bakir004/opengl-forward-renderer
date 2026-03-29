# Sprint 2 — Minimal Renderer Core, Shaders, and First Geometry

**Status:** Complete
**Objective:** Replace the bootstrap-only renderer with a reusable minimal renderer capable of drawing indexed geometry through managed shader, buffer, vertex layout, and primitive abstractions.

---

## What Was Done

### Files Added
| File | Purpose |
|---|---|
| `src/core/Buffer.h/.cpp` | RAII wrapper for a single OpenGL buffer object (VBO or EBO) |
| `src/core/VertexArray.h/.cpp` | RAII wrapper for an OpenGL Vertex Array Object (VAO) |
| `src/core/VertexLayout.h/.cpp` | Describes vertex memory layout; automates `glVertexAttribPointer` setup |
| `src/core/MeshBuffer.h/.cpp` | GPU mesh abstraction: owns VAO + VBO + optional EBO; supports indexed and non-indexed rendering |
| `src/core/ShaderProgram.h/.cpp` | Loads, compiles, links GLSL stages; caches uniform locations; exposes typed `SetUniform` overloads |
| `src/core/Primitives.h/.cpp` | CPU-side primitive generators (triangle, quad, cube) returning `PrimitiveMeshData` → `MeshBuffer` |
| `src/app/SampleScene.h/.cpp` | Sprint 2 demo scene: loads `basic.vert/frag`, uploads three primitives, issues draws via `Renderer::SubmitDraw` |
| `shaders/basic.vert` | Pass-through vertex shader (clip-space positions, per-vertex colour); no MVP yet |
| `shaders/basic.frag` | Outputs interpolated `v_Color`; no lighting |

### Files Modified
| File | Change |
|---|---|
| `CMakeLists.txt` | Added all new `.cpp` sources; integrated GLM via FetchContent |
| `src/core/Renderer.h/.cpp` | Added `BeginFrame`/`EndFrame`, `SubmitDraw`, `UnbindShader`, `ClearFlags` bitmask, `FrameParams`; deprecated `RenderFrame()` |
| `src/core/Renderer.h/.cpp` | Added pipeline state management: `SetDepthTest`, `SetBlendMode`, `SetCullMode` with enum-based API and state caching |
| `src/utils/Options.h/.cpp` | Added doc comments; error message improved |
| `src/app/Application.h/.cpp` | Owns `SampleScene`; calls `scene.Setup(...)` in `Initialize` and `scene.Render(...)` in the main loop |

---

## How the Renderer Works — Detailed Explanation

This section is written for someone unfamiliar with OpenGL internals.

### The Core Problem OpenGL Solves

Your CPU holds mesh data (vertices and indices) as plain arrays. The GPU needs that data in its own fast memory (VRAM) to draw with it. OpenGL gives you three objects to manage this:

- **VBO (Vertex Buffer Object)** — a chunk of GPU memory that holds raw vertex data (positions, colors, UVs, etc.)
- **EBO (Element Buffer Object)** — a chunk of GPU memory that holds indices (which vertices to connect into triangles)
- **VAO (Vertex Array Object)** — records *how* to interpret a VBO (which byte offset is position, which is color, etc.) so you don't have to re-specify this every frame

### Why This Matters

Without these abstractions, every draw call would require you to:
1. Manually bind the right buffers
2. Re-specify the vertex attribute layout every time
3. Remember to clean up GL objects at the right time (memory leak if you forget)

The Sprint 2 code wraps all of this so you never deal with raw `glGenBuffers` / `glVertexAttribPointer` calls in higher-level code.

---

### `Buffer` — `src/core/Buffer.h/.cpp`

A `Buffer` is a C++ RAII wrapper for one OpenGL buffer object. RAII means the resource is tied to the object's lifetime — created in the constructor, destroyed in the destructor, automatically.

**Internals:**
- `glGenBuffers(1, &m_id)` — asks the GPU to create a buffer and give us back an integer ID (a handle)
- `glBufferData(target, size, data, usage)` — uploads data from CPU RAM to that GPU buffer
- `glDeleteBuffers(1, &m_id)` — frees the GPU memory when the `Buffer` object goes out of scope

**The `Type` enum:**
- `VERTEX` → binds to `GL_ARRAY_BUFFER` (for vertex data)
- `ELEMENT` → binds to `GL_ELEMENT_ARRAY_BUFFER` (for index data)

**Critical gotcha — EBO unbind rule:**
```cpp
// In Buffer constructor:
if (m_type == VERTEX) Unbind();
// The EBO is NOT unbound here, intentionally.
```
In OpenGL, the EBO binding is stored *inside* the currently active VAO. If you unbind the EBO (call `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)`) while a VAO is bound, OpenGL records "this VAO has no EBO" — silently breaking indexed drawing. This is one of the most common beginner OpenGL bugs. The code avoids it by never unbinding EBOs while a VAO is active.

**Move semantics:**
```cpp
Buffer(Buffer&& other) noexcept : m_id(other.m_id), m_type(other.m_type) {
    other.m_id = 0;  // 0 = "I don't own anything anymore"
}
```
Copy is deleted. Move transfers ownership by stealing the ID and setting the source to 0. When `glDeleteBuffers(0)` is called, OpenGL ignores it — so a moved-from buffer safely destructs.

---

### `VertexArray` — `src/core/VertexArray.h/.cpp`

Same RAII pattern as `Buffer`, but for a VAO.

**What a VAO actually stores:**
When you call `glVertexAttribPointer(...)` while a VAO is bound, OpenGL records that configuration into the VAO. From that point on, you just `glBindVertexArray(vao_id)` before a draw call and OpenGL knows exactly how to read vertex data from the associated VBO — no need to re-specify attribute layout every frame.

A VAO stores:
- Which attribute slots are enabled
- For each enabled slot: which VBO, what offset, what stride, what data type
- Which EBO is attached (this is why you can't unbind the EBO while a VAO is bound)

---

### `VertexLayout` — `src/core/VertexLayout.h/.cpp`

This is the piece that bridges the gap between "I have vertex data" and "OpenGL knows how to read it."

**The problem it solves:**

Suppose each vertex is:
```
[X, Y, Z, R, G, B]   — 6 floats = 24 bytes per vertex
```

OpenGL needs to know:
- Attribute 0 (position): 3 floats, starting at byte 0, every 24 bytes
- Attribute 1 (color): 3 floats, starting at byte 12, every 24 bytes

Calculating these offsets manually for every mesh is tedious and error-prone. `VertexLayout` does it automatically.

**How it works:**
```cpp
VertexLayout layout({
    {0, 3, GL_FLOAT, GL_FALSE},  // location=0, 3 floats, not normalized → position
    {1, 3, GL_FLOAT, GL_FALSE},  // location=1, 3 floats, not normalized → color
});
```

Constructor walks the attribute list and sums up byte sizes to compute the total `stride` (24 bytes in this example).

`Apply()` walks the list again, calling for each attribute:
```cpp
glVertexAttribPointer(
    attrib.index,       // which shader location (matches layout(location=N) in GLSL)
    attrib.count,       // how many components (3 for vec3)
    attrib.type,        // GL_FLOAT
    attrib.normalized,  // GL_FALSE
    m_stride,           // how many bytes between the start of consecutive vertices (24)
    offset              // where in one vertex does this attribute start (0 for position, 12 for color)
);
glEnableVertexAttribArray(attrib.index);
offset += attrib.ByteSize();  // advance offset for next attribute
```

The `layout(location = N)` in GLSL shaders must match the `index` field here. That's how GLSL knows which buffer data maps to which shader variable.

---

### `MeshBuffer` — `src/core/MeshBuffer.h/.cpp`

`MeshBuffer` is the top-level GPU mesh object. It owns and manages all three GL objects (VAO + VBO + EBO) together and provides a simple `Bind() / Draw()` interface.

**Constructor flow:**
```
MeshBuffer(vertices, vertexBytes, indices, indexCount, layout)
  1. Construct m_vbo → glGenBuffers + glBufferData (uploads vertex data)
  2. Construct m_ebo → glGenBuffers + glBufferData (uploads index data)
  3. m_vao.Bind()   → all subsequent state goes into this VAO
  4. m_vbo.Bind()   → VAO records this VBO
  5. m_ebo.Bind()   → VAO records this EBO (never unbound while VAO active)
  6. layout.Apply() → VAO records glVertexAttribPointer for each attribute
  7. m_vao.Unbind() → VAO is fully configured, deactivate it
```

After construction, everything OpenGL needs to draw this mesh is baked into `m_vao`. To draw:

```cpp
meshBuffer.Bind();  // binds VAO — OpenGL now knows VBO layout + EBO
// (shader must be bound by caller, not MeshBuffer's job)
meshBuffer.Draw();  // glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr)
```

`glDrawElements` uses the indices in the EBO to look up vertices in the VBO and assemble triangles. The `nullptr` at the end means "start at the beginning of the EBO."

**What `MeshBuffer` intentionally does NOT do:**
- It does not know about shaders (which program to use)
- It does not know about materials (textures, colors)
- It does not know about transforms (where to place the mesh in the world)
- It does not call any draw calls from its constructor

This separation of concerns means you can reuse the same `MeshBuffer` with different shaders, or have a shader system that's completely independent.

---

### Per-Frame Draw Flow (Sprint 2 final)

`RenderFrame()` is now deprecated. The new pattern, wired up in `Application::Run()`:

```cpp
renderer.BeginFrame();               // glClear with FrameParams defaults
scene.Render(renderer, elapsed);     // calls SubmitDraw for each primitive
renderer.EndFrame();                 // asserts m_inFrame, resets flag
```

Inside `SampleScene::Render`:
```cpp
renderer.SetDepthTest(true, DepthFunc::Less);
renderer.SetCullMode(CullMode::Back);
renderer.SubmitDraw(*m_shader, *m_triangle);  // non-indexed: glDrawArrays
renderer.SubmitDraw(*m_shader, *m_quad);      // indexed: glDrawElements
renderer.SubmitDraw(*m_shader, *m_cube);      // indexed: glDrawElements
renderer.UnbindShader();
```

`SubmitDraw` binds the shader and mesh, calls `Draw()`, then unbinds the mesh. Geometry is drawn in clip space — no MVP uniforms yet (Sprint 3).

---

## Pipeline State Management — State API for `Renderer`

Sprint 2 added an abstraction layer for OpenGL pipeline state. Instead of scattering raw `glEnable()` and `glBlendFunc()` calls throughout the codebase, the `Renderer` exposes type-safe methods.

### Why This Matters

**Bad — raw GL calls in application code:**
```cpp
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LESS);
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

**Good — clean renderer API:**
```cpp
renderer->SetDepthTest(true, DepthFunc::Less);
renderer->SetBlendMode(BlendMode::Alpha);
```

Benefits:
- **Type safety** — no raw `GLenum` values, IDE autocomplete works
- **State caching** — redundant GL calls are skipped automatically
- **Centralized control** — all pipeline state changes go through one place
- **Easier debugging** — breakpoint `SetBlendMode` to see who's changing blend state

### Depth Test — `SetDepthTest(bool enable, DepthFunc func)`

Controls whether fragments are discarded based on depth buffer comparison.

Supported depth functions:
- `DepthFunc::Less` — standard 3D rendering (closer objects win)
- `DepthFunc::LessEqual` — allows equal-depth fragments
- `DepthFunc::Greater` / `GreaterEqual` — reverse-Z depth (advanced optimization)
- `DepthFunc::Always` / `Never` / `Equal` / `NotEqual` — special cases

Usage example:
```cpp
renderer->SetDepthTest(true, DepthFunc::Less);  // Enable depth testing
// ... draw opaque geometry ...
```

Default: Enabled with `DepthFunc::Less` (set in `Renderer::Initialize()`)
 
---

### Blend Mode — `SetBlendMode(BlendMode mode)`

Controls how fragment colors are combined with the framebuffer.

Supported modes:
- `BlendMode::Disabled` — no blending, fragment overwrites framebuffer
- `BlendMode::Alpha` — standard transparency: `src.rgb * src.a + dst.rgb * (1 - src.a)`
- `BlendMode::Additive` — light/particle effects: `src + dst` (accumulates brightness)
- `BlendMode::Multiply` — darken: `src * dst` (used for shadows, decals)

Usage example:
```cpp
renderer->SetBlendMode(BlendMode::Disabled);
// ... draw opaque geometry ...
renderer->SetBlendMode(BlendMode::Alpha);
// ... draw transparent geometry (sorted back-to-front) ...
```

Default: `Disabled`
 
---

### Cull Mode — `SetCullMode(CullMode mode)`

Controls which triangle faces are culled (not rendered) based on winding order.

Supported modes:
- `CullMode::Back` — cull back-facing triangles (default for solid objects)
- `CullMode::Front` — cull front-facing triangles (useful for inside-out geometry)
- `CullMode::Disabled` — render both sides (double-sided geometry, grass, cloth)
- `CullMode::FrontAndBack` — cull everything (rarely used; depth-only passes)

Front face winding: Currently hardcoded to counter-clockwise (`GL_CCW`). This matches the standard OpenGL convention and most model exporters.

Usage example:
```cpp
renderer->SetCullMode(CullMode::Back);    // Normal 3D objects
// ... draw scene ...
renderer->SetCullMode(CullMode::Disabled); // Double-sided foliage
// ... draw grass/leaves ...
```

Default: `CullMode::Back`
 
---

### Implementation Details

**State caching:** Each `Set*` function checks if the requested state matches the cached state before calling OpenGL. This avoids redundant driver calls.
```cpp
if (m_blendMode == mode) return;  // Skip if already set
```

**Initialization:** Defaults are set in `Renderer::Initialize()` immediately after GLAD loads:
```cpp
SetDepthTest(true, DepthFunc::Less);
SetBlendMode(BlendMode::Disabled);
SetCullMode(CullMode::Back);
```
This ensures the renderer starts in a known, predictable state.

**Future extensions:**
- Depth write masking (`glDepthMask`)
- Polygon mode (`glPolygonMode` for wireframe debug)
- Scissor test (UI clipping)
- Stencil test (advanced masking)

---

## Definition of Done — Checklist

- [x] `Buffer` RAII wrapper implemented with correct EBO unbind behavior
- [x] `VertexArray` RAII wrapper implemented
- [x] `VertexLayout` system implemented with automatic stride/offset calculation
- [x] `MeshBuffer` abstraction implemented with correct VAO setup order; supports indexed and non-indexed paths
- [x] All classes have doc comments on public methods
- [x] `ShaderProgram` abstraction with source loading, compilation, and link error reporting
- [x] Built-in primitive generation (triangle, quad, cube) using `MeshBuffer`
- [x] `Renderer::SubmitDraw` wired to real geometry via `SampleScene`
- [x] Indexed rendering path verified with quad and cube primitives
- [x] Application code depends only on public renderer headers, not GL directly
- [x] Fallback/error shader strategy for development visibility

---

## New Classes Added in Feature/Primitive-Generation

### `ShaderProgram` — `src/core/ShaderProgram.h/.cpp`

Manages the full lifecycle of a GLSL shader program.

**Load → compile → link flow:**
```
ShaderProgram(vertPath, fragPath)
  1. ReadFile(vertPath)  → source string
  2. CompileStage(source, GL_VERTEX_SHADER, path)   → GLuint
  3. ReadFile(fragPath)  → source string
  4. CompileStage(source, GL_FRAGMENT_SHADER, path) → GLuint
  5. LinkProgram(vert, frag, ...)                   → m_id
```
`CompileStage` calls `glGetShaderInfoLog` and forwards errors to `spdlog::error` with the source path included. `IsValid()` returns `m_id != 0`. `SampleScene::Setup` checks `IsValid()` before proceeding.

**Uniform helpers:**
`SetUniform(name, value)` is overloaded for `float`, `int`, `bool`, `vec2`–`vec4`, `mat3`, `mat4`. Locations are cached in `m_uniformCache` to avoid repeated `glGetUniformLocation` calls per frame.

---

### `Primitives` — `src/core/Primitives.h/.cpp`

Three factory functions that return `PrimitiveMeshData` (CPU-side `std::vector<VertexPC>` + `std::vector<uint32_t>`):

| Function | Render path | Notes |
|---|---|---|
| `GenerateTriangle()` | Non-indexed (`glDrawArrays`) | 3 vertices, per-vertex RGB |
| `GenerateQuad()` | Indexed (`glDrawElements`) | 4 vertices, 6 indices (2 triangles) |
| `GenerateCube()` | Indexed (`glDrawElements`) | 24 vertices (unique per face), 36 indices |

`PrimitiveMeshData::CreateMeshBuffer()` constructs and returns a ready-to-draw `MeshBuffer`.

`VertexPC` is a `standard_layout` struct of two `glm::vec3`s (position + color), matching `basic.vert` locations 0 and 1.

---

### `SampleScene` — `src/app/SampleScene.h/.cpp`

Owns the three `MeshBuffer` instances and the `ShaderProgram`. `Setup(vertPath, fragPath)` loads the shader and uploads geometry. `Render(renderer, time)` issues three `SubmitDraw` calls each frame. Vertex positions are baked in clip space so the scene is visible without a camera.

---

## What Is Still Pending (carry to Sprint 3)

### 1. `Options` fields that exist in JSON but aren't consumed

`config/settings.json` has `logging.level`, `paths.asset_root`, and `debug.gl_errors` fields, but `Options` only reads `window`. These should be plumbed through when their systems exist.

### 2. MVP uniforms (by design — deferred to Sprint 3)

`basic.vert` draws in clip space with no transforms. Model, view, and projection matrices are Sprint 3 work (camera + transform system).
