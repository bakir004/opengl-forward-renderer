# Setup Guide

This is the **only document required** to get the Forward Renderer from a fresh
clone/unzip to a running application. If you are validating a clean-machine
build, follow this page top to bottom and nothing else.

---

## 1. Prerequisites

You need exactly two things on PATH: **CMake 3.26+** and a **C++20 compiler**.
Everything else (GLFW, spdlog, nlohmann/json, GLM, stb, Dear ImGui, Assimp) is
fetched automatically by CMake the first time you configure the project.

> **Important: The Bistro scene assets are required to start the program.**
> Before building, download and set up the NVIDIA ORCA Bistro bundle by
> following [docs/BistroAssetSetup.md](./BistroAssetSetup.md). Without these
> assets the application will not start. Be aware that the download is large
> and the full setup process (download + first configure + build) can take a
> significant amount of time.

### 1.1 Common to all platforms

| Tool | Minimum version | Check with |
|------|------------------|------------|
| CMake | 3.26 | `cmake --version` |
| Git | any recent | `git --version` (only needed if cloning rather than unzipping) |
| Internet access | - | required at first configure, to fetch dependencies via `FetchContent` |

### 1.2 Windows

- A C++ toolchain. Either works:
  - **MSYS2/MinGW-w64** - provides `g++` and `mingw32-make`.
  - **Visual Studio Build Tools** (C++ workload) - provides `cl`/`nmake`, or use with Ninja.
- Install CMake via `winget install kitware.cmake`, or download from
  [cmake.org](https://cmake.org/download/).
- Verify your toolchain is on PATH by running, in `cmd.exe`:
  ```
  g++ --version
  mingw32-make --version
  cmake --version
  ```
  If any of these are not recognized, fix your PATH before continuing -
  `build.bat` will fail with `Error: No supported C/C++ toolchain detected.`

### 1.3 Linux (Ubuntu/Debian shown; adapt for your distro)

CMake fetches the C++ *libraries* automatically, but the **system OpenGL /
windowing development headers** are not part of that fetch and must be
installed via your package manager first:

```bash
sudo apt update && sudo apt install \
  cmake build-essential \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libgl1-mesa-dev libwayland-dev wayland-protocols pkg-config \
  libxkbcommon-dev ninja-build
```

Equivalents for other package managers:

| System | Package Manager | Command |
| :--- | :--- | :--- |
| Arch Linux | `pacman` | `sudo pacman -S cmake base-devel libx11 libxrandr libxinerama libxcursor libxi mesa wayland wayland-protocols libxkbcommon pkgconf ninja` |
| Fedora | `dnf` | `sudo dnf install cmake gcc-c++ make libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel wayland-devel wayland-protocols-devel libxkbcommon-devel pkgconf-pkg-config ninja-build` |

### 1.4 GPU / driver requirement

A GPU and driver supporting **OpenGL 4.6 Core Profile** is preferred. At
startup the app tries 4.6 → 4.5 → 4.4 → 4.3 → 4.2 → 4.1 → 4.0 → 3.3 in that
order and uses the first context your driver accepts, so older GPUs may still
run it - but visuals/behavior are only validated against 4.6. Update your GPU
driver if the app fails to create a window.

---

## 2. Get the source

Either clone or unzip - both produce the same folder layout.

```bash
git clone <repository-url> ForwardRenderer
cd ForwardRenderer
```

or unzip the release package and `cd` into the extracted folder.

> **Critical: the project path must not contain spaces.**
> A path like `C:\Users\John\Radna povrsina\renderer` will compile but fail
> at **link time**, and the application window will not open. Place the
> project under a path with no spaces, e.g. `C:\dev\ForwardRenderer` or
> `~/dev/ForwardRenderer`.

---

## 3. Configure, build, and run

The recommended way to build and run the project is through the
**CMake Tools extension in Visual Studio** (or Visual Studio Code). This gives
you better control over the build configuration, easier debugging, and avoids
the limitations of the shell scripts.

### 3.1 Recommended: CMake Tools extension

1. Open the project root folder in **Visual Studio Code** (or **Visual Studio**
   with the CMake Tools extension installed).
2. CMake Tools will detect the `CMakeLists.txt` automatically. Select your
   preferred **kit** (compiler toolchain) when prompted.
3. Choose your build variant (**Release** for normal use, **Debug** for
   development) from the status bar.
4. Click **Build** (or press `F7`) to configure and compile. On the first run
   this will also fetch all dependencies via `FetchContent`, which may take
   several minutes.
5. Use the **Run** (▶) button in the status bar to launch `TestApp` directly from within the IDE.

This approach is preferred over the shell scripts because it integrates with
the IDE's build output, error navigation, and debugger.

### 3.2 Alternative: build scripts (not recommended)

`build.sh` / `build.bat` and `run.sh` / `run.bat` are provided as a
convenience but are not the recommended path. They configure CMake, build, and
run the binary in one step, but offer no IDE integration and can be harder to
troubleshoot when something goes wrong. Use the CMake Tools extension instead
wherever possible.

### 3.3 Where the binary ends up (all methods)

The executable is named **`TestApp`** (`TestApp.exe` on Windows). Depending on
your generator it lands in one of:

```
build/test-app/TestApp[.exe]
build/test-app/Debug/TestApp.exe
build/test-app/Release/TestApp.exe
```

The `assets/` folder is copied next to whichever of these is built, every
build, so the executable is self-contained and can be copied/zipped from
there if needed.

---

## 4. First run - what you should see

On a successful launch:

- A window opens (size/title from `config/settings.json`, default
  1280×720, titled "Forward Renderer").
- The **Terrain** scene loads by default (scene `1`).
- An ImGui inspector panel is docked on the left; a Dynamic-Island-style
  topbar is centered at the top.
- The console/log shows `[Application] ImGui initialized` and no `[GL]`
  error-severity lines.
