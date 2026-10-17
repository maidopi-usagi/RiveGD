# RiveGD

[![Release Builds](https://github.com/maidopi-usagi/RiveGD/actions/workflows/release.yml/badge.svg)](https://github.com/maidopi-usagi/RiveGD/actions/workflows/release.yml)

An **unofficial** Rive runtime with hardware accelerated GPU Renderer for Godot 4 as a GDExtension. Implemented Rive Renderer as rendering backend instead of CPU approaches with Skia. Artboards are directly rendered into a Texture.

WIP!! PRs are welcomed.

> Prebuilt binaries (macOS-universal, Windows-x86_64, Linux-x86_64, Android-arm64; debug + release) are available from [GitHub Releases](https://github.com/maidopi-usagi/RiveGD/releases).

https://github.com/user-attachments/assets/615cefe9-f9ba-4821-b8d4-bf70510b7d0c

| <img src="https://github.com/user-attachments/assets/4d2b2bf4-3e52-47c8-8e48-cb562d0cb637" width="100%" /> | <img src="https://github.com/user-attachments/assets/3e05f52d-6b5c-4b64-aa8d-ee32bb1d1284" width="100%" /> | <img src="https://github.com/user-attachments/assets/60ab0db4-9f11-4778-8f70-89e278516135" width="100%" /> |
| :---: | :---: | :---: |
| Texture Sharing | DataBinding | Custom Vector Rendering |

## Features

- **GPU rendering**: Vulkan / D3D12 on Windows, Metal on macOS, Vulkan on Linux and Android. OpenGL and GLES3 support is partial.
- **Godot nodes**: `RiveControl` for UI; `RiveCanvas2D` with `RiveFileInstance` / `RiveMultiInstance` for 2D scenes; `RiveRaw` for custom vector drawing.
- **Resources**: `.riv` import and hot reload, optional SVG import, Godot textures and fonts, and external asset loading helpers.
- **Rive playback**: Animations, state machines, events, responsive layout, mixing and cross-fades.
- **Data binding**: Nested ViewModels, scalar and resource properties, lists, reusable presets, and Inspector editing with Undo/Redo.
- **Input and helpers**: Pointer, touch, keyboard, gamepad, text selection / IME, component followers and an optional accessibility bridge.
- **Optional backends**: Godot audio bus routing and Rive-signed Luau scripts.

## Usage

This extension is still WIP. APIs may change; it is not ready for production use.

### Basic Usage

1. Drop a `.riv` file into your project. For SVG, select **Import As > Rive File** explicitly.
2. Add a `RiveControl`, assign the file, then select an artboard and animation or state machine.
3. For 2D scenes, use a `RiveFileInstance` or `RiveMultiInstance` under `RiveCanvas2D`.
4. For custom drawing, add `RiveRaw` under `RiveCanvas2D` and connect `draw_rive(renderer)`.

### Data Binding

Edit properties in the Inspector or from GDScript:

```gdscript
$RiveControl.set_property_value("Score", 100)
$RiveControl.set_property_value("Avatar", preload("res://avatar.png"))
$RiveControl.set_property_value("menu[0].label", "Home")
```

Inspector edits are saved with the scene. Use `RiveViewModelPreset` for reusable initial values; direct ViewModel mutations are runtime-only.

### Examples

- [Vector drawing](project/raw_drawing_demo.gd)
- [SVG rendering](project/svg_demo.gd)

## Limitations

- iOS and Web are not supported yet.
- Android device testing and cross-platform IME / screen-reader verification are still needed.
- OpenGL support is partial. Use Metal on macOS; MoltenVK rendering is unreliable and statically linked ANGLE is not supported.
- Rendering requires a graphics context; headless tests do not validate GPU output.

## Todo

- [ ] Android device testing and linear color-space support; iOS and Web support.
- [ ] Richer preset editing and more vector drawing commands.
- [ ] Inspector list creation and drag reordering.
- [ ] Cross-Control batching and mobile performance testing.
- [ ] More examples and API documentation.

## Building from Source

### Prerequisites

- [Godot 4.5+](https://godotengine.org/) (4.6 recommended for D3D12).
- [Python 3](https://www.python.org/), [SCons](https://scons.org/), `ply`, and a C++20 compiler.
- Vulkan targets: Vulkan SDK 1.4.321.1 with `VULKAN_SDK` set.
- Windows: MSVC, Windows SDK `fxc`, and DirectX-Headers (`DIRECTX_HEADERS_PATH` if not auto-detected).
- macOS: Xcode with the Metal compiler.
- Android: Android SDK and NDK 28+; CI uses NDK 28.1.13356709.

### Build Steps

Replace `<platform>` with `windows`, `macos`, `linux`, or `android`:

```bash
git clone --recursive https://github.com/maidopi-usagi/RiveGD.git
cd RiveGD
python3 -m pip install scons ply
python3 scripts/build_shaders.py \
    third-party/rive-runtime/renderer/src/shaders \
    third-party/rive-runtime/renderer/include/generated/shaders <platform>
scons platform=<platform> target=template_debug
```

Use `template_release` for release builds. Android also needs `arch=arm64 ndk_version=<installed-version>`. Then open `project/` in Godot.

Optional flags: `rive_audio_backend=godot` routes audio through Godot; `rive_scripting=yes` enables Rive-signed Luau bytecode. Scripting is disabled by default.

### Tests

GitHub Actions builds all four platforms and runs Linux Vulkan regressions on Godot 4.5 / 4.6. Manual runs with a blank release tag produce artifacts only.

For desktop checks with a graphics display:

```bash
godot --path project --rendering-method mobile --rendering-driver vulkan \
    --render-thread safe --script ../scripts/smoke_test.gd
```

Other regression suites are in [scripts/](scripts/); their setup is listed in the [CI workflow](.github/workflows/release.yml).
