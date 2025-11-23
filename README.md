# RiveGD

An **unofficial** Rive runtime with hardware accelerated GPU Renderer for Godot 4 as a GDExtension. Implemented Rive Renderer as rendering backend instead of CPU approaches with Skia. Artboards are directly rendered into an Texture.


https://github.com/user-attachments/assets/615cefe9-f9ba-4821-b8d4-bf70510b7d0c


## Usage

This extension is still highly WIP.

DO NOT USE IN PRODUCTION as APIs will change and stability is not tested well.

So far there's only a basic RiveViewer Control Node implemented. Just create and load a file and use it as a regular ui panel.

More functionality in the Rive Runtime will be add later.

## Limitations

- **Not tested on:** Linux/Android/iOS
- **OpenGL backend:** Godot uses OpenGL3 while Rive needs 4+. Applied a small patch upon official repo to support OpenGL3
   - MacOS doesn't support native GLES fallback so cannot work on MacOS right now. I'll looking into this when I have time, maybe fallback to ANGLE when ANGLE backend got fixed.
   - ANGLE Backend: Godot official builds links ANGLE statically. I can only make it work using dynamic-linked libEGL and libGLESv2.
- **MoltenVK:** Seems that MoltenVK is missing some features, rendered texture is blotchy. Please use Metal on MacOS.

## Building from Source

### Prerequisites

- [Godot 4.5+](https://godotengine.org/)
   
   - Note that DirectX12 Backend works incorrectly on 4.5 for some reason, while 4.6 is fine.

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
   git clone --recursive https://github.com/maidopi-usagi/RiveGD.git
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
