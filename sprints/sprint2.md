# Sprint 2 — Minimal Renderer Core, Shaders, and First Geometry

**Status:** Partially complete — GPU buffer layer done, shader system and primitive geometry not yet implemented
**Objective:** Replace the bootstrap-only renderer with a reusable minimal renderer capable of drawing indexed geometry through managed shader, buffer, vertex layout, and primitive abstractions.

---

## What Was Done

### Files Added
| File | Purpose |
|---|---|
| `src/core/Buffer.h/.cpp` | RAII wrapper for a single OpenGL buffer object (VBO or EBO) |
| `src/core/VertexArray.h/.cpp` | RAII wrapper for an OpenGL Vertex Array Object (VAO) |
| `src/core/VertexLayout.h/.cpp` | Describes vertex memory layout; automates `glVertexAttribPointer` setup |
| `src/core/MeshBuffer.h/.cpp` | GPU mesh abstraction: owns VAO + VBO + EBO, exposes `Bind/Draw` |

### Files Modified
| File | Change |
|---|---|
| `CMakeLists.txt` | Added `Buffer.cpp`, `VertexArray.cpp`, `VertexLayout.cpp`, `MeshBuffer.cpp` to the build |
| `src/core/Renderer.h/.cpp` | Added doc comments; `RenderFrame` still only clears (not yet wired to geometry) |
| `src/utils/Options.h/.cpp` | Added doc comments; error message improved |

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

### Current State of `Renderer::RenderFrame()`

```cpp
void Renderer::RenderFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
```

This only clears the screen to a dark gray. **`MeshBuffer` is not yet used here.** The buffer infrastructure exists but is not yet wired into actual draw calls. There is also no shader system yet, so nothing can actually be drawn even if a `MeshBuffer` were created.

---

## Definition of Done — Checklist

- [x] `Buffer` RAII wrapper implemented with correct EBO unbind behavior
- [x] `VertexArray` RAII wrapper implemented
- [x] `VertexLayout` system implemented with automatic stride/offset calculation
- [x] `MeshBuffer` abstraction implemented with correct VAO setup order
- [x] All classes have doc comments on public methods
- [ ] `ShaderProgram` abstraction with source loading, compilation, and link error reporting
- [ ] Fallback/error shader strategy for development visibility
- [ ] Built-in primitive generation (triangle, quad, cube) using `MeshBuffer`
- [ ] `Renderer::RenderFrame()` actually draws geometry (not just clears)
- [ ] Indexed rendering path verified with at least one non-trivial primitive
- [ ] Application code depends only on public renderer headers, not GL directly

---

## What Is Missing

### 1. `ShaderProgram` — the biggest gap

There is no shader system at all. The SRS requires (FR-004) a `ShaderProgram` abstraction that:
- Loads `.glsl` source files from disk
- Compiles vertex and fragment stages separately
- Links them into a program
- Reports compile/link errors with the stage name and source file
- Allows reuse by identifier

Without this, `MeshBuffer` is useless — you can't draw anything without a shader telling the GPU what to do with the vertex data.

There are also **no `.glsl` shader files in the repository yet.** The SRS planned a `shaders/` directory; it doesn't exist.

### 2. Built-in Primitive Geometry

The SRS (FR-006) requires built-in triangle, quad, and cube primitives for testing. No primitive generation code exists. These would be functions/classes that return pre-built `MeshBuffer` instances with hard-coded vertex/index arrays.

### 3. `Renderer::RenderFrame()` draws nothing

The render frame only clears. `MeshBuffer` is not instantiated or called anywhere in the renderer. This needs shader + primitive work first.

### 4. `Options` fields that exist in JSON but aren't consumed

`config/settings.json` has `logging.level`, `paths.asset_root`, and `debug.gl_errors` fields, but `Options` only reads `window`. These should be plumbed through when their systems exist.

### 5. GLM not integrated

No math library is set up yet. The moment a camera or transforms are needed (Sprint 3), GLM needs to be added to `CMakeLists.txt`.
