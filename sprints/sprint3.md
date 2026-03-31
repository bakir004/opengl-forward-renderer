# Sprint 3 — Camera, Transforms, Scene Submission, and Debug Controls

**Status:** In Progress
**Objective:** Turn the renderer from "it can draw primitives" into "it can render an actual 3D scene." Introduce a perspective camera, per-object transforms, a scene submission flow, per-frame GPU constants via UBOs, and a lightweight ImGui debug overlay. After this sprint, the sample app never issues direct draw calls — it only describes what should be rendered.

---

## Feature Scope

- Perspective camera with free-fly controls
- Transform system (translation, rotation, scale → model matrix)
- Scene submission data types: `RenderItem`, `FrameSubmission`, `CameraData`
- Per-frame uniform blocks (view, projection, camera position, per-object model matrix)
- Refined renderer frame API consuming `FrameSubmission`
- ImGui debug overlay (frame stats, camera info, wireframe toggle)
- 3D test scene built entirely through the submission interface

---

## CPU → GPU Data Flow

This section describes exactly what data moves from CPU to GPU each frame, at what granularity, and through which OpenGL mechanism. Understanding this is essential before implementing the UBO and draw loop.

### Overview

There are three distinct moments when CPU data reaches the GPU. They happen at completely different points in the program's lifetime:

```
STARTUP (once, when MeshBuffer is constructed — Sprint 2)
─────────────────────────────────────────────────────────
CPU: std::vector<VertexPC>          glBufferData → VBO (VRAM)
     std::vector<uint32_t>          glBufferData → EBO (VRAM)
     VertexLayout::Apply()          glVertexAttribPointer → VAO records layout
                                    VAO now knows: "read position from VBO offset 0,
                                                    read color from VBO offset 12"
     ShaderProgram constructor      glCompileShader + glLinkProgram → shader lives on GPU

     After this point the mesh data never moves again.
     The VAO, VBO, EBO all live in VRAM until ~MeshBuffer() calls glDelete*.

PER FRAME (once per frame, at BeginFrame — Sprint 3 new)
─────────────────────────────────────────────────────────
CPU: CameraData { view, proj,       glBufferSubData → UBO binding 0 (VRAM)
     viewProj, pos, time }          All shaders see these values for every draw
                                    call this frame without any further uploads.

PER OBJECT (once per RenderItem in the draw loop — Sprint 3 new)
─────────────────────────────────────────────────────────────────
CPU: Transform::GetModelMatrix()    glUniformMatrix4fv → u_Model (GPU register)
                                    Overwritten for each object before its draw call.

GPU OUTPUT (written by shaders, never read back by CPU)
───────────────────────────────────────────────────────
Fragment shader → colour buffer     presented to screen by glfwSwapBuffers
Depth test      → depth buffer      consumed internally by OpenGL for occlusion,
                                    discarded at frame end, never sent to CPU
```

---

### Startup: Mesh and Shader Upload (Sprint 2 — VBO / EBO / VAO)

This happens inside `MeshBuffer`'s constructor and `ShaderProgram`'s constructor. It runs once per object when the scene is set up, not during the render loop.

**Vertex data (VBO):**
`glGenBuffers` allocates a buffer object in VRAM. `glBufferData(GL_ARRAY_BUFFER, size, vertices.data(), GL_STATIC_DRAW)` copies the raw `VertexPC` array from CPU RAM to GPU VRAM. After this call the CPU-side `std::vector` is no longer needed by the GPU — it can be freed and the mesh still draws.

`GL_STATIC_DRAW` tells the driver this data will be written once and read many times, which lets it place the buffer in the fastest VRAM region.

**Index data (EBO):**
Same pattern, bound to `GL_ELEMENT_ARRAY_BUFFER`. Contains `uint32_t` indices. The GPU uses this to look up which vertices to connect into triangles rather than reading them sequentially. A cube needs 36 indices but only 24 vertices — without an EBO you'd need 36 vertices with duplicates.

**Layout description (VAO):**
`glGenVertexArrays` creates a VAO — a small GPU-side object that records *how* to interpret the VBO. `VertexLayout::Apply()` calls `glVertexAttribPointer` for each attribute:
- location 0: read 3 floats starting at byte 0, stride 24 → `a_Position`
- location 1: read 3 floats starting at byte 12, stride 24 → `a_Color`

After `Apply()`, the VAO is fully configured. At draw time, `glBindVertexArray` is the only call needed — the driver already knows everything about the vertex format.

**Critical ordering rule:** The EBO is recorded as VAO state. If you unbind the EBO (`glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)`) while a VAO is active, the VAO records "no index buffer" and indexed drawing silently breaks. `Buffer` never unbinds the EBO while a VAO is bound.

**Shader compilation:**
`glCompileShader` runs the GLSL compiler on the GPU driver. The compiled binary lives on the GPU. `glLinkProgram` connects the vertex and fragment stages. After linking, the CPU-side GLSL source strings are no longer needed.

| CPU source | GL call | GPU destination | Lifetime |
|---|---|---|---|
| `std::vector<VertexPC>` | `glBufferData` | VBO in VRAM | Until `~MeshBuffer` |
| `std::vector<uint32_t>` | `glBufferData` | EBO in VRAM | Until `~MeshBuffer` |
| `VertexLayout` attribute list | `glVertexAttribPointer` | VAO state | Until `~VertexArray` |
| GLSL source strings | `glCompileShader` / `glLinkProgram` | Shader program on GPU | Until `~ShaderProgram` |

---

### Per-Frame Data (UBO — Sprint 3)

**What:** Camera view matrix, projection matrix, their product, camera world position, elapsed time.

**How:** One `glBufferSubData` call at the start of each frame writes the entire `FrameUniforms` struct into the UBO. All shaders bound during that frame automatically read from binding point 0 — no per-draw upload needed.

**Why UBO instead of `SetUniform`:** `SetUniform` is one GL call per variable per draw call. With 8 objects and 5 frame-constant uniforms each, that's 40 calls per frame for data that doesn't change. A UBO is 1 upload regardless of object count and stays cheap as the scene grows.

| CPU field | GLSL name | Type | When |
|---|---|---|---|
| `CameraData::view` | `u_View` | `mat4` | Once per frame |
| `CameraData::projection` | `u_Projection` | `mat4` | Once per frame (also on resize) |
| `CameraData::viewProjection` | `u_ViewProjection` | `mat4` | Once per frame |
| `CameraData::position` | `u_CameraPos` | `vec3` | Once per frame |
| elapsed time | `u_Time` | `float` | Once per frame |

---

### Per-Object Data (uniform — Sprint 3)

**What:** The model matrix — where in the world this object sits, how it's rotated and scaled.

**How:** `SetUniform("u_Model", item.transform.GetModelMatrix())` inside the render item loop, immediately before `Draw()`. One `glUniformMatrix4fv` call per object. The GPU register holding `u_Model` is overwritten for each successive object.

**Why not UBO for this too:** At 8–20 objects it doesn't matter. A per-object UBO (or SSBO for instancing) is Sprint 4–5 territory. `SetUniform` keeps the draw loop readable without premature abstraction.

| CPU field | GLSL name | Type | When |
|---|---|---|---|
| `Transform::GetModelMatrix()` | `u_Model` | `mat4` | Once per object per frame |

---

### GPU → CPU (Readback)

Sprint 3 has no GPU→CPU readback. The GPU writes to the colour buffer and depth buffer. GLFW presents the colour buffer to the screen. The depth buffer is consumed internally and discarded. No `glReadPixels`, no transform feedback, no sync points.

ImGui reads CPU-side state (`deltaTime`, `camera.position`, `submission.items.size()`) — none of it comes from the GPU.

---

### Full Frame Timeline

```
APPLICATION STARTUP
│
├─ MeshBuffer constructor (per mesh, once)
│   ├─ glGenBuffers + glBufferData (vertices) ──────────────→ VBO in VRAM
│   ├─ glGenBuffers + glBufferData (indices)  ──────────────→ EBO in VRAM
│   ├─ glGenVertexArrays                      ──────────────→ VAO created
│   ├─ glBindVertexArray + glBindBuffer       ── VAO records VBO + EBO association
│   └─ VertexLayout::Apply (glVertexAttribPointer) ─────────→ VAO records attribute layout
│
└─ ShaderProgram constructor (per shader, once)
    ├─ glCompileShader (vert + frag) ──────────────────────→ compiled stages on GPU
    └─ glLinkProgram ──────────────────────────────────────→ linked program on GPU

─────────────────────────────────────────────── (main loop begins)

Application::Run() tick N
│
├─ glfwPollEvents
│   └─ input handler mutates Camera (WASD velocity, mouse yaw/pitch)
│
├─ Camera::Update(deltaTime)
│   └─ recompute position, view matrix, viewProjection (CPU only)
│
├─ Build FrameSubmission (CPU only — no GL calls)
│   ├─ snapshot CameraData from Camera
│   └─ build RenderItem list (mesh*, shader*, Transform per object)
│
├─ Renderer::BeginFrame(submission)
│   ├─ glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) ─→ clears GPU framebuffer
│   └─ glBufferSubData → FrameUniforms UBO ─────────────────→ view/proj/pos/time → VRAM
│                                                              (one upload, covers all draws)
│
├─ For each RenderItem in submission.items:
│   ├─ [skip if !item.visible]
│   ├─ glUseProgram(item.shader->GetID()) ──────────────────→ GPU state: active shader
│   ├─ glUniformMatrix4fv(u_Model) ─────────────────────────→ model matrix → GPU register
│   ├─ glBindVertexArray(item.mesh VAO) ────────────────────→ GPU state: active VAO
│   │                                                          (VBO + EBO + layout restored)
│   └─ glDrawElements / glDrawArrays
│       GPU vertex shader runs per vertex:
│         reads a_Position, a_Color from VBO  (static, uploaded at startup)
│         reads u_ViewProjection from UBO     (uploaded once this frame)
│         reads u_Model from uniform register (uploaded for this object)
│         writes gl_Position, v_Color
│       GPU fragment shader runs per fragment:
│         reads v_Color interpolated across triangle
│         depth test compares against depth buffer
│         writes to colour buffer if depth passes
│
├─ ImGui::Render()
│   └─ reads CPU state only (deltaTime, camera pos, item count)
│      draws overlay into colour buffer
│
└─ glfwSwapBuffers() ───────────────────────────────────────→ colour buffer → screen
                                                               depth buffer discarded
```

---

## Detailed Engineering Tasks

### 1. Camera

Implement a camera class that owns or computes:
- `glm::vec3 position`, forward/right/up vectors
- Field of view, aspect ratio, near/far clip planes
- View matrix (`glm::lookAt`)
- Projection matrix (`glm::perspective`)

Requirements:
- **a)** Separate camera *state* from input polling — camera does not read GLFW directly; input handler mutates camera state
- **b)** Recompute projection matrix when the framebuffer is resized (`OnResize(width, height)`)
- **c)** Clamp pitch to ±89° when using mouse-look to prevent gimbal flip
- **d)** Movement must be frame-rate independent — scale all deltas by `deltaTime`

Support modes: free-fly (WASD + mouse-look) as the primary mode for this sprint. First-person and third-person are future extensions.

---

### 2. Transform

Add a `Transform` data structure with:
- `glm::vec3 position`
- `glm::vec3 rotation` (Euler angles in degrees, or quaternion — your choice, document it)
- `glm::vec3 scale`
- `glm::mat4 GetModelMatrix() const` — builds TRS matrix: `T * R * S`
- Normal matrix derivation: `glm::transpose(glm::inverse(glm::mat3(modelMatrix)))` — needed for lighting later; compute it in the UBO upload path, not per shader call

Non-uniform scale must be supported (a box stretched on one axis should work).

---

### 3. Submission Data Types

Create three new structs/classes, likely in `renderer/src/core/` or a new `renderer/src/scene/` folder:

#### `RenderItem`
Describes one drawable object for a single frame:
```
MeshBuffer*      mesh        — which geometry to draw
ShaderProgram*   shader      — which program to use (material slot for Sprint 4)
Transform        transform   — position, rotation, scale in world space
GLenum           topology    — GL_TRIANGLES (default), GL_LINES for debug
bool             visible     — skip this item if false
```

#### `CameraData`
Snapshot of camera matrices for one frame:
```
glm::mat4 view
glm::mat4 projection
glm::mat4 viewProjection   — precomputed VP
glm::vec3 position
```

#### `FrameSubmission`
Everything the renderer needs to draw a frame:
```
CameraData                    camera
glm::vec4                     clearColor
std::vector<RenderItem>       items
bool                          wireframe     — debug toggle
```

The sample app constructs a `FrameSubmission` every frame and hands it to the renderer. It never calls `SubmitDraw` directly after this sprint.

---

### 4. Per-Frame Uniform Blocks (UBOs)

Upload per-frame GPU constants through a Uniform Buffer Object rather than scattering `SetUniform` calls across draw submissions.

Minimum UBO layout for Sprint 3:

```glsl
// binding = 0, updated once per frame
layout(std140) uniform FrameUniforms {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProjection;
    vec3 u_CameraPos;
    float u_Time;
};
```

Per-object data (changes per draw):

```glsl
// Still via SetUniform for now — one mat4 per object
uniform mat4 u_Model;
```

When Sprint 4 introduces a proper UBO for per-object data, `u_Model` moves there. For now, `SetUniform("u_Model", item.transform.GetModelMatrix())` inside the draw loop is acceptable.

Implementation notes:
- Create a `UniformBuffer` class (thin RAII wrapper, similar to `Buffer`) or reuse `Buffer` with a new type
- Bind to a fixed binding point (0) at renderer init; shaders declare `binding = 0`
- Update with `glBufferSubData` each frame before iterating render items

---

### 5. Refined Renderer Frame API

Replace the current `BeginFrame / SubmitDraw × N / EndFrame` pattern with:

```
I.   BeginFrame(FrameSubmission&)
II.  Upload FrameUniforms UBO
III. Set clear color from submission
IV.  Iterate submission.items:
       - skip if !item.visible
       - apply wireframe toggle if set
       - bind item.shader
       - SetUniform("u_Model", item.transform.GetModelMatrix())
       - bind item.mesh
       - Draw()
       - unbind mesh
V.   Render ImGui overlay
VI.  EndFrame() — swap handled by Application
```

`UnbindShader()` should be called once after the item loop, not inside it.

The old `SubmitDraw(ShaderProgram&, MeshBuffer&)` can remain for backwards compatibility during the transition but should be marked deprecated.

---

### 6. Update Shaders

`basic.vert` needs to consume the new uniforms:

```glsl
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProjection;
    vec3 u_CameraPos;
    float u_Time;
};

uniform mat4 u_Model;

out vec3 v_Color;

void main() {
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
    v_Color = a_Color;
}
```

`basic.frag` is unchanged.

---

### 7. ImGui Debug Overlay

Integrate ImGui (add to CMake via FetchContent if not already present). Keep it to one overlay window. Contents:

| Field | Source |
|---|---|
| Frame time (ms) | `deltaTime * 1000` |
| FPS | `1.0f / deltaTime` |
| Camera position | `camera.position` |
| Camera yaw / pitch | Camera state |
| Framebuffer size | From `Renderer` / GLFW |
| Submitted items | `submission.items.size()` |
| Wireframe toggle | `submission.wireframe` checkbox |

ImGui render must happen after the main scene draw and before `glfwSwapBuffers`. It must not corrupt depth/blend state — save and restore renderer pipeline state around the ImGui pass, or use ImGui's own GL state backup.

---

### 8. Test Scene

Replace `SampleScene` with a scene built entirely via `FrameSubmission`. The scene should demonstrate that transforms work visually:

- At least one row of cubes at different X positions (same Y, Z)
- At least one object with non-uniform scale
- At least one rotated object (rotate around Y axis, optionally animated with `u_Time`)
- A flat quad as a ground plane (scaled wide and flat, placed at Y=0)
- Camera starting position above and behind the scene, angled down slightly so depth and perspective are immediately obvious
- WASD + mouse-look navigation working from the first frame

The scene should contain at least 6–8 distinct `RenderItem`s to prove the submission loop works for multiple objects.

---

## New Files

| File | Purpose |
|---|---|
| `renderer/src/core/Camera.h/.cpp` | Perspective camera: state, view/projection matrix, free-fly update |
| `renderer/src/core/Transform.h/.cpp` | TRS transform with `GetModelMatrix()` |
| `renderer/src/core/RenderItem.h` | `RenderItem` struct |
| `renderer/src/core/FrameSubmission.h` | `FrameSubmission` + `CameraData` structs |
| `renderer/src/core/UniformBuffer.h/.cpp` | RAII UBO wrapper |
| `test-app/src/Scene3D.h/.cpp` | Sprint 3 demo scene (replaces SampleScene in the render callback) |
| `shaders/basic.vert` | Updated with `u_Model`, `u_ViewProjection`, UBO block |

---

## Modified Files

| File | Change |
|---|---|
| `renderer/src/core/Renderer.h/.cpp` | Add `BeginFrame(FrameSubmission&)` overload; add UBO management; add ImGui init/render |
| `renderer/src/app/Application.h/.cpp` | Pass camera delta time; forward resize event to camera |
| `test-app/src/main.cpp` | Build `FrameSubmission` in render callback; instantiate `Scene3D` |
| `CMakeLists.txt` | Add ImGui via FetchContent; add new `.cpp` sources |

---

## Definition of Done

- [ ] Sample app builds a `FrameSubmission` every frame — no direct `SubmitDraw` calls in `main.cpp` or scene code
- [ ] Multiple objects render at distinct positions, rotations, and scales through the renderer-facing scene path
- [ ] Camera movement (WASD + mouse-look) is smooth, stable, and frame-rate independent
- [ ] Projection matrix updates on window resize without stretching or clipping artifacts
- [ ] Per-frame data (view, projection, camera pos) is uploaded through the UBO once per frame, not through scattered `SetUniform` calls
- [ ] ImGui overlay renders without corrupting scene depth test, blending, or input
- [ ] The test application depends only on public renderer headers — no internal GL calls in `main.cpp` or scene files
- [ ] Build passes cleanly on `./build.sh` with no new warnings
