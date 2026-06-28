# Limitations — Unsupported Features and Known Issues

This document lists features that are intentionally **out of scope** for this project, capabilities that are **partially implemented**, and **known issues** a user or evaluator may encounter. It exists so that scope boundaries and rough edges are documented rather than discovered by surprise.

This renderer is a one-term academic project. Its goal is a credible, modular **forward-rendering graphics subsystem** — not a complete game engine.

---

## 1. Out of Scope (By Design)

The following are deliberately excluded. They are not bugs; they were never in scope for this project.

### 1.1 Rendering Architecture

- **Deferred rendering / G-buffer pipeline.** The renderer is forward-only. There is no geometry pre-pass, no G-buffer, and no tiled/clustered deferred shading.
- **Multi-backend abstraction.** OpenGL 4.6 Core is the only backend. There is no Vulkan, DirectX, or Metal path, and the codebase is not abstracted behind a backend-agnostic RHI.

### 1.2 Advanced Lighting & Global Illumination

- **Screen-space effects** — SSAO, SSR, and screen-space shadows are not implemented.
- **Volumetrics** — no volumetric fog, light shafts, or god rays.
- **Advanced global illumination** — no real-time GI (voxel GI, light probes networks, path tracing, radiosity). Indirect lighting is limited to image-based lighting (IBL) from a single environment.
- **Full IBL authoring** — IBL resources (irradiance, prefiltered specular, BRDF LUT) are generated at runtime from a skybox cubemap; there is no offline bake pipeline or `.hdr` equirectangular import workflow beyond what the environment loader provides.

### 1.3 Engine Subsystems

The renderer is the **only** engine subsystem in scope. The following do not exist:

- Skeletal/skinned animation, morph targets, and animation playback.
- Particles and compute-driven effects.
- A scene editor / GUI authoring tool (the ImGui overlay is a debug inspector, not an editor).
- Gameplay systems, physics, collision, audio, networking, and scripting.

### 1.4 Asset Pipeline

- **Dynamic/streaming assets** — Assimp import is used for **static** meshes only. There is no level streaming or runtime asset hot-reloading guarantee.
- **Animation import** — although Assimp can parse animation data, the renderer does not consume or play it.

---

## 2. Partially Implemented / Limited

These features exist but are intentionally minimal or constrained.

| Feature | What works | Limitation |
| :------ | :--------- | :--------- |
| **Transparency** | The forward path has pragmatic hooks for blended draws (blend state, draw mode). | There is no full order-independent transparency, depth-sorted transparent queue, or refraction. The renderer is optimized for **opaque** rendering. |
| **Cascaded shadow maps (CSM)** | The shadow system is structured with cascades in mind and the renderer uses a cascaded shadow map resource. | CSM was specified as a **stretch goal** in the SRS; cascade count, split tuning, and stabilization are basic and not production-grade. |
| **Shadows for non-directional lights** | Directional shadow mapping is supported. | Point and spot lights illuminate but do **not** cast shadows (no cubemap/omnidirectional shadow maps). |
| **Terrain** | Procedural heightfield generation, material zones, and vegetation suitability masks. | Terrain is a single static heightfield. There is no chunking/LOD, runtime tessellation, or vegetation instancing/placement — the masks are metadata prepared for a future sprint. |
| **Erosion post-process** | — | The optional hydraulic-erosion stretch goal may be absent or minimal depending on build. |
| **Instanced rendering** | Repeated geometry can be drawn through the renderer. | Instancing coverage is limited to demo/capture scenes; it is not applied automatically across all repeated render items. |

---

## 3. Known Issues

### 3.1 Build & Environment

- **Paths with spaces break the build.** If the project's absolute path contains a space (e.g. `C:\Users\John\Radna povrsina\renderer`), the build can fail at link time and the window may not open. Place the project in a path with **no spaces**. (See the README "Quick start" note.)
- **Linux system headers required.** The build script fetches C++ libraries automatically, but X11/Wayland and OpenGL **development headers** must be installed via the system package manager first (see [setup.md](./setup.md) / README).
- **CMake generator mismatch.** Switching generators (e.g. to/from Ninja) without clearing the build directory produces a `Does not match the generator used previously` error. Delete `build/` (or `cmake-build-*`) and reconfigure.

### 3.2 Runtime

- **Scene availability is build-dependent.** The test application only runs scenes that report a successful `Setup()`. A scene whose assets are missing (e.g. large imported models) is skipped and logged rather than crashing — so the available scene list may differ between machines depending on which assets are present.
- **Large imported scenes are memory- and load-time-heavy.** Big models (e.g. the Bistro scene) can take noticeable time to import on first load and require sufficient GPU memory.
- **Performance targets are "interactive," not fixed.** Per NFR-001, the renderer targets interactive frame rates on moderate demo scenes on target hardware; there is no hard FPS guarantee, and heavy scenes on low-end GPUs may drop below interactive rates.

### 3.3 Visual / Correctness Caveats

- **Normal-map convention.** Normal maps are expected in **OpenGL (Y-up) tangent-space** convention. DirectX-sourced maps may need their green channel flipped (see [materials.md](./materials.md)).
- **Color-space mistakes are silent.** Loading a color texture as Linear (or a data texture as sRGB) does not error — it produces a washed-out or over-saturated result. Verify color space per slot (see [materials.md](./materials.md)).
- **Shadow bias is tuned per scene.** Default shadow bias works for the demo scenes; extreme camera distances or thin geometry may still show minor self-shadowing (acne) or peter-panning, tunable at runtime in the debug overlay.

---

## 4. Reporting

This is an academic deliverable; there is no public issue tracker. New defects discovered during evaluation should be recorded here under **Known Issues** with a short reproduction and the affected scene, so the limitation set stays current.
