# Modern OpenGL Forward Renderer

A modular, modern **OpenGL 4.6 Core Profile** forward renderer written in C++20, targeting Windows desktop. It implements imported assets, a metallic-roughness PBR material system, directional and point/spot lighting, directional shadow mapping, HDR rendering with tone mapping and bloom, image-based lighting, procedural terrain, frustum culling, and an ImGui debug overlay.

### GitFlow

> For contributors, see [Git Workflow Guide](./GITFLOW.md)

## Features

- **Platform & context** — GLFW window, GLAD-loaded OpenGL 4.6 Core Profile context, GL debug callback wired to spdlog, JSON-driven runtime config (`config/settings.json`).
- **Renderer core** — explicit `Initialize` / resize / `BeginFrame` / `SubmitDraw` / `EndFrame` / shutdown lifecycle; no raw GL calls in application or scene code.
- **Geometry** — RAII buffer/VAO/mesh abstractions; built-in primitives (triangle, quad, cube, sphere); Assimp model import (OBJ, glTF, FBX, DAE) with multi-submesh meshes.
- **Resources** — `Texture2D` (stb_image, sRGB/Linear, mipmaps, fallbacks) and an `AssetImporter` cache that deduplicates shaders, textures, meshes, and materials by path.
- **Materials** — data-driven metallic-roughness PBR materials and per-object material instances; six PBR texture slots on fixed texture units. See [docs/materials.md](./docs/materials.md).
- **Camera & scene** — perspective camera (FreeFly / FirstPerson / ThirdPerson), per-object transforms, and a `FrameSubmission` / `RenderItem` scene submission API.
- **Lighting** — one directional light plus multiple point and spot lights, uploaded via std140 uniform blocks.
- **Shadows** — directional shadow mapping with PCF filtering, tunable bias/filter settings, and a shadow-map debug view.
- **PBR & normal mapping** — Cook-Torrance BRDF (GGX / Smith / Schlick) with tangent-space normal mapping.
- **Post-processing** — HDR offscreen target, tone mapping (Reinhard / ACES / Uncharted 2) with exposure control, and bloom (bright-pass + separable Gaussian blur).
- **Image-based lighting** — skybox/cubemap background, diffuse irradiance map, prefiltered specular map, and BRDF integration LUT.
- **Terrain** — procedural seed-based heightfield with layered noise, height/slope material zones, and vegetation suitability masks.
- **Optimization** — CPU-side frustum culling and render-queue sorting to reduce redundant state changes.
- **Diagnostics** — ImGui debug overlay with frame/pass timings, culling and draw/state counters, light/camera/resource inspectors, and intermediate-target previews.

## Demo Scenes

The test application registers the following scenes. Switch between them with number keys `1`–`6` or the top `Scenes` menu. For full descriptions, controls, and expected results, see [docs/scenes.md](./docs/scenes.md).

| Key | Scene | Purpose |
| :-- | :---- | :------ |
| `1` | **Terrain** | Procedural terrain generation with material zones, masks, and tuning controls. |
| `2` | **Bistro** | Large imported scene exercising PBR materials, shadows, HDR, bloom, and IBL. |
| `3` | **Capture: Baseline** | Small, stable object set for low-cost reference captures. |
| `4` | **Capture: Dense Grid** | Many repeated objects to make draw-call and pass-timing changes visible. |
| `5` | **Capture: Material Sweep** | Many material instances to exercise material/state counters. |
| `6` | **Capture: Culling Test** | Objects inside, outside, and behind the camera to compare submitted/visible/culled counts. |

## Quick start

Pull the repo locally and run the executable `build.sh` on Linux or `build.bat` on Windows.
This will let CMake download the necessary dependencies and build the project.

If you do not have CMake installed, please visit the [CMake website](https://cmake.org/download/) to download and install it.
Alternatively, install it via the package manager of your choice.

Ensure the project is placed in a directory whose full path contains no spaces.
Paths with spaces (e.g. C:\Users\John\"Radna povrsina"\renderer) will cause linker failures and prevent the application window from opening.
Move the project to a location whose folders in path do not contain spaces.

### Prerequisites

To build this project, you need **CMake (3.26+)** and a **C++ compiler**. On Linux, you also need the development headers for X11/Wayland and OpenGL that GLFW requires to compile from source.

#### Install CMake & Dependencies

| System | Package Manager | Command |
| :--- | :--- | :--- |
| **Arch Linux** | `pacman` | `sudo pacman -S cmake base-devel libx11 libxrandr libxinerama libxcursor libxi mesa wayland wayland-protocols libxkbcommon pkgconf ninja` |
| **Ubuntu / Debian** | `apt` | `sudo apt update && sudo apt install cmake build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libwayland-dev wayland-protocols pkg-config libxkbcommon-dev ninja-build` |
| **Fedora** | `dnf` | `sudo dnf install cmake gcc-c++ make libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel wayland-devel wayland-protocols-devel libxkbcommon-devel pkgconf-pkg-config ninja-build` |
| **Windows** | `winget` | `winget install kitware.cmake` |

> **Note for Linux Users:** Even though the build script fetches C++ libraries (GLFW, spdlog, JSON) automatically, the system-level development headers listed above must be installed via your package manager first.

> **Note for Windows Users:** Verify that your C++ toolchain is correctly detected by your IDE or terminal.
>
> Ensure that your install's `bin/` directory is on your system PATH and that the following are recognized in `cmd.exe`:
> - `g++ --version`
> - `mingw32-make --version`
> - `cmake --version`
>
> If these commands are not recognized, the build script will fail with:
> `Error: No supported C/C++ toolchain detected.`
>
> Also ensure that your CMake generator is consistent. If you encounter an error like:
> `Does not match the generator used previously: Ninja`
>
> Delete the `build/` or `cmake-build-*` directory and rebuild.

## Debug Overlay and Capture Presets

The runtime debug overlay is the ImGui inspector shown on the left side of the window. Press `X` to show or hide it, and press `TAB` if mouse-look is captured and you need UI control. Open the `Stats` tab to inspect frame timing, render pass timings, culling counts, draw/state-change counters, lights, cameras, and scene/cache resource counts.

Demo scenes and capture presets can be selected from the top `Scenes` menu or with number keys `1` through `9`. The capture presets are deterministic scenes intended for screenshots, demos, and quick renderer-stat checks:

- `Capture: Baseline` keeps a small, stable object set for low-cost reference captures.
- `Capture: Dense Grid` submits many repeated objects so draw-call and pass timing changes are easy to see.
- `Capture: Material Sweep` uses many material instances to exercise material/state counters.
- `Capture: Culling Test` places objects inside, outside, and behind the camera view so submitted, visible, and culled counts are easy to compare.

The overlay timings are CPU-side measurements. Submitted items come from the active scene submission, visible items are accepted into the forward render queue after culling, culled items are rejected by camera-frustum culling, and draw/state counters are reset and measured each frame while the queue is flushed.

### IDE / LSP Support
For the best experience in **Neovim (clangd)** or **VS Code**, the build script generates a `compile_commands.json` in the `build/` directory.
You should symlink this to the project root so your LSP can find the headers:

```bash
ln -s build/compile_commands.json .
```

## Documentation

| Document | Contents |
| :------- | :------- |
| [docs/setup.md](./docs/setup.md) | Toolchain, OpenGL/GPU requirements, dependencies, build and run instructions. |
| [docs/architecture.md](./docs/architecture.md) | Renderer core, resources, shaders, materials, scene submission, lighting/shadows, post-processing, diagnostics, and per-frame flow. |
| [docs/materials.md](./docs/materials.md) | PBR parameters, texture slots, sRGB/linear policy, normal-map conventions, and fallbacks. |
| [docs/scenes.md](./docs/scenes.md) | Demo scenes: purpose, controls, expected results, and debug toggles. |
| [docs/testing.md](./docs/testing.md) | Acceptance checklist, smoke/regression tests, benchmark and clean-machine procedures. |
| [docs/limitations.md](./docs/limitations.md) | Unsupported features and known issues. |

## Known Limitations

This renderer is a one-term academic project scoped to a forward-rendering graphics subsystem only — no deferred/G-buffer pipeline, no editor, physics, audio, or scripting, and no Vulkan/DirectX backend. For the full list of unsupported features and known issues, see [docs/limitations.md](./docs/limitations.md).
