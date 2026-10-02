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
- **Multiple Backends**: Windows supports Vulkan, Direct3D 12, and OpenGL (partial); macOS uses Metal; Linux supports Vulkan and OpenGL (partial); Android uses Vulkan with a GLES3 fallback.
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
- **Not supported yet:** iOS / Web. CI builds Windows, macOS, Linux, and Android, and includes Linux Vulkan smoke checks with Godot 4.5 and 4.6.
- **OpenGL backend:** Godot uses OpenGL3 while Rive needs 4+. Applied a small patch upon official repo to support OpenGL3
   - MacOS doesn't support native GLES fallback so cannot work on MacOS right now. I'll looking into this when I have time, maybe fallback to ANGLE when ANGLE backend got fixed.
   - ANGLE Backend: Godot official builds links ANGLE statically. I can only make it work using dynamic-linked libEGL and libGLESv2.
- **MoltenVK:** Seems that MoltenVK is missing some features, rendered texture is blotchy. Please use Metal on MacOS.

## Todo

- [ ] **Platform Support**: Physical device testing for Android; iOS and Web support.
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
    - **Vulkan**: `glslangValidator` (from the Vulkan SDK or glslang tools package)
    - **macOS (Metal)**: Xcode Command Line Tools (`xcrun`)
- **Android**:
    - Android SDK + NDK 28 or later (CI pins NDK 28.1.13356709; pass `ndk_version=<installed-version>` for local builds)
    - `glslangValidator` and `spirv-opt` on PATH (or set `GLSLANG_VALIDATOR` and `SPIRV_OPT`)
    - Modern Vulkan headers (≥ 1.4.318) — set `VULKAN_SDK` to a directory containing them, or let CI fetch them automatically
    - The extension links with 16 KB page alignment for compatible Android devices.

### Build Steps

The Rive submodule is pinned to upstream `runtime-v0.1.502` (`820cc19bd8405f84a8f85f9f388a133d6a2c7511`). Its matching HarfBuzz 13.1.1 and Yoga grid dependencies are downloaded from fixed commits with SHA-256 verification; upgraded caches use separate directories from older builds.

1. **Clone the repository** (including submodules):
   ```bash
   git clone --recursive https://github.com/maidopi-usagi/RiveGD.git
   cd godot-rive
   ```

2. **Generate Rive Shaders**:
   Before compiling, generate the shader headers for the Rive runtime. The third positional argument selects `macos`, `windows`, `linux`, or `android`. Omitting it selects the current host platform. Android requires `spirv-opt` for upstream's Adreno driver workarounds. The script reads the pinned runtime's shader rules and invalidates its cache when shader sources, platform, or optimizer configuration change.
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
   Open the `project/` directory in Godot. The `RiveControl`, `RiveCanvas2D`, and `RiveRaw` nodes should be available.

   After a desktop debug build, run the GPU smoke test from the repository root:
   ```bash
   godot --path project --rendering-method mobile --rendering-driver vulkan \
       --render-thread separate --script ../scripts/smoke_test.gd
   ```
   It checks rendered path/image pixels, SVG resource reload, binary `.riv` import, and resource teardown. Use a real graphics display; headless mode does not exercise GPU rendering.
