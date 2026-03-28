# Modern OpenGL Forward Renderer

### GitFlow

> For contributors, see [Git Workflow Guide](./GITFLOW.md)

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

### IDE / LSP Support
For the best experience in **Neovim (clangd)** or **VS Code**, the build script generates a `compile_commands.json` in the `build/` directory.
You should symlink this to the project root so your LSP can find the headers:

```bash
ln -s build/compile_commands.json .
```

gas
