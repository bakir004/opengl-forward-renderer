# Modern OpenGL Forward Renderer

## Quick start

Pull the repo locally and run the executable `build.sh` on Linux or `build.bat` on Windows. 
This will let CMake download the necessary dependencies and build the project.

If you do not have CMake installed, please visit the [CMake website](https://cmake.org/download/) to download and install it.
Alternatively, install it via the package manager of your choice.

### Prerequisites

To build this project, you need **CMake (3.26+)** and a **C++ compiler**. On Linux, you also need the development headers for X11/Wayland and OpenGL that GLFW requires to compile from source.

#### Install CMake & Dependencies

| System | Package Manager | Command |
| :--- | :--- | :--- |
| **Arch Linux** | `pacman` | `sudo pacman -S cmake base-devel libx11 libxrandr libxinerama libxcursor libxi mesa` |
| **Ubuntu / Debian** | `apt` | `sudo apt update && sudo apt install cmake build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev` |
| **Fedora** | `dnf` | `sudo dnf install cmake gcc-c++ libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel` |
| **Windows** | `winget` | `winget install kitware.cmake` |

> **Note for Linux Users:** Even though the build script fetches C++ libraries (GLFW, spdlog, JSON) automatically, the system-level development headers listed above must be installed via your package manager first.

### IDE / LSP Support
For the best experience in **Neovim (clangd)** or **VS Code**, the build script generates a `compile_commands.json` in the `build/` directory.
You should symlink this to the project root so your LSP can find the headers:

```bash
ln -s build/compile_commands.json .
```
