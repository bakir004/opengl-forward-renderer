# Modern OpenGL Forward Renderer

A modular, modern **OpenGL 4.6 Core Profile** forward renderer written in C++20, targeting Windows and Linux desktop. It implements imported assets, a metallic-roughness PBR material system, directional and point/spot lighting, directional shadow mapping, HDR rendering with tone mapping and bloom, image-based lighting, procedural terrain, frustum culling, and an ImGui debug overlay.

## Quick start

**New to this project? Start with [docs/setup.md](./docs/setup.md).** It is
the single, self-contained guide for installing prerequisites, configuring
CMake, building, and running - written so it requires no prior knowledge of
this codebase.

The short version, once prerequisites are installed ([details](./docs/setup.md#1-prerequisites)):

```bash
git clone <repository-url> ForwardRenderer
cd ForwardRenderer
./build.sh      # Linux/macOS/Git Bash - Release by default
# or
build.bat       # Windows - Release by default
```

> **The project path must not contain spaces.** See
> [docs/setup.md](./docs/setup.md#2-get-the-source) for why.

## Documentation

| Document | Contents |
| :------- | :------- |
| [docs/setup.md](./docs/setup.md) | **Start here.** Prerequisites, CMake configuration, build, first run, troubleshooting. |
| [docs/scenes.md](./docs/scenes.md) | Every demo scene: purpose, controls, what a correct run looks like. |
| [docs/architecture.md](./docs/architecture.md) | Renderer core, resources, shaders, materials, scene submission, lighting/shadows, post-processing, diagnostics, per-frame flow. |
| [docs/materials.md](./docs/materials.md) | PBR parameters, texture slots, sRGB/linear policy, normal-map conventions, fallbacks. |
| [docs/assets.md](./docs/assets.md) | Asset folder layout, naming conventions, `.mat` file schema. |
| [docs/BistroAssetSetup.md](./docs/BistroAssetSetup.md) | How to download and place the optional Bistro demo asset bundle. |
| [GITFLOW.md](./GITFLOW.md) | Branch naming and commit message conventions (contributors only). |

## Features

- **Platform & context** - GLFW window, GLAD-loaded OpenGL 4.6 Core Profile context, GL debug callback wired to spdlog, JSON-driven runtime config (`config/settings.json`).
- **Renderer core** - explicit `Initialize` / resize / `BeginFrame` / `SubmitDraw` / `EndFrame` / shutdown lifecycle; no raw GL calls in application or scene code.
- **Geometry** - RAII buffer/VAO/mesh abstractions; built-in primitives (triangle, quad, cube, sphere); Assimp model import (OBJ, glTF, FBX, DAE) with multi-submesh meshes.
- **Resources** - `Texture2D` (stb_image, sRGB/Linear, mipmaps, fallbacks) and an `AssetImporter` cache that deduplicates shaders, textures, meshes, and materials by path.
- **Materials** - data-driven metallic-roughness PBR materials and per-object material instances; six PBR texture slots on fixed texture units. See [docs/materials.md](./docs/materials.md).
- **Camera & scene** - perspective camera (FreeFly / FirstPerson / ThirdPerson), per-object transforms, and a `FrameSubmission` / `RenderItem` scene submission API.
- **Lighting** - one directional light plus multiple point and spot lights, uploaded via std140 uniform blocks.
- **Shadows** - cascaded directional shadow mapping with PCF filtering, tunable bias/filter settings, and a per-cascade debug preview.
- **PBR & normal mapping** - Cook-Torrance BRDF (GGX / Smith / Schlick) with tangent-space normal mapping.
- **Post-processing** - HDR offscreen target, tone mapping (Reinhard / ACES / Uncharted 2) with exposure control, and bloom (bright-pass + separable Gaussian blur).
- **Image-based lighting** - skybox/cubemap background, diffuse irradiance map, prefiltered specular map, and BRDF integration LUT.
- **Terrain** - procedural seed-based heightfield with layered noise, height/slope material zones, hydraulic + thermal erosion, and instanced vegetation placement.
- **Optimization** - CPU-side frustum culling and render-queue sorting to reduce redundant state changes.
- **Diagnostics** - ImGui debug overlay with frame/pass timings, culling and draw/state counters, light/camera/resource inspectors, and intermediate-target previews.

## Demo scenes

Switch between scenes with number keys `1`-`6` or the top `Scenes` menu.
Full descriptions, controls, and what a correct run should look like are in
[docs/scenes.md](./docs/scenes.md).

| Key | Scene | Purpose |
| :-- | :---- | :------ |
| `1` | **Terrain** | Procedural terrain generation with material zones, masks, and tuning controls. |
| `2` | **Bistro** | Large imported scene exercising PBR materials, shadows, HDR, bloom, and IBL. Requires a manual asset download - see [docs/BistroAssetSetup.md](./docs/BistroAssetSetup.md). |
| `3` | **Capture: Baseline** | Small, stable object set for low-cost reference captures. |
| `4` | **Capture: Dense Grid** | Many repeated objects to make draw-call and pass-timing changes visible. |
| `5` | **Capture: Material Sweep** | Many material instances to exercise material/state counters. |
| `6` | **Capture: Culling Test** | Objects inside, outside, and behind the camera to compare submitted/visible/culled counts. |

## Debug overlay

Press `X` to show or hide the runtime debug overlay (the ImGui inspector on
the left side of the window), and `TAB` if mouse-look is captured and you
need UI control. The **Stats** tab shows frame timing, render pass timings,
culling counts, draw/state-change counters, lights, cameras, and resource
counts - the fastest way to confirm a scene is behaving correctly. Press `H`
at any time for the full in-app keyboard-shortcut reference.

### IDE / LSP support

For **Neovim (clangd)** or **VS Code**, the build script generates a
`compile_commands.json` in `build/`. Symlink it to the project root so your
LSP can find headers:

```bash
ln -s build/compile_commands.json .
```

## Known limitations

This renderer is a one-term academic project scoped to a forward-rendering graphics subsystem only - no deferred/G-buffer pipeline, no editor, physics, audio, or scripting, and no Vulkan/DirectX backend. For the full list of unsupported features and known issues, see [docs/limitations.md](./docs/limitations.md).

## Contributing

See [GITFLOW.md](./GITFLOW.md) for branch naming and commit message
conventions, enforced by git hooks installed via `build.sh`/`build.bat`.