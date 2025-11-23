# RiveGD

<video src="https://raw.githubusercontent.com/maidopi-usagi/RiveGD/main/media/01.mp4" controls width="640">Your browser does not support the video tag. <a href="https://raw.githubusercontent.com/maidopi-usagi/RiveGD/main/media/01.mp4">Download video</a></video>

Rive runtime for Godot 4 as a GDExtension. Using Rive Renderer as rendering backend instead of CPU approaches with Skia. Artboards are directly rendered into an Texture2DRD.

## Limitations

- **Vulkan backend:** Work in progress — currently not supported due to Godot doesn't expose vkGetInstanceProcAddr.
- **OpenGL backend:** Not implemented yet.


## Building from Source

### Prerequisites

- [Godot 4.3+](https://godotengine.org/)
- [SCons](https://scons.org/)
- [Python 3](https://www.python.org/)
- C++ Compiler (Clang, GCC, or MSVC)
- Shader compilation tools:
    - **Vulkan**: `glslangValidator` and `spirv-opt` (from Vulkan SDK)
    - **macOS (Metal)**: Xcode Command Line Tools (`xcrun`)
    - **Windows (D3D)**: `fxc` (from Windows SDK)

### Build Steps

1. **Clone the repository** (including submodules):
   ```bash
   git clone --recursive https://github.com/maidopi-usagi/godot-rive.git
   cd godot-rive
   ```

2. **Generate Rive Shaders**:
   Before compiling, you need to generate the shader headers for the Rive runtime.
   ```bash
   python3 rive/generate_shaders.py third-party/rive-runtime/renderer/src/shaders third-party/rive-runtime/renderer/include/generated/shaders
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
