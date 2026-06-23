# OpenGL Forward Renderer — Architecture

This document describes the core architecture of the forward renderer: the renderer pipeline, resource management, shader system, materials, scene submission, lighting and shadows, post-processing, and the per-frame rendering flow.

---

## 1. Renderer Core

The `Renderer` class (`src/include/core/Renderer.h`) is the central orchestrator of the rendering pipeline. It is a **thin orchestrator** — it owns several helper objects but delegates all direct OpenGL calls to those helpers. This separation keeps the main renderer logic decoupled from GL state management.

### Key Responsibilities

- **Initialization**: Loads GLAD function pointers, enables GL debug output, sets default pipeline state.
- **Per-frame coordination**: Calls `BeginFrame()`, accepts `SubmitDraw()` calls, then calls `EndFrame()`.
- **Shadow rendering**: Manages cascaded shadow map rendering before main-scene rendering.
- **HDR rendering**: Manages off-screen HDR framebuffer so HDR values survive until tone mapping.
- **Environment lighting (IBL)**: Coordinates precomputed irradiance and specular maps.
- **Debug statistics**: Accumulates frame-level metrics (draw calls, triangle count, light counts, shadow stats).

### Owned Objects

| Member | Purpose |
|--------|---------|
| `InitContext` | One-time GL state: debug callback, default GL capabilities, error shader. |
| `SubmissionContext` | Current pipeline state snapshot: depth test, blend mode, cull mode. Cached to avoid redundant GL calls. |
| `RenderQueue` | Collects and sorts draw calls, manages shader batching and flush. |
| `UniformBuffer` (camera) | Holds per-frame camera data (view, projection, position). |
| `UniformBuffer` (lights) | Holds scene light data (directional, point, spot lights). |
| `CascadedShadowMap` | Depth-only FBO with 4 cascade layers for directional shadow mapping. |
| `HdrFramebuffer` | Float16 offscreen target (GL_RGBA16F) for HDR rendering. |
| `EnvironmentLightingPipeline` | Manages IBL: irradiance map, specular prefilter, BRDF LUT computation. |
| `Skybox` | Cubemap renderer for environment background. |

### State Tracking for Efficiency

The renderer caches current GL state in `SubmissionContext` to avoid redundant API calls:
- Only changed GL state is applied per frame.
- Shader switches are minimized via `RenderQueue::Sort()` batching.
- Texture unit assignments are managed per-material to avoid conflicts.

---

## 2. Resources

The renderer manages five primary GPU resource types. All use **RAII** semantics: destructors release GL objects, copy is disabled, move transfers ownership.

### 2.1 Buffer

**File**: `src/include/core/Buffer.h`

A wrapper around a single OpenGL buffer object (VBO, EBO, or UBO). Supports all three buffer types via a `Type` enum.

**Key operations**:
```cpp
Buffer vbo(Buffer::VERTEX, vertexData, byteSize, GL_STATIC_DRAW);
vbo.Bind();
vbo.UpdateData(newData, updateSize, offsetBytes);
vbo.Unbind();
```

**Important caveat**: EBO binding is VAO state. Never unbind `GL_ELEMENT_ARRAY_BUFFER` while a VAO is bound — unbinding modifies the active VAO's EBO binding.

### 2.2 VertexArray

**File**: `src/include/core/VertexArray.h`

RAII wrapper around a Vertex Array Object (VAO). Encapsulates all vertex attribute state (location, format, stride, offset) and the EBO binding.

**Usage pattern**:
```cpp
VertexArray vao;
vao.Bind();
vbo.Bind();      // Vertex buffer
ebo.Bind();      // Element buffer (VAO captures this)
vertexLayout.Apply();  // Defines attributes
vao.Unbind();
```

### 2.3 VertexLayout

**File**: `src/include/core/VertexLayout.h`

Describes vertex memory layout: attribute positions, component types, sizes, and strides. Computes byte offsets automatically. Call `Apply()` while the correct VAO and VBO are bound.

**Example**:
```cpp
VertexLayout layout;
layout.Push<glm::vec3>(0);  // Position at location 0
layout.Push<glm::vec3>(1);  // Normal at location 1
layout.Push<glm::vec2>(2);  // Texcoord at location 2
layout.Apply();             // Sets up glVertexAttribPointer calls
```

### 2.4 Texture2D

**File**: `src/include/core/Texture2D.h`

RAII 2D texture with stb_image loading, mipmap generation, and color-space interpretation. Supports two color spaces:
- **sRGB**: For color/albedo data. Driver converts to linear during sampling. Internal format: `GL_SRGB8_ALPHA8`.
- **Linear**: For non-color data (normals, roughness, metallic, AO). No conversion. Internal format: `GL_RGBA8`.

**Construction**:
```cpp
// Load from disk
Texture2D albedo("assets/models/albedo.png", TextureColorSpace::sRGB);
Texture2D normal("assets/models/normal.png", TextureColorSpace::Linear);

// Create fallback when load fails
Texture2D fallback = Texture2D::CreateFallback(255, 0, 255, 255);  // Magenta

// Create from procedural data
Texture2D renderTarget = Texture2D::CreateRenderTarget(
    512, 512, GL_RGBA16F, GL_RGBA, GL_FLOAT
);
```

### 2.5 UniformBuffer

**File**: `src/include/core/UniformBuffer.h`

GPU-side Uniform Buffer Object (UBO). Wraps a `Buffer` and provides:
- `Upload(data, size, offset)` — write CPU struct to GPU via `glBufferSubData`.
- `BindToSlot(bindingPoint)` — bind to a numbered binding point via `glBindBufferBase`.

**Requires std140 layout** on the CPU struct: `vec3` padded to `vec4` (add float pad), matrices already 16-aligned.

**Usage pattern**:
```cpp
// Define struct with std140 rules
struct CameraBlock {
    glm::mat4 view;          // 64 bytes
    glm::mat4 projection;    // 64 bytes
    glm::vec3 cameraPos;     // 12 bytes
    float     _pad;          //  4 bytes
};

// Create UBO
UniformBuffer cameraUBO(sizeof(CameraBlock));

// Each frame
CameraBlock block = { camera.GetView(), ... };
cameraUBO.Upload(&block, sizeof(block));
cameraUBO.BindToSlot(0);  // ShaderProgram maps the Camera block to slot 0
```

### 2.6 ShaderProgram

**File**: `src/include/core/ShaderProgram.h`

Manages GLSL shader lifecycle: file loading, per-stage compilation, linking, and uniform cache.

**Construction**:
```cpp
ShaderProgram meshShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
if (!meshShader.IsValid()) {
    // Compilation or linking failed; error was logged
}
```

**Uniform API** (type-safe, cached):
```cpp
meshShader.Bind();
meshShader.SetUniform("u_AlbedoColor", glm::vec3(1.0f, 0.8f, 0.6f));
meshShader.SetUniform("u_Metallic", 0.3f);
meshShader.SetUniform("u_Model", modelMatrix);
meshShader.Unbind();
```

---

## 3. Shaders

Shaders implement the rendering algorithms. The renderer uses a forward shading architecture with PBR (Physically Based Rendering) lighting.

### 3.1 Shader Files

| File | Purpose |
|------|---------|
| `mesh.vert` / `mesh.frag` | Main PBR forward shader: MVP transforms, normal mapping, Blinn-Phong → PBR lighting, IBL, cascaded shadow sampling. |
| `shadow_depth.vert` / `shadow_depth.frag` | Depth-only pass for cascaded shadow map generation. |
| `tone_mapping.vert` / `tone_mapping.frag` | Post-process: HDR-to-LDR tone mapping and bloom. |
| `ibl_brdf_lut.vert` / `ibl_brdf_lut.frag` | Offline BRDF integration lookup table (2D). |
| `ibl_irradiance.frag` | Convolve environment cubemap into diffuse irradiance. |
| `ibl_prefilter.frag` | Prefilter environment into specular mip levels. |
| `skybox.vert` / `skybox.frag` | Render cubemap background. |
| `error.vert` / `error.frag` | Fallback magenta shader for missing/broken materials. |
| `basic.vert` / `basic.frag` | Simple per-vertex color shader (primitives). |

### 3.2 Key GLSL Includes

**`light_block.glsl`**
Shared GLSL include defining the `LightBlock` UBO struct. Declares directional, point, and spot light arrays, packed in std140 layout. Included by `mesh.frag` and other shaders that need lighting.

**`pbr_helpers.glsl`**
Shared PBR math: Fresnel, distribution, geometric functions; combines with lighting loops for direct + indirect (IBL) shading.

### 3.3 Texture Unit Assignments

Fixed texture units for materials and IBL are defined in `Material.h`:

**Material texture units** (0–6):
```glsl
uniform sampler2D u_AlbedoMap;           // unit 0
uniform sampler2D u_NormalMap;           // unit 1
uniform sampler2D u_MetallicMap;         // unit 2
uniform sampler2D u_RoughnessMap;        // unit 3
uniform sampler2D u_AOMap;               // unit 4
uniform sampler2D u_EmissiveMap;         // unit 5
uniform sampler2D u_SpecularGlossinessMap; // unit 6
```

**Environment/IBL texture units** (8–11):
```glsl
uniform samplerCube u_IrradianceMap;     // unit 8 (diffuse IBL)
uniform samplerCube u_PrefilteredMap;    // unit 9 (specular IBL)
uniform sampler2D   u_BRDFLUT;           // unit 10 (BRDF split-sum)
uniform samplerCube u_SourceEnvironmentMap; // unit 11 (original reflection probe)
```

**Shadow map** (unit 7):
```glsl
uniform sampler2DArray u_CascadeShadowMaps; // unit 7 (4 cascade layers)
```

---

## 4. Materials

The material system provides a two-layer hierarchy:

### 4.1 Material Class

**File**: `src/include/core/Material.h`

Immutable material template. Owns a `ShaderProgram` and default parameter values (floats, vec3/vec4, textures).

**Creation**:
```cpp
auto meshShader = AssetImporter::LoadShader("mesh.vert", "mesh.frag");
auto material = std::make_shared<Material>(meshShader);

material->SetVec3("u_AlbedoColor", glm::vec3(0.9f, 0.8f, 0.7f));
material->SetFloat("u_Roughness", 0.4f);
material->SetFloat("u_Metallic", 0.8f);
material->SetTexture(TextureSlot::Albedo, albedoTex);
material->SetTexture(TextureSlot::Normal, normalTex);
```

**Binding**:
```cpp
material->Bind();  // Activates shader + uploads all defaults
```

### 4.2 MaterialInstance Class

Per-object override layer. Inherits all parameters from a parent `Material` but can override any subset for this instance.

**Usage**:
```cpp
auto instance = std::make_shared<MaterialInstance>(material);
instance->SetFloat("u_Roughness", 0.2f);  // Override
instance->SetTexture(TextureSlot::Albedo, differentAlbedo);

// When rendering
instance->Bind();  // Calls parent->Bind() first, then applies overrides
```

### 4.3 Texture Slots

Standard slot names for PBR materials (defined in `TextureSlot` namespace):

| Slot | Usage |
|------|-------|
| `Albedo` | Base color map (sRGB). |
| `Normal` | Surface normal perturbation in tangent space (Linear). |
| `Metallic` | Metallicness (0 = dielectric, 1 = metal). Single channel → replicated. Linear. |
| `Roughness` | Perceptual roughness (0 = mirror, 1 = rough). Single channel → replicated. Linear. |
| `AO` | Ambient occlusion. Single channel. Linear. |
| `Emissive` | Self-emitted light color (sRGB). |
| `SpecularGlossiness` | Legacy workflow: specular color (sRGB) + glossiness (Linear). |

### 4.4 Parameter Hierarchy

When rendering, the shader reads parameters in this priority order:
1. **Material defaults** (from `Material`).
2. **MaterialInstance overrides** (if instance is bound).
3. **Shader-declared defaults** (if no Material binds the uniform).

---

## 5. Scene Submission

The scene submission API connects scene data to the renderer for a single frame.

### 5.1 RenderItem

**File**: `src/include/scene/RenderItem.h`

Describes one renderable object:

```cpp
struct RenderItem {
    const MeshBuffer* mesh = nullptr;           // Simple single-mesh option
    const Mesh* meshMulti = nullptr;            // Multi-submesh option (takes priority)
    uint32_t subMeshIndex = 0;                  // Which submesh (if meshMulti set)
    const ShaderProgram* shader = nullptr;      // Legacy: shader without material
    const MaterialInstance* material = nullptr; // Takes priority over shader
    Transform transform;                        // Object's TRS in world space
    glm::quat rotationOffset;                   // Mesh orientation correction (quaternion)
    glm::vec3 translationOffset;                // Mesh origin correction
    PrimitiveTopology topology;                 // Triangles, Lines, Points, etc.
    DrawMode drawMode;                          // Fill, Wireframe, Points
    RenderFlags flags;                          // visible, castShadow, receiveShadow
};
```

Material priority: If `material` is set, it is used (provides both shader and parameters). Otherwise `shader` is used directly (legacy / primitive path).

### 5.2 FrameSubmission

**File**: `src/include/scene/FrameSubmission.h`

Bundled data passed from `Scene::BuildSubmission()` to `Renderer::BeginFrame()`:

```cpp
struct FrameSubmission {
    const Camera* camera = nullptr;
    const Skybox* skybox = nullptr;
    ReflectionProbe* activeReflectionProbe = nullptr;
    FrameClearInfo clearInfo;                    // Viewport + clear color
    SubmissionContext context;                  // Pipeline state
    LightEnvironment lights;                    // All scene lights
    std::vector<RenderItem> objects;            // All renderable objects
    float time = 0.0f;                         // Elapsed time (for animation)
    float deltaTime = 0.0f;                    // Frame delta time
};
```

### 5.3 Scene Base Class

**File**: `src/include/scene/Scene.h`

All renderable scenes inherit from `Scene`. The base class manages camera, lights, and object submission. Subclasses override `OnUpdate()` to implement per-frame logic, then call `BuildSubmission()` to produce a `FrameSubmission` for the renderer.

**Typical usage**:
```cpp
class MyScene : public Scene {
public:
    void Setup() override {
        SetCamera(makeFreeFlyCamera(...));
        SetClearColor(...);
        AddObject(renderItem);
    }

    void OnUpdate(float deltaTime, const KeyboardInput& kbd, const MouseInput& mouse) override {
        // Update camera, animation, physics, etc.
    }
};
```

---

## 6. Lighting and Shadows

### 6.1 Light Types

**File**: `src/include/scene/Light.h`

Three light types, each with shadow-mapping parameters:

| Type | Range | Shadow | Use |
|------|-------|--------|-----|
| `DirectionalLight` | Infinite (direction only) | Cascaded (4 levels) | Sun, moonlight |
| `PointLight` | Finite (position + radius) | None (future) | Lamps, fires |
| `SpotLight` | Finite (position + direction + cone) | None (future) | Flashlights, spotlights |

**Definition**:
```cpp
struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
    LightShadowParams shadowParams;
};

struct PointLight {
    glm::vec3 position;
    float radius;
    Attenuation attenuation;
    glm::vec3 color;
    float intensity;
};

struct SpotLight {
    glm::vec3 position;
    glm::vec3 direction;
    SpotCone cone;  // inner/outer degrees
    Attenuation attenuation;
    glm::vec3 color;
    float intensity;
};
```

### 6.2 LightEnvironment

**File**: `src/include/scene/LightEnvironment.h`

Scene-facing container:
- **1** DirectionalLight (sun)
- **Up to 16** PointLights (lamps)
- **Up to 8** SpotLights (flashlights)
- **Ambient color + intensity** (fallback when no IBL)

**Usage**:
```cpp
LightEnvironment lights;
lights.SetDirectionalLight(sun);
lights.AddPointLight(lamp1);
lights.AddPointLight(lamp2);
lights.SetAmbient(glm::vec3(0.1f), 0.5f);
```

### 6.3 Cascaded Shadow Mapping (Directional)

**File**: `src/include/core/shadows/CascadedShadowMap.h`

The directional light uses **cascaded shadow maps** (4 cascades):
- Each cascade covers a different distance range from the camera.
- Near objects use high-res shadow details; far objects use coarser cascades.
- Reduces artifacts and memory by adapting resolution to distance.

**Owned by Renderer**:
```cpp
std::unique_ptr<CascadedShadowMap> m_directionalShadowMap;
```

**Per-frame shadow pass**:
1. For each cascade (i = 0..3):
   - Bind shadow FBO, attach cascade layer.
   - Set viewport to shadow map resolution.
   - Set view-projection for that cascade's frustum.
   - Render all shadow casters (RenderFlags::castShadow == true).
   - Result: depth written to cascade layer i.

2. Main pass:
   - Bind HDR framebuffer (or default if no HDR).
   - For each pixel, determine which cascade is visible.
   - Sample cascade depth for shadow test (PCF if enabled).

**Debug visualization**: `RendererDebugStats` exposes per-cascade preview textures for ImGui inspection.

### 6.4 Light Packing for GPU

**File**: `src/include/scene/LightBlock.h`

Lights are packed into a `LightBlock` UBO struct in **std140 layout** and uploaded each frame. The shader includes `light_block.glsl` and reads lights from `layout(std140) uniform LightBlock { ... };`; `ShaderProgram` maps that block to binding point 1 after linking.

---

## 7. Post-Processing

Post-processing operates on the HDR render target after forward rendering completes.

### 7.1 HDR Framebuffer

**File**: `src/include/core/HdrFramebuffer.h`

RAII FBO with a `GL_RGBA16F` color attachment (allows HDR values > 1.0) and depth+stencil.

**Usage**:
```cpp
HdrFramebuffer hdr(1920, 1080);
hdr.Bind();       // Redirect main scene rendering here
// ... render scene
hdr.Unbind();     // Back to default FBO
```

The color texture (`hdr.GetColorTexture()`) is sampled by post-process passes.

### 7.2 Tone Mapping Pass

**File**: `assets/shaders/tone_mapping.vert/.frag`

Converts HDR scene (stored in HdrFramebuffer) to LDR for final output:
- Samples `u_MainTexture` (the HDR color texture).
- Applies tone curve (Reinhard, ACES, or similar).
- Applies bloom (optional).
- Outputs to default framebuffer.

**Per-frame sequence**:
1. Main forward pass renders into HDR FBO.
2. Tone mapping pass reads HDR FBO, writes to screen.

### 7.3 IBL Pipeline

**File**: `src/include/core/EnvironmentLightingPipeline.h`

Manages precomputed indirect lighting from cubemaps (environment reflection probes):
- **Irradiance map** (diffuse IBL) — convolved from source cubemap.
- **Prefiltered cubemap** (specular IBL) — mipmaps representing different roughness levels.
- **BRDF LUT** (split-sum approximation) — 2D lookup table for specular attenuation.

These are bound to fixed texture units (8, 9, 10) and sampled in `mesh.frag` during shading.

---

## 8. Diagnostics

### 8.1 RendererDebugStats

**File**: `src/include/core/Renderer.h`

Per-frame debug snapshot:

| Metric | Purpose |
|--------|---------|
| `submittedRenderItemCount` | Objects passed via `SubmitDraw()`. |
| `queuedRenderItemCount` | Objects accepted by render queue (visible + have geometry). |
| `processedRenderItemCount` | Objects actually drawn after sorting. |
| `drawCallCount` | Number of actual GL draw calls issued. |
| `approxTriangleCount` | Estimated triangle count. |
| `directionalLightCount`, `pointLightCount` | Active light count. |
| `shadowCasterCount`, `shadowReceiverCount` | Objects with shadow flags. |
| `frameTimeMs`, `fps` | Frame time and instantaneous FPS. |
| `shadowMapTextureId`, `-Width`, `-Height` | Shadow map GL handle + resolution. |
| `hdrColorTextureId`, `-Width`, `-Height` | HDR FBO color texture handle + size. |
| `iblSourceTextureId`, `iblIrradianceTextureId`, etc. | IBL cubemap and derived texture handles. |
| `cascadePreviewTextureIds[]` | Per-cascade depth layer preview textures. |
| `cascadeSplitDistances[]` | View-space distance covered by each cascade. |
| `iblDebugMode`, `iblDebugPrefilteredMip` | IBL debug visualization mode and settings. |

**Access**:
```cpp
const RendererDebugStats& stats = renderer.GetDebugStats();
printf("FPS: %.1f, Draw calls: %u\n", stats.fps, stats.drawCallCount);
```

### 8.2 GL Debug Callback

**File**: `src/include/core/InitContext.h`

`InitContext::Initialize()` installs a GL debug callback that logs all high/medium severity GL errors to spdlog. Helps catch:
- Texture unit conflicts.
- Shader uniform location misses.
- Framebuffer attachment mismatches.

---

## 9. Per-Frame Flow

The complete per-frame rendering sequence:

### 9.1 Application Loop (Simplified)

```cpp
while (running) {
    glfwPollEvents();                           // Input capture
    scene->OnUpdate(deltaTime, kbd, mouse);     // Scene logic
    
    FrameSubmission submission = scene->BuildSubmission();
    
    renderer.BeginFrame(submission);            // Shadow pass + setup
    
    for (const auto& item : submission.objects) {
        renderer.SubmitDraw(item);              // Enqueue draws
    }
    
    renderer.EndFrame();                        // Sort + flush
    
    imguiFrame.Render();                        // Debug UI
    glfwSwapBuffers();
}
```

### 9.2 Renderer::BeginFrame() Steps

1. **Shadow pass** (if directional light present):
   - For each cascade (i = 0..3):
     - Bind shadow FBO, attach cascade layer i.
     - Set viewport to shadow map resolution.
     - Compute view-projection for that cascade.
     - Clear depth buffer.
     - Render all items with `RenderFlags::castShadow == true` using depth-only shader.

2. **Main framebuffer setup**:
   - Bind HDR FBO (if post-processing enabled) or default FBO.
   - Set viewport from `FrameClearInfo`.
   - Clear color and depth buffers.

3. **UBO uploads**:
   - `m_cameraUBO.Upload(cameraBlock, sizeof(...))` — view, projection, position.
   - `m_lightUBO.Upload(lightBlock, sizeof(...))` — all lights in std140 layout.

4. **Queue setup**:
   - `m_queue.SetDirectionalShadowData(...)` — cascade matrices, splits, texture.
   - `m_queue.SetEnvironmentData(...)` — IBL cubemaps.
   - `m_queue.SetLightingDebugControls(...)` — diagnostic overrides.

5. **Pipeline state**:
   - Apply `SubmissionContext` (depth test, blend, cull).

### 9.3 Renderer::SubmitDraw() Steps

1. **Validation**: Check visible flag, resolve shader, ensure geometry exists.
2. **Enqueue**: Add to `m_queue` if valid.
3. **Track stats**: Increment submitted item count.

### 9.4 Renderer::EndFrame() Steps

1. **Sort**: `m_queue.Sort()` groups draws by shader to minimize state changes.
2. **Flush**: `m_queue.Flush()` iterates sorted items:
   - Bind shader (skip if already bound).
   - Set polygon mode (fill, wireframe, points).
   - Write `u_Model` and `u_NormalMatrix` uniforms.
   - Bind material (uploads textures, parameters).
   - Issue `glDrawElements()` or `glDrawArrays()`.
3. **Unbind**: Reset polygon mode to fill.
4. **Clear**: `m_queue.Clear()` for next frame.
5. **Blit**: If HDR enabled, tone-mapping pass reads HDR FBO and outputs to screen.

### 9.5 Post-Processing (Optional)

If HDR framebuffer is active:
1. `renderer.RebindHdrFramebuffer()` (if scene needs to add more draws, e.g., vegetation).
2. After all scene draws, unbind HDR FBO.
3. Run tone-mapping pass: read HDR texture, apply tone curve, output to default FBO.

---

## 10. Batch Optimization Strategy

The renderer minimizes GL state changes via:

- **Shader batching**: `RenderQueue::Sort()` groups by shader pointer. Consecutive items using the same shader avoid `glUseProgram()` calls.
- **Texture caching**: Material instances reuse parent shader and remember last-bound texture unit assignments.
- **UBO caching**: `SubmissionContext::Apply()` tracks which state has changed and only calls the necessary GL functions.
- **Pipeline state**: Depth test, blend mode, and cull mode are only applied if different from current.

---

## Conclusion

The forward renderer is organized as a pipeline of modular, reusable components:
- **GPU resources** (Buffer, Texture2D, etc.) use RAII for memory safety.
- **Materials** provide shader + parameter management at two levels (template + instance).
- **Scene submission** decouples scene logic from GL state management.
- **Per-frame flow** is a predictable sequence: shadow pass → main pass → post-processing.
- **Diagnostics** expose actionable metrics for profiling and debugging.

This architecture supports efficient rendering, easy asset integration via `AssetImporter`, and extensibility for future features (more post-effects, advanced shadows, etc.).
