# RiveGD

[![Release Builds](https://github.com/maidopi-usagi/RiveGD/actions/workflows/release.yml/badge.svg)](https://github.com/maidopi-usagi/RiveGD/actions/workflows/release.yml)

An **unofficial** Rive runtime with hardware accelerated GPU Renderer for Godot 4 as a GDExtension. Implemented Rive Renderer as rendering backend instead of CPU approaches with Skia. Artboards are directly rendered into a Texture.

WIP!! PRs are welcomed.

> Prebuilt binaries (macOS-universal, Windows-x86_64, Linux-x86_64, Android-arm64; debug + release) are attached to every [GitHub Release](https://github.com/maidopi-usagi/RiveGD/releases).


https://github.com/user-attachments/assets/615cefe9-f9ba-4821-b8d4-bf70510b7d0c

| <img src="https://github.com/user-attachments/assets/4d2b2bf4-3e52-47c8-8e48-cb562d0cb637" width="100%" /> | <img src="https://github.com/user-attachments/assets/3e05f52d-6b5c-4b64-aa8d-ee32bb1d1284" width="100%" /> | <img src="https://github.com/user-attachments/assets/60ab0db4-9f11-4778-8f70-89e278516135" width="100%" /> |
| :---: | :---: | :---: |
| Texture Sharing | DataBinding | Custom Vector Rendering |


## Features

- **Hardware Accelerated Rendering**
- **Multiple Backends**: Supports Vulkan, Metal, Direct3D 12, OpenGL (desktop, partially), and GLES3 (Android fallback).
- **Godot Integration**:
    - `RiveControl`: A Control node for UI integration.
    - `RiveFileInstance`: A Node2D for 2D scene integration.
    - `RiveCanvas2D` + `RiveRaw`: 2D canvas that walks all descendants and exposes a `draw_rive(renderer)` signal for fully custom GDScript-driven Rive drawing.
    - `RiveFile`: Resource-based workflow for `.riv` files. Supports **hot-reloading** when files are updated externally.
    - `RiveSVG` / SVG import: load `.svg` files as Rive paths and draw them through the Rive renderer.
- **Custom Vector Drawing API** (GDScript-friendly):
    - `RivePath` (move/line/cubic/quad/close, `add_rect/add_oval/add_circle/add_poly/add_path`, `parse_svg`)
    - `RivePaint` (fill/stroke, color, gradient, thickness, join/cap, feather)
    - `RiveGradient` (linear/radial with color stops)
    - `RiveImage` from a Godot `Texture2D`, drawable via `draw_image` / `draw_image_mesh`
    - `RiveFont` + `RiveText` (multi-run text shaping via `shape_glyphs()`)
- **Rive Features Support**:
    - **State Machines**: Full support for State Machines, Inputs (Number, Boolean, Trigger), and Listeners.
    - **ViewModels**: Support for Rive ViewModels including Text, Number, Boolean, Enum, Color, and Triggers.
      - **Textures Sharing**: Pass Godot's Texture resources efficiently to Rive (via `ViewModelImageProperty`)
    - **Data Binding**: Update Rive properties dynamically from Godot via GDScript or the Inspector.
- **Interactivity**:
    - Mouse/Pointer input forwarding (Hover, Click, Move).
    - `RiveControl` supports hit-test so input events can handle with Godot's builtin Controls.
- **Editor Integration**:
    - Custom Inspector for selecting Artboards, Animations, and State Machines.
    - Dynamic property list generation for State Machine inputs and ViewModel properties.

## Usage

This extension is still highly WIP.

DO NOT USE IN PRODUCTION as APIs will change and stability is not tested well.

### Basic Usage

1. **Import**: Drop your `.riv` or `.svg` files into the Godot project. They will be automatically imported as `RiveFile` resources.
2. **UI**: Add a `RiveControl` node to your scene for UI elements.
3. **2D Scene**: Add a `RiveFileInstance` or `RiveMultiInstance` node under `RiveCanvas2D` for 2D game objects.
4. **Custom drawing**: Add a `RiveRaw` under `RiveCanvas2D`, connect its `draw_rive(renderer)` signal, and use `RivePath`/`RivePaint`/`RiveGradient`/`RiveImage`/`RiveText` to draw whatever you want. See `project/raw_drawing_demo.gd` (12 sections covering paths, gradients, clipping, images, image-mesh, multi-run text, jelly morph, etc.) and `project/svg_demo.gd` (downloads the Ghostscript Tiger SVG and renders it).
5. **Configuration**:
   - Assign the `Rive File` property in the inspector.
   - Select the desired **Artboard**.
   - Choose an **Animation** or **State Machine** to play.
   - (Optional) Configure State Machine inputs directly in the Inspector under the "Rive" group.

## Limitations

- **Android**: arm64 builds are produced by CI (Vulkan primary, GLES3 fallback). Not yet tested on a physical device — expect rough edges.
- **iOS**: Not supported yet.
- Linux and Windows builds are produced by CI but only lightly smoke-tested.
- **OpenGL backend:** Godot uses OpenGL3 while Rive needs 4+. Applied a small patch upon official repo to support OpenGL3
   - MacOS doesn't support native GLES fallback so cannot work on MacOS right now. I'll looking into this when I have time, maybe fallback to ANGLE when ANGLE backend got fixed.
   - ANGLE Backend: Godot official builds links ANGLE statically. I can only make it work using dynamic-linked libEGL and libGLESv2.
- **MoltenVK:** Seems that MoltenVK is missing some features, rendered texture is blotchy. Please use Metal on MacOS.

## Todo

- [ ] **Platform Support**: Physical device testing for Android; iOS support.
- [ ] **Android**: Color space correction (gamma→linear) for linear-color-space Godot projects.
- [ ] **Rendering**: Add more vector drawing commands.
- [ ] **Integration**: Implement `RiveRenderTargetTexture` for rendering Rive content to a Godot Texture resource.
- [ ] **Events**: Add support for Rive Events (Note: Rive recommends using DataBinding/ViewModels for most interactions).
- [ ] **Documentation**: Add docs when APIs becomes stable, and add several demos.
- [ ] **Scripting**: Enable Luau scripting support within Rive.
- [ ] **Text**: Add support for using Godot fonts in Rive.
- [ ] **Audio**: Add audio support (TBD).

## Building from Source

### Prerequisites

- [Godot 4.5+](https://godotengine.org/)
   
   - Note that DirectX12 Backend works incorrectly on 4.5 for some reason, while 4.6 is fine.

- [SCons](https://scons.org/)
- [Python 3](https://www.python.org/)
    - **Dependencies**: Install `ply` via pip: `pip install ply`
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
- **Android**:
    - Android SDK + NDK (the build system auto-detects the latest installed NDK version)
    - `glslangValidator` and `spirv-opt` on PATH
    - Modern Vulkan headers (≥ 1.4.318) — set `VULKAN_SDK` to a directory containing them, or let CI fetch them automatically

### Build Steps

1. **Clone the repository** (including submodules):
   ```bash
   git clone --recursive https://github.com/maidopi-usagi/RiveGD.git
   cd godot-rive
   ```

2. **Generate Rive Shaders**:
   Before compiling, you need to generate the shader headers for the Rive runtime. The third positional arg picks a platform-specific subset (`macos` / `windows` / `linuxbsd`); omit it to build all.
   ```bash
   python3 scripts/build_shaders.py \
       third-party/rive-runtime/renderer/src/shaders \
       third-party/rive-runtime/renderer/include/generated/shaders \
       macos
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

   *Example (Android arm64 debug):*
   ```bash
   scons platform=android target=template_debug arch=arm64
   ```

4. **Use in Godot**:
   Open the `project/` directory in Godot. The `RiveViewer` node should be available.
