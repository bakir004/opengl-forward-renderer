# Core Classes — renderer/src/core

Sprint 2 state. These classes form the GPU abstraction layer between raw OpenGL and application code.

---

## Pipeline Integration Overview

The classes compose into a strict ownership and call order:

```
Primitives
  └─ generates PrimitiveMeshData (CPU)
        └─ CreateMeshBuffer()
              └─ MeshBuffer  (owns VAO + VBO + optional EBO)
                    ├─ VertexArray   (wraps VAO)
                    ├─ Buffer        (wraps VBO)
                    ├─ Buffer        (wraps EBO, optional)
                    └─ VertexLayout  (configures attrib pointers at upload time)

ShaderProgram  (owns GL program, caches uniform locations)

Renderer
  └─ BeginFrame()  →  SubmitDraw(ShaderProgram, MeshBuffer)  →  EndFrame()
```

No class reaches past its neighbour. `MeshBuffer` does not know about shaders. `ShaderProgram` does not know about geometry. `Renderer` is the only class that touches both.

---

## Renderer

**File:** `Renderer.h / Renderer.cpp`

Top-level pipeline manager. The only class that `Application` holds a reference to for rendering. Owns all OpenGL state and caches it to avoid redundant GL calls.

### Supporting types defined in Renderer.h

| Type | Purpose |
|---|---|
| `DepthFunc` | Enum for depth comparison: `Less`, `LessEqual`, `Greater`, `GreaterEqual`, `Always`, `Never`, `Equal`, `NotEqual` |
| `BlendMode` | Enum for blend equation: `Disabled`, `Alpha`, `Additive`, `Multiply` |
| `CullMode` | Enum for face culling: `Disabled`, `Back`, `Front`, `FrontAndBack` |
| `Viewport` | Struct: `x, y, width, height`. Used for viewport tracking. |
| `ClearFlags` | Bitmask enum: `Color`, `Depth`, `Stencil`, `ColorDepth`, `All`. Supports `|` and `&`. |
| `FrameParams` | Struct passed to `BeginFrame`: `clearColor (vec4)`, `clearFlags (ClearFlags)` |

### Methods

| Method | What it does |
|---|---|
| `Initialize()` | Loads GL function pointers via GLAD. Sets up the debug message callback in debug builds. Must be called after the GL context is current. Returns false if GLAD fails. |
| `Shutdown()` | Cleanup before the GL context is destroyed. |
| `BeginFrame(FrameParams)` | Clears the framebuffer using the params' clear color and flags. Sets `m_inFrame = true`. |
| `EndFrame()` | Asserts the frame was properly opened. Sets `m_inFrame = false`. |
| `SubmitDraw(ShaderProgram, MeshBuffer)` | Binds the shader and mesh, calls `MeshBuffer::Draw()`, then unbinds both. This is the primary draw path. |
| `UnbindShader()` | Calls `ShaderProgram::Unbind()`. Used after a batch of draws sharing one shader. |
| `SetViewport(x, y, w, h)` / `SetViewport(Viewport)` | Sets the GL viewport. Cached — skips the GL call if state hasn't changed. |
| `SetClearColor(vec4)` | Sets the GL clear color. Cached. |
| `Clear(ClearFlags)` | Issues `glClear` for the specified buffer targets. |
| `Resize(width, height)` | Called by the GLFW framebuffer resize callback in `Application`. Updates the viewport to match the new framebuffer size. |
| `SetDepthTest(enable, DepthFunc)` | Enables/disables depth testing and sets the comparison function. Cached. |
| `SetBlendMode(BlendMode)` | Configures OpenGL blending. `Disabled` calls `glDisable(GL_BLEND)`. Others configure src/dst factors. Cached. |
| `SetCullMode(CullMode)` | Configures face culling. `Disabled` calls `glDisable(GL_CULL_FACE)`. Others call `glCullFace`. Cached. |
| `GetClearColor()` | Returns cached clear color without a GL round-trip. |
| `GetViewport()` | Returns cached viewport without a GL round-trip. |
| `RenderFrame()` | **Deprecated.** Compatibility stub for old call sites. Use `BeginFrame`/`EndFrame` instead. |

### State caching

All pipeline state (`m_depthTestEnabled`, `m_depthFunc`, `m_blendMode`, `m_cullMode`, `m_clearColor`, `m_viewport`) is cached in member variables. Setters check the cached value before issuing a GL call, so calling `SetDepthTest(true, Less)` every frame incurs no GL overhead after the first call.

---

## Buffer

**File:** `Buffer.h / Buffer.cpp`

RAII wrapper for a single OpenGL buffer object — either a VBO (`GL_ARRAY_BUFFER`) or an EBO (`GL_ELEMENT_ARRAY_BUFFER`). Not used directly by application code; `MeshBuffer` owns and manages `Buffer` instances internally.

### Methods

| Method | What it does |
|---|---|
| `Buffer(Type, data, size, usage)` | Calls `glGenBuffers`, binds, and uploads data via `glBufferData`. Type is `VERTEX` or `ELEMENT`. |
| `~Buffer()` | Calls `glDeleteBuffers`. Safe to call on a moved-from instance (`m_id == 0`). |
| `Bind()` | Binds to `GL_ARRAY_BUFFER` or `GL_ELEMENT_ARRAY_BUFFER` depending on type. |
| `Unbind()` | Unbinds the target. **Do not call on an EBO while a VAO is bound** — unbinding `GL_ELEMENT_ARRAY_BUFFER` modifies the active VAO's EBO slot. |
| `UpdateData(data, size, offset)` | Partial or full re-upload via `glBufferSubData`. Useful for dynamic geometry. |

Move-only. Copy constructor and assignment are deleted.

---

## VertexArray

**File:** `VertexArray.h / VertexArray.cpp`

RAII wrapper for a VAO (`glGenVertexArrays` / `glDeleteVertexArrays`). Records the vertex attribute configuration and EBO binding set up while it is bound. Not used directly by application code — owned by `MeshBuffer`.

### Methods

| Method | What it does |
|---|---|
| `VertexArray()` | Calls `glGenVertexArrays`. |
| `~VertexArray()` | Calls `glDeleteVertexArrays`. |
| `Bind()` | Makes this VAO active for subsequent attribute setup or draw calls. |
| `Unbind()` | Binds VAO 0 (no active VAO). |

Move-only. All vertex attribute state and the EBO binding are stored inside the VAO on the GPU — nothing is tracked CPU-side beyond the `m_id`.

---

## VertexLayout

**File:** `VertexLayout.h / VertexLayout.cpp`

Describes the memory layout of one vertex. Computes stride and per-attribute byte offsets automatically from a list of `VertexAttribute` descriptors, then applies them to the currently bound VAO + VBO via `Apply()`.

### Supporting type

`VertexAttribute` — `{ index, count, type, normalized }` — maps to one `glVertexAttribPointer` call.

### Methods

| Method | What it does |
|---|---|
| `VertexLayout(vector<VertexAttribute>)` | Stores the attribute list and computes `m_stride` by summing all attribute byte sizes. |
| `Apply()` | Iterates attributes, calling `glVertexAttribPointer` and `glEnableVertexAttribArray` for each. Must be called while the correct VAO and VBO are bound. |
| `GetStride()` | Returns the total byte size of one vertex. |

### Usage order

```cpp
vao.Bind();
vbo.Bind();
layout.Apply();   // records attrib pointers into the bound VAO
// do NOT unbind the EBO here if one exists
```

---

## MeshBuffer

**File:** `MeshBuffer.h / MeshBuffer.cpp`

The primary geometry abstraction. Owns a `VertexArray`, a vertex `Buffer`, and an optional index `Buffer`. Constructed from raw CPU data — uploads everything to the GPU during construction and configures the VAO. After construction, the CPU data is no longer needed.

Supports both rendering paths:
- **Non-indexed:** `glDrawArrays(GL_TRIANGLES, 0, vertexCount)`
- **Indexed:** `glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0)`

### Methods

| Method | What it does |
|---|---|
| `MeshBuffer(vertexData, vertexBufferSize, vertexCount, indexData, indexCount, layout, usage)` | Generates VAO + VBO (+ EBO if `indexData != nullptr`). Binds VAO, uploads vertex data, applies layout, uploads index data. Logs success via spdlog. |
| `Bind()` | Binds the VAO. |
| `Unbind()` | Unbinds the VAO. |
| `Draw()` | Issues `glDrawElements` or `glDrawArrays` depending on whether an EBO is present. |
| `GetVertexCount()` | Returns number of vertices. |
| `GetIndexCount()` | Returns number of indices (0 for non-indexed). |
| `IsIndexed()` | Returns true if an EBO was created. |

Move-only. `Renderer::SubmitDraw` calls `Bind()`, `Draw()`, `Unbind()` in sequence.

---

## ShaderProgram

**File:** `ShaderProgram.h / ShaderProgram.cpp`

Manages the full lifecycle of a GLSL shader program: reads source files from disk, compiles each stage, links the program, and caches uniform locations for efficient per-frame updates.

If compilation or linking fails, the program falls back to a hardcoded magenta shader so failures are visually obvious rather than silent.

### Private helpers

| Method | What it does |
|---|---|
| `ReadFile(path)` | Reads a `.vert` or `.frag` file into a string. Returns `nullopt` on failure. |
| `CompileStage(source, stageType, path)` | Calls `glCreateShader`, `glShaderSource`, `glCompileShader`. Logs errors with stage and file name. Returns 0 on failure. |
| `LinkProgram(vertShader, fragShader, ...)` | Calls `glCreateProgram`, attaches stages, links. Logs errors. Deletes stage objects after linking. |
| `GetUniformLocation(name)` | Looks up `m_uniformCache` first; calls `glGetUniformLocation` on miss and stores the result. |

### Public methods

| Method | What it does |
|---|---|
| `ShaderProgram(vertexPath, fragmentPath)` | Full load-compile-link pipeline. Falls back to magenta on any failure. |
| `~ShaderProgram()` | Calls `glDeleteProgram`. |
| `IsValid()` | Returns `m_id != 0`. Check this after construction before rendering. |
| `GetID()` | Returns the raw GL program handle. |
| `Bind()` | Calls `glUseProgram(m_id)`. |
| `Unbind()` (static) | Calls `glUseProgram(0)`. |
| `SetUniform(name, value)` | Typed overloads for `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`, `mat3`, `mat4`. Resolves location from cache then calls the appropriate `glUniform*` function. |

Move-only.

---

## Primitives

**File:** `Primitives.h / Primitives.cpp`

CPU-side geometry factory. Generates test geometry as `PrimitiveMeshData` (a `vector<VertexPC>` + `vector<uint32_t>`). Call `CreateMeshBuffer()` on the result to upload to the GPU.

### Supporting types

`VertexPC` — `{ glm::vec3 position, glm::vec3 color }` — matches `basic.vert` locations 0 and 1. Statically asserted to be standard-layout and 24 bytes.

`PrimitiveMeshData` — CPU mesh container with helper methods:

| Method | What it does |
|---|---|
| `GetVertexBufferSize()` | Returns `vertices.size() * sizeof(VertexPC)` as `GLsizeiptr`. |
| `GetVertexCount()` | Returns vertex count as `GLsizei`. |
| `GetIndexCount()` | Returns index count as `GLsizei`. |
| `IsIndexed()` | Returns `!indices.empty()`. |
| `CreateLayout()` (static) | Returns the `VertexLayout` matching `VertexPC` (position @ 0, color @ 1). |
| `CreateMeshBuffer(usage)` | Constructs and returns a `MeshBuffer` from this CPU data. One-shot upload. |

### Generator functions

| Function | Vertices | Indices | Notes |
|---|---|---|---|
| `GenerateTriangle()` | 3 | none | Non-indexed path. RGB corners. |
| `GenerateQuad()` | 4 | 6 | Two triangles. |
| `GenerateCube()` | 24 | 36 | 4 unique vertices per face (not shared) to allow per-face colors. |
