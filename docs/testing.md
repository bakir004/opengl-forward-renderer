# Testing & Acceptance Guide

This document describes how to validate the Forward Renderer after a build, run regression tests, perform benchmarks, and execute the clean-machine setup validation checklist required for release.

---

## 1. Acceptance Checklist

Before marking a build as ready for release, confirm these critical behaviors on at least two different test machines (one Windows, recommended one Linux):

### 1.1 Application Launch & Config

- [ ] Application window opens without errors (title and size from `config/settings.json`)
- [ ] Console shows `[Application] ImGui initialized` and no `[GL]` error-severity logs
- [ ] ImGui inspector and topbar render without visual corruption
- [ ] Fullscreen toggle (`F11`) works bidirectionally
- [ ] Window can be resized without crashes or GL error spam

### 1.2 Scene Registration & Switching

- [ ] Exactly 6 scenes appear in the `Scenes` menu on first run (or fewer if Bistro assets are absent)
- [ ] Number keys `1`–`6` jump to the correct scene by name
- [ ] Menu selection updates the active scene without freezing
- [ ] Scene names match the table in [docs/scenes.md](./scenes.md)
- [ ] Bistro scene only appears if `assets/models/fbx/bistro_v5_2/` is present; absence is silent (no crash, no error log)

### 1.3 Terrain Scene (Key `1`)

- [ ] Terrain mesh renders with elevation variation
- [ ] Material zones (water/sand/grass/forest/rock/snow) blend smoothly with no hard contours
- [ ] Water plane is visible at configured sea level
- [ ] Vegetation (trees, grass, rocks) scatters in appropriate zones without gaps or overlaps
- [ ] Shadows from directional light fall correctly across terrain and vegetation
- [ ] **Terrain** sidebar tab exposes generation parameters (octaves, frequency, amplitude, scale, etc.)
- [ ] Pressing **Regenerate** reflows the terrain without errors or memory leaks
- [ ] Camera controls (WASD, mouse, Sprint) work; no stutter or lag spikes

### 1.4 Bistro Scene (Key `2`, if assets present)

- [ ] Interior and exterior geometry render with textures, normal maps, and correct shading
- [ ] Metallic/roughness response is visually plausible across all PBR materials
- [ ] A top-right ImGui panel lets you switch between 5 skybox environments
- [ ] Switching skyboxes recomputes IBL without visual pop-in or errors
- [ ] Transparent materials (glass, wine, water) do not cast shadows
- [ ] Emissive materials (lamps, signs) glow visibly under IBL
- [ ] Shadows and lighting remain stable when panning the camera

### 1.5 Capture Scenes (Keys `3`–`6`)

**Baseline (Key `3`):**

- [ ] All 4 objects (floor, blue cube, brass sphere, clay block) render correctly
- [ ] Shadows cast onto the floor from both directional and point lights
- [ ] Stats tab shows low, stable draw-call count (<10)

**Dense Grid (Key `4`):**

- [ ] All 144 objects render without flicker, Z-fighting, or missing geometry
- [ ] Stats tab: toggling **Frustum culling** changes draw-call and triangle counts visibly
- [ ] Culled count > 0 with culling enabled; Culled count = 0 with culling disabled

**Material Sweep (Key `5`):**

- [ ] All 20 spheres (5 metallic rows × 4 roughness columns) render smoothly
- [ ] Metallic sweep (left-to-right) shows tint shift toward albedo
- [ ] Roughness sweep (top-to-bottom) shows highlight spread
- [ ] No visual discontinuity between adjacent rows or columns

**Culling Test (Key `6`):**

- [ ] Objects in frustum are visible; objects outside frustum are culled
- [ ] Stats tab: with **Frustum culling** on, "Culled" > 0 and "Visible" < "Submitted"
- [ ] Stats tab: with **Frustum culling** off, "Visible" = "Submitted"

### 1.6 Input & Camera

- [ ] WASD + SPACE/CTRL move the camera smoothly
- [ ] LEFT SHIFT (sprint) increases movement speed ×3
- [ ] TAB toggles mouse capture; RMB hold provides temporary look mode
- [ ] Mouse look (while captured) updates camera without lag or inversion issues
- [ ] Scroll wheel zooms (affects FOV in FreeFly/FirstPerson, orbit radius in ThirdPerson)
- [ ] F1/F2/F3 switch camera modes; C cycles them
- [ ] Camera mode switch persists across scenes

### 1.7 Debug UI & Stats

- [ ] **Stats** tab shows FPS, frame time, draw calls, triangle count
- [ ] **Stats** tab shows per-pass timings (shadow, main, post, etc.) in milliseconds
- [ ] Draw-call count and triangle count change when toggling culling or rendering flags
- [ ] X toggles the sidebar; H shows/hides the shortcut help window
- [ ] Z toggles wireframe without errors
- [ ] N toggles normal maps (visual difference should be obvious on high-detail geometry)
- [ ] K toggles skybox (skybox appears/disappears)

### 1.8 Logging

- [ ] No log spam (repeating messages every frame)
- [ ] No `[GL]` error-severity lines in console during normal operation
- [ ] Scene setup failures (e.g., missing Bistro assets) log once at startup, not repeatedly

---

## 2. Smoke Test (Quick Sanity Check)

Run this after every build to catch regressions before deeper testing:

**Time: ~2 minutes**

1. Build (Release): `build.bat` or `./build.sh`
2. Allow the app to launch and idle on the Terrain scene for 5 seconds
   - [ ] Window is open, ImGui is visible
   - [ ] Console shows no `[GL]` error lines
   - [ ] FPS in Stats tab is >30 (or >60 if GPU is capable)
3. Press `2` → Bistro scene (or advance to next available scene if Bistro assets missing)
   - [ ] Scene switches without freezing
   - [ ] New geometry loads without visual pop-in
4. Press `TAB` to enable mouse capture, then move the mouse and WASD for 3 seconds
   - [ ] Camera responds smoothly
   - [ ] No stutter or input lag
5. Press `X` to toggle the sidebar off, then on
   - [ ] Sidebar collapses and expands without corruption
6. Press `Z` to toggle wireframe
   - [ ] Scene switches to/from wireframe mode
7. Press `ESC` or close the window
   - [ ] Application terminates cleanly with no access violations or heap corruption

---

## 3. Regression Test

Run this monthly or after major feature changes to ensure nothing breaks:

**Time: ~15 minutes**

### 3.1 Build Test

```bash
# Release
build.bat Release   # or ./build.sh Release

# Debug
build.bat Debug     # or ./build.sh Debug
```

- [ ] Release build completes without linker errors
- [ ] Debug build completes without linker errors
- [ ] Both produce an executable under `build/test-app/`

### 3.2 Scene Completeness Test

For each registered scene (or each that passes Setup()):

1. Launch the app
2. Press the scene's number key (1–6)
3. Idle for 3 seconds
4. Record:
   - [ ] Scene name in window title or ImGui (match [docs/scenes.md](./scenes.md) table)
   - [ ] Geometry is visible (not black/empty)
   - [ ] FPS is ≥30
   - [ ] Stats tab shows >0 triangles and >0 draw calls
   - [ ] No `[GL]` error lines in console

### 3.3 Render State Test

On **Terrain** scene:

1. Press `Z` (wireframe) → Press `Z` again (fill)
   - [ ] Scene renders in wireframe, then returns to fill without errors
2. Press `N` (normal map toggle) twice
   - [ ] Visual appearance changes (objects appear less detailed when off)
3. Press `K` (skybox toggle) twice
   - [ ] Skybox appears/disappears; lighting may change subtly if using IBL
4. In **Stats** tab, toggle **Frustum culling** on/off
   - [ ] "Culled" count changes from 0 to a positive number and back

### 3.4 Lighting Test

On **Bistro** (if assets present) or **Dense Grid** (Key `4`):

1. Confirm shadow regions exist under objects
   - [ ] Shadows are not uniform black; they have gradient falloff (PCF)
2. Confirm point lights illuminate nearby geometry with correct falloff
3. Confirm directional light illuminates the entire scene consistently

### 3.5 Memory Leak Test

1. Launch the app and let it run on Terrain for 60 seconds
2. Cycle through all scenes (press 1, 2, 3, 4, 5, 6 over 2 minutes)
3. Press X to toggle sidebar on/off 10 times rapidly
4. Close the app
   - [ ] No freezing, stutter, or lag (indicates leaks or unbounded allocations)
   - [ ] Application terminates cleanly

---

## 4. Benchmark Procedure

Benchmarks are optional but recommended to establish performance baselines before optimization work.

**Prerequisites:**

- Same test machine for all runs
- Run in Release build
- Close background applications (browser, Discord, etc.)
- GPU driver up to date

### 4.1 Capture: Baseline (Key `3`) - Draw-Call Baseline

Scene: 4 simple objects, no culling, 1 shadow pass.

1. Launch: `TestApp` or `TestApp.exe`
2. Press `3` (Baseline scene)
3. Let it settle for 5 seconds
4. Record from **Stats** tab:
   - FPS (target: >100)
   - Frame time (target: <10 ms)
   - Draw calls (expected: ~12–15)
   - Triangle count (expected: ~20k)

### 4.2 Capture: Dense Grid (Key `4`) - Culling Impact

Scene: 144 objects, culling toggleable, 1 shadow pass.

1. Press `4` (Dense Grid scene)
2. Let it settle for 5 seconds
3. **With culling enabled** (default in Stats tab):
   - Record: FPS, Frame time, Draw calls, Visible count
4. **Toggle culling off** in Stats tab
   - Record: FPS, Frame time, Draw calls, Visible count (should equal Submitted)
5. **Culling benefit** = (Submitted – Visible with culling) / Submitted × 100%
   - Target: >30% visible, <70% culled in default view

### 4.3 Capture: Material Sweep (Key `5`) - Material Bind Overhead

Scene: 20 material instances (all unique), 1 shadow pass.

1. Press `5` (Material Sweep scene)
2. Let it settle for 5 seconds
3. Record from **Stats** tab:
   - Draw calls (expected: ~22–25 with state batching)
   - Material switches (if exposed in debug UI)

### 4.4 Summary Benchmark Report

After running all four, compile a simple CSV or table:

| Scene                    | FPS | Frame (ms) | Draws | Triangles | Notes      |
| ------------------------ | --- | ---------- | ----- | --------- | ---------- |
| Baseline                 | 120 | 8.3        | 12    | 20k       | Expected   |
| Dense Grid (culling ON)  | 90  | 11.1       | 72    | 200k      | 50% culled |
| Dense Grid (culling OFF) | 45  | 22.2       | 144   | 200k      | No culling |
| Material Sweep           | 150 | 6.7        | 24    | 40k       | Expected   |

Keep this report for comparison against future optimizations.

---

## 5. Clean-Machine Validation (Release Acceptance)

This is the **gold standard** for acceptance: a person unfamiliar with the project can extract the release package, follow only `docs/setup.md`, and end up with a running application.

**Environment:** A VM or laptop with:

- Fresh OS install (no pre-existing dev tools or GPU drivers beyond manufacturer defaults)
- Internet access (for CMake FetchContent)
- Administrator access (for toolchain install)

### 5.1 Pre-Test Checklist

- [ ] Release package is compressed as `.zip` or `.tar.gz`
- [ ] Package includes:
  - Source code (`src/`, `test-app/`)
  - Build scripts (`build.sh`, `build.bat`)
  - Assets (`assets/shaders/`, `assets/materials/`, `assets/models/`, `assets/skybox/`)
  - Configuration (`config/settings.json`)
  - Documentation (`docs/setup.md`, `docs/scenes.md`, `docs/architecture.md`, `docs/materials.md`, `docs/limitations.md`, `README.md`)
  - CMakeLists.txt, .gitignore, etc.
- [ ] No `.git` history included (clean checkout, no branch cruft)
- [ ] No `build/` directory included (saves ~200 MB)
- [ ] No generated files (`compile_commands.json`, `*.o`, `*.exe`, etc.)

### 5.2 Setup Phase (Follow Only docs/setup.md)

1. **Extract package** to a fresh folder with no spaces in the path
   - [ ] All files present under extracted root

2. **Install prerequisites** (Section 1 of setup.md):
   - [ ] CMake 3.26+ installed and on PATH
   - [ ] C++ toolchain (g++/MSVC) installed and on PATH
   - [ ] Internet available for dependency fetch

3. **Build** (Section 3 of setup.md):
   - [ ] Run `build.bat` (Windows) or `./build.sh` (Linux/macOS)
   - [ ] Build completes without CMake errors
   - [ ] Binary produced at one of the expected paths (Section 3.3)

4. **Launch** (Section 4 of setup.md):
   - [ ] App window opens showing Terrain scene
   - [ ] ImGui UI is visible and responsive
   - [ ] Console shows no `[GL]` error-severity logs

### 5.3 Validation Phase (Section 6 - Next Steps)

1. **Read docs/scenes.md** to understand demo scenes
2. **Switch to each scene** using number keys 1–6:
   - [ ] Scene 1 (Terrain) — procedural terrain + vegetation visible
   - [ ] Scene 2 (Bistro) — skipped silently if no asset bundle, appears with full FBX geometry if bundle present
   - [ ] Scenes 3–6 (Capture presets) — small deterministic scenes for performance reference

3. **Test standard controls** (from docs/scenes.md):
   - [ ] WASD + mouse move camera
   - [ ] TAB toggles mouse capture
   - [ ] X toggles sidebar
   - [ ] Z toggles wireframe
   - [ ] ESC or window close terminates cleanly

### 5.4 Acceptance Sign-Off

If all checks pass, the build is **accepted**. Log result:

```
Date: 2026-06-22
OS: Windows 11 | Linux Ubuntu 22.04 | macOS 13
GPU: RTX 3080 | Intel Arc | Apple Silicon
Clean Machine: YES
Setup time: 45 min (first run includes CMake FetchContent)
All scenes: PASS
All controls: PASS
No GL errors: YES
Verdict: ACCEPTED for release
```

---

## 6. Known Test Limitations

- **Bistro scene requires manual asset download.** The clean-machine test will pass without it; Bistro simply won't appear. To test Bistro, follow [docs/BistroAssetSetup.md](./BistroAssetSetup.md) beforehand.
- **Performance varies by GPU.** Benchmark targets (>30 FPS for Dense Grid, >100 FPS for Baseline) are indicative; your mileage may vary.
- **Fullscreen on multi-monitor systems may behave unexpectedly.** Test on single monitor if fullscreen is critical.
- **OpenGL 3.3 contexts may show degraded rendering.** The app falls back to OpenGL 3.3 if 4.6 is unavailable, but visuals are only validated against 4.6 Core Profile.

---

## 7. Quick Reference: Test Matrices

### Platforms

| OS           | Compiler  | Generator              | Status    |
| ------------ | --------- | ---------------------- | --------- |
| Windows 11   | MSVC 2022 | Visual Studio 17       | Preferred |
| Windows 11   | MinGW-w64 | MinGW Makefiles        | Supported |
| Ubuntu 22.04 | g++ 11    | Unix Makefiles         | Supported |
| macOS 13     | clang     | Xcode / Unix Makefiles | Supported |

### GPU Generations Tested

| Vendor            | Model            | Driver         | Result                            |
| ----------------- | ---------------- | -------------- | --------------------------------- |
| NVIDIA            | RTX 3080 Ti      | 560+           | Full 4.6 support, excellent perf  |
| AMD               | RX 6900 XT       | 24.10+         | Full 4.6 support, good perf       |
| Intel             | Arc A770         | Latest         | Full 4.6 support, stable          |
| Intel             | Iris Xe (mobile) | Latest         | 4.6 context, lower perf           |
| Older (2015–2017) | Various          | Latest drivers | Falls back to 4.0–4.3, still runs |

---

## 8. Troubleshooting Failed Tests

### Smoke Test Fails at Launch

**Symptom:** Window doesn't open, or `[GL]` error log spam.

**Solutions:**

1. Update GPU driver to latest
2. Verify C++ runtime is installed (Visual C++ Redistributable on Windows)
3. Check that project path has no spaces
4. Verify OpenGL support: Run `glxinfo` (Linux) or GPU control panel (Windows/macOS)

### Scene Freezes During Cycle

**Symptom:** Switching scenes hangs or takes >5 seconds.

**Solutions:**

1. Check console for asset load errors (missing texture, shader compile fail)
2. Verify `assets/` folder is next to the executable
3. See docs/limitations.md for known asset issues

### Regression Test: Draw-Call Count Way Off

**Symptom:** Stats tab shows 200+ draw calls on Baseline scene.

**Solutions:**

1. Check that frustum culling is enabled (default in Stats tab)
2. Verify normal map toggle (N) is not doubling passes
3. Confirm shadow rendering is not counting as extra draws (it shouldn't)

### Clean-Machine Test: CMake Configure Fails

**Symptom:** `cmake -B build` or build script fails.

**Solutions:**

1. Verify CMake 3.26+ on PATH: `cmake --version`
2. Verify C++ toolchain on PATH: `g++ --version` or `cl`
3. Check that project path has no spaces
4. Manually run `cmake -B build -G "Unix Makefiles"` (or MSVC generator on Windows) for more verbose error output

---

## 9. Release Checklist

Before packaging and shipping:

### Code Quality

- [ ] No uncommitted changes in `src/` or `test-app/`
- [ ] All commits follow Conventional Commits (docs/testing.md, feat(renderer), etc.)
- [ ] No debug print statements or spdlog spam in release code
- [ ] No compiler warnings (at least in primary source files)

### Assets

- [ ] `assets/shaders/` contains all .vert, .frag, .glsl files needed by registered scenes
- [ ] `assets/materials/` contains `default.mat` and any scene-specific materials
- [ ] `assets/skybox/` contains the 5 skybox bundles (Bistro, Moody, Mountains, NeutralRoom, OutdoorSky)
- [ ] `config/settings.json` has reasonable defaults (window size, vsync, shadow map resolution)
- [ ] No large test files or placeholder data in assets/

### Documentation

- [ ] `README.md` lists all features and demo scenes accurately
- [ ] `docs/setup.md` is current and tested on clean machine
- [ ] `docs/scenes.md` matches registered scenes in `main.cpp`
- [ ] `docs/architecture.md` covers all public APIs and per-frame flow
- [ ] `docs/materials.md` documents PBR parameters and texture slots
- [ ] `docs/limitations.md` lists unsupported features and known bugs
- [ ] `docs/testing.md` (this file) is complete and tested

### Build & Package

- [ ] `build.bat` and `build.sh` work without user intervention
- [ ] `run.bat` and `run.sh` re-run the binary correctly
- [ ] Smoke test passes on at least 2 platforms
- [ ] All 6 demo scenes load and render (or are silently skipped if assets missing)
- [ ] No compiler warnings in Release build

### Archiving

- [ ] Package is `.zip` (Windows) or `.tar.gz` (Unix)
- [ ] Package unpacks to a single `ForwardRenderer/` directory with no internal structure surprises
- [ ] Package size is <100 MB (if it's >500 MB, `build/` directory was included accidentally)
- [ ] Package hash (SHA-256) is recorded for integrity verification

---

End of testing guide. For each acceptance round, fill in the checklist above and archive the results with the release package.
