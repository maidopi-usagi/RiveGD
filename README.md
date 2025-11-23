# RiveGD

Rive runtime for Godot 4 as a GDExtension. Using Rive Renderer as rendering backend instead of CPU approaches with Skia. Artboards are directly rendered into an Texture2DRD.

## Limitations

- **Not tested on:** Linux/Android/iOS
- **OpenGL backend:** Not implemented yet.


## Building from Source

### Prerequisites

- [Godot 4.3+](https://godotengine.org/)
- [SCons](https://scons.org/)
- [Python 3](https://www.python.org/)
- C++ Compiler (Clang, GCC, or MSVC)
- **Vulkan SDK**: Ensure `VULKAN_SDK` environment variable is set.
- **Windows (D3D12)**:
    - **DirectX Headers**: The build system looks for `d3dx12.h`. It automatically checks:
        - `%LOCALAPPDATA%\Godot\build_deps\agility_sdk\build\native\include\d3dx12` (Standard Godot build dep location)
        - Or set `DIRECTX_HEADERS_PATH` environment variable.
    - **Shader Compiler**: `fxc` (from Windows SDK) must be in your PATH or standard location.
- Shader compilation tools:
    - **Vulkan**: `glslangValidator` and `spirv-opt` (from Vulkan SDK)
    - **macOS (Metal)**: Xcode Command Line Tools (`xcrun`)

### Build Steps

1. **Clone the repository** (including submodules):
   ```bash
   git clone --recursive https://github.com/maidopi-usagi/godot-rive.git
   cd godot-rive
   ```

2. **Generate Rive Shaders**:
   Before compiling, you need to generate the shader headers for the Rive runtime.
   ```bash
   python3 scripts/build_shaders.py third-party/rive-runtime/renderer/src/shaders third-party/rive-runtime/renderer/include/generated/shaders
   ```

3. **Compile the Extension**:
   Run SCons to build the GDExtension library.
   ```bash
   scons
   ```
   
   To build for a specific platform or target:
   ```bash
   scons platform=<platform> target=<template_debug|template_release>
   ```
   
   *Example (macOS debug):*
   ```bash
   scons platform=macos target=template_debug
   ```

4. **Use in Godot**:
   Open the `project/` directory in Godot. The `RiveViewer` node should be available.
