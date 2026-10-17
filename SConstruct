#!/usr/bin/env python
import os
import subprocess
from glob import glob
from pathlib import Path

env = SConscript("third-party/godot-cpp/SConstruct")

def apply_patch(patch_path, target_dir, patch_name):
    print(f"Checking for {patch_name} patch...")
    patch_file = os.path.abspath(patch_path)
    target_dir = os.path.abspath(target_dir)
    
    if not os.path.isfile(patch_file):
        raise RuntimeError(f"Required {patch_name} patch is missing: {patch_file}")
    if not os.path.isdir(target_dir):
        raise RuntimeError(f"Patch target is missing: {target_dir}")

    try:
        subprocess.check_call(
            ["git", "apply", "--ignore-space-change", "--check", "--reverse", patch_file],
            cwd=target_dir,
            stderr=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
        )
        print(f"{patch_name} patch already applied.")
        return
    except subprocess.CalledProcessError:
        pass

    try:
        subprocess.check_call(
            ["git", "apply", "--ignore-space-change", "--check", patch_file],
            cwd=target_dir,
        )
        subprocess.check_call(
            ["git", "apply", "--ignore-space-change", patch_file], cwd=target_dir
        )
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(
            f"Failed to apply required {patch_name} patch. "
            "Ensure patch files are checked out with LF line endings and the "
            "rive-runtime submodule matches the pinned revision."
        ) from exc
    print(f"{patch_name} patch applied successfully.")

rive_runtime_dir = "third-party/rive-runtime"

apply_patch("src/patches/opengl_texture_support.patch", rive_runtime_dir, "OpenGL texture support")

apply_patch("src/patches/d3d12_external_texture_state.patch", rive_runtime_dir, "D3D12 external texture state")

apply_patch("src/patches/texture_header_support.patch", rive_runtime_dir, "Complete GPU texture type")

apply_patch("src/patches/text_input_composition.patch", rive_runtime_dir, "Text input IME composition")

rive_lib, rive_env = SConscript("third-party/SConscript.rive", exports="env")

extension_env = rive_env.Clone()

sources = Glob("src/*.cpp")
sources += Glob("src/renderer/*.cpp")
sources += Glob("src/patches/*.cpp")
sources += Glob("src/resources/*.cpp")
sources += Glob("src/scene/*.cpp")
sources += Glob("src/editor/*.cpp")
if extension_env["platform"] == "macos":
    sources += Glob("src/*.mm")
    sources += Glob("src/renderer/*.mm")

def _basename(node):
    try:
        return os.path.basename(str(node))
    except Exception:
        return str(node)

excluded_sources = {"render_context_gl_impl.cpp", "render_buffer_gl_impl.cpp"}
if extension_env["platform"] != "windows":
    excluded_sources.update({"rive_renderer_d3d12.cpp", "rive_renderer_d3d12.mm", "rive_renderer_d3d12.c"})

filtered_sources = []
for source in sources:
    name = _basename(source)
    if name in excluded_sources:
        print(f"RIVE: Excluding {name}")
        continue
    filtered_sources.append(source)
sources = filtered_sources

extension_env.Append(CPPPATH=[
    os.path.abspath("src/"),
])

if extension_env["platform"] == "macos":
    extension_env.Append(LINKFLAGS=[
        "-framework", "Metal",
        "-framework", "Foundation",
        "-framework", "QuartzCore",
        "-framework", "AppKit",
        "-framework", "CoreGraphics",
        "-framework", "CoreText",
        "-framework", "AudioToolbox"
    ])
elif extension_env["platform"] == "android":
    extension_env.Append(LIBS=["log", "android", "vulkan", "EGL", "GLESv3"])
    extension_env.Append(LINKFLAGS=["-Wl,-z,max-page-size=16384"])

extension_env.Append(LIBS=[rive_lib])

(extension_path,) = glob("project/addons/*/*.gdextension")

addon_path = Path(extension_path).parent

project_name = Path(extension_path).stem

scons_cache_path = os.environ.get("SCONS_CACHE")
if scons_cache_path != None:
    CacheDir(scons_cache_path)
    print("Scons cache enabled... (path: '" + scons_cache_path + "')")

debug_or_release = "release" if extension_env["target"] == "template_release" else "debug"
if extension_env["platform"] == "macos":
    library = extension_env.SharedLibrary(
        "{0}/bin/lib{1}.{2}.{3}.framework/{1}.{2}.{3}".format(
            addon_path,
            project_name,
            extension_env["platform"],
            debug_or_release,
        ),
        source=sources,
    )
else:
    library = extension_env.SharedLibrary(
        "{}/bin/lib{}.{}.{}.{}{}".format(
            addon_path,
            project_name,
            extension_env["platform"],
            debug_or_release,
            extension_env["arch"],
            extension_env["SHLIBSUFFIX"],
        ),
        source=sources,
    )

Default(library)
