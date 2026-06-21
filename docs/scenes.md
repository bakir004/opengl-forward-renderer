# Demo Scenes

This document describes every scene registered in `test-app/src/main.cpp` -
i.e. everything reachable from the running application - what it
demonstrates, how to switch to it, and what a correct run looks like.

> The codebase contains additional scene classes under `test-app/src/`
> (`SampleScene`, `SolarSystemScene`, `DioramaScene`, `NeonCityScene`,
> `JapanScene`, `PbrValidationScene`, `NormalMapScene`, `IblValidationScene`)
> that are **not** registered in `main.cpp` and are therefore not part of the
> shipped demo. This document only covers scenes you can actually reach by
> running the application.

---

## Switching scenes

- **Number keys `1`–`6`** jump directly to the matching scene below.
- The **`Scenes` menu** in the topbar lists every successfully-loaded scene
  by name; click one to switch.
- A scene only appears in either list if its `Setup()` call returned `true`
  at startup. A scene whose assets are missing is skipped and a
  `spdlog::error` is logged - this is expected behavior for the Bistro
  scene unless you've downloaded its asset bundle (see
  [BistroAssetSetup.md](./BistroAssetSetup.md)).

---

## Key | Scene | Purpose

| Key | Scene | Purpose |
| :-- | :---- | :------ |
| `1` | **Terrain** | Procedural terrain generation with material zones, vegetation, and tuning controls. |
| `2` | **Bistro** | Large imported scene exercising PBR materials, shadows, HDR, bloom, and IBL. Requires a manual asset download. |
| `3` | **Capture: Baseline** | Small, stable object set for low-cost reference captures. |
| `4` | **Capture: Dense Grid** | Many repeated objects to make draw-call and pass-timing changes visible. |
| `5` | **Capture: Material Sweep** | Many material instances to exercise material/state counters. |
| `6` | **Capture: Culling Test** | Objects inside, outside, and behind the camera to compare submitted/visible/culled counts. |

---

## 1 - Terrain

**Source:** `test-app/src/TerrainScene.cpp`

Procedurally generates a heightfield terrain (noise-stacked hills, mountains,
plateaus, optional hydraulic + thermal erosion, optional volcano shaping),
builds a renderable mesh from it, classifies it into material zones
(water/sand/grass/forest/rock/snow), and scatters instanced vegetation across
plantable areas.

**What to check:**
- Terrain renders with smoothly blended material zones (no hard contour
  lines between, e.g., grass and rock).
- A water plane is visible at the configured sea level.
- Vegetation (trees, grass, rocks) appears in the appropriate zones.
- The sidebar's **Terrain** tab exposes generation parameters and a sticky
  **Regenerate** button at the bottom; pressing it should reflow the terrain
  without errors.
- Shadows from the directional light fall correctly across the terrain.

**Controls:** Standard camera controls (see [§ Shared Controls](#shared-controls-all-scenes)) plus the Terrain sidebar tab for live parameter tuning.

---

## 2 - Bistro

**Source:** `test-app/src/BistroScene.cpp`

Loads the NVIDIA ORCA Amazon Lumberyard Bistro interior + exterior FBX
models with full PBR materials (including packed ORM textures), multiple
point lights, a directional sun with shadows, and several swappable HDR
skyboxes for IBL comparison.

**Prerequisite:** This scene's assets are **not tracked in git**. Before
expecting scene `2` to appear, follow
[docs/BistroAssetSetup.md](./BistroAssetSetup.md) to download and place the
Bistro bundle at `assets/models/fbx/bistro_v5_2/`. If the assets are absent,
`BistroScene::Setup()` returns `false`, the scene is silently skipped, and
the numbering of the remaining scenes does **not** shift - keys `3`–`6` will
simply not exist in that run (the app only registers scenes that loaded
successfully, in order).

**What to check (once assets are present):**
- Interior and exterior geometry both render with textures, normal maps,
  and correct metallic/roughness response.
- A small ImGui panel in the top-right lets you switch between five skybox
  environments (Bistro Sky, two night skies, Neutral Room, Outdoor Sky);
  switching should relight the scene via IBL without errors.
- Transparent materials (glass, wine, water) do not cast shadows.
- Emissive materials (lamps, signs) glow visibly.

**Controls:** Standard camera controls.

---

## 3 - Capture: Baseline

**Source:** `test-app/src/CapturePresetScene.cpp` (`CapturePresetKind::Baseline`)

A small, deterministic set of primitives (floor, a blue cube, a brass
sphere, a clay block) under directional + point lighting with shadows
enabled. Intended as a stable low-cost reference shot - useful for
before/after comparisons when changing renderer internals.

**What to check:** All four objects render with correct shading and cast
shadows onto the floor; draw-call count in the Stats tab is low and stable.

---

## 4 - Capture: Dense Grid

**Source:** `test-app/src/CapturePresetScene.cpp` (`CapturePresetKind::DenseGrid`)

A 12×12 grid (144 objects) of cubes/spheres with varied materials and
heights, lit and shadowed. Designed to make draw-call and pass-timing
changes obvious in the Stats tab.

**What to check:** All 144 objects render without flicker or missing
geometry; the Stats tab's draw-call and triangle counts move noticeably
when toggling **Frustum culling** in the Stats tab.

---

## 5 - Capture: Material Sweep

**Source:** `test-app/src/CapturePresetScene.cpp` (`CapturePresetKind::MaterialSweep`)

A 5×4 grid of spheres sweeping metallic (rows) against roughness (columns),
letting you visually confirm the Cook-Torrance BRDF response across the
full parameter range in one shot.

**What to check:** Metallic increases left-to-right tint shift toward
albedo; roughness increases top-to-bottom highlight spread. No row/column
should look discontinuous with its neighbors.

---

## 6 - Capture: Culling Test

**Source:** `test-app/src/CapturePresetScene.cpp` (`CapturePresetKind::CullingTest`)

Places objects directly in front of the camera, far to each side (outside
the frustum), and behind the camera. Shadows are disabled for this preset
so the Stats tab's submitted/visible/culled counts isolate frustum-culling
behavior specifically.

**What to check:** With **Frustum culling** enabled (Stats tab), the
"Culled" count should be greater than zero and "Visible" should be less
than "Submitted." Disabling culling should make "Visible" equal
"Submitted."

---

## Shared controls (all scenes)

| Key / Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Move |
| `SPACE` / `LEFT CTRL` | Move up / down |
| `LEFT SHIFT` | Sprint (movement speed ×3) |
| Mouse (while captured) | Look around |
| `TAB` | Toggle mouse capture (look mode) |
| Right mouse button (hold) | Temporary look mode without toggling capture |
| Scroll wheel | Zoom (FOV in FreeFly/FirstPerson, orbit radius in ThirdPerson) |
| `F1` / `F2` / `F3` | Camera mode: FreeFly / FirstPerson / ThirdPerson |
| `C` | Cycle camera mode |
| `X` | Toggle the inspector sidebar |
| `Z` | Toggle wireframe |
| `N` | Toggle normal maps |
| `K` | Toggle skybox |
| `H` | Toggle the in-app keyboard-shortcut help window |
| `F11` | Toggle fullscreen |
| `1`–`6` | Jump to a registered scene (see table above) |

The sidebar's **Stats** tab is the fastest way to confirm a scene is
behaving correctly: FPS, frame time, per-pass timings, draw calls, triangle
counts, and culling counts are all there without needing to read logs.