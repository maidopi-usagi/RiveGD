#!/usr/bin/env python
import os
from glob import glob
from pathlib import Path

# TODO: Do not copy environment after godot-cpp/test is updated <https://github.com/godotengine/godot-cpp/blob/master/test/SConstruct>.
env = SConscript("third-party/godot-cpp/SConstruct")

# Apply patches
import subprocess

def apply_patch(patch_path, target_dir, patch_name):
    print(f"Checking for {patch_name} patch...")
    patch_file = os.path.abspath(patch_path)
    target_dir = os.path.abspath(target_dir)
    
    if os.path.exists(patch_file) and os.path.exists(target_dir):
        try:
            # Check if patch is already applied (reverse check succeeds if applied)
            subprocess.check_call(["git", "apply", "--check", "--reverse", patch_file], cwd=target_dir, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
            print(f"{patch_name} patch already applied.")
        except subprocess.CalledProcessError:
            # Not applied (or conflicts), try to apply
            print(f"Applying {patch_name} patch...")
            try:
                subprocess.check_call(["git", "apply", patch_file], cwd=target_dir)
                print(f"{patch_name} patch applied successfully.")
            except subprocess.CalledProcessError as e:
                print(f"Warning: Failed to apply {patch_name} patch: {e}")
                print(f"Build may fail if {patch_name} support is missing.")

rive_runtime_dir = "third-party/rive-runtime"

if env["platform"] == "macos":
    apply_patch("src/patches/metal_texture_support.patch", rive_runtime_dir, "Metal texture support")

# Apply OpenGL patch for all platforms (or specifically non-macOS if desired, but harmless if applied)
# This patch fixes OpenGL compatibility issues (SSBOs, GLES version)
apply_patch("src/patches/opengl_texture_support.patch", rive_runtime_dir, "OpenGL texture support")

# Build Rive Runtime
# We pass the godot-cpp environment so it inherits platform flags/compilers
rive_lib, rive_env = SConscript("third-party/SConscript.rive", exports="env")

# --- RiveGD Build ---

# Use the environment configured by Rive (includes paths, defines, etc.)
# This environment also has the Godot configuration because it was cloned from 'env'
# We clone it again just to be safe/clean, though not strictly necessary if we are done with rive_env
extension_env = rive_env.Clone()

# Add RiveGD sources
sources = Glob("src/*.cpp")
sources += Glob("src/renderer/*.cpp")
sources += Glob("src/patches/*.cpp")
sources += Glob("src/resources/*.cpp")
sources += Glob("src/scene/*.cpp")
sources += Glob("src/editor/*.cpp")
if extension_env["platform"] == "macos":
    sources += Glob("src/*.mm")
    sources += Glob("src/renderer/*.mm")

# On macOS exclude D3D12 renderer source(s) that live in the project `src/` directory.
# Also exclude render_context_gl_impl.cpp and render_buffer_gl_impl.cpp from src/patches/ as they are now applied as patches to rive-runtime
def _basename(node):
    try:
        return os.path.basename(str(node))
    except Exception:
        return str(node)

excludes = {"render_context_gl_impl.cpp", "render_buffer_gl_impl.cpp"}
if extension_env["platform"] == "macos":
    excludes.update({"rive_renderer_d3d12.cpp", "rive_renderer_d3d12.mm", "rive_renderer_d3d12.c"})

filtered_sources = []
for s in sources:
    name = _basename(s)
    if name in excludes:
        print(f"RIVE: Excluding {name}")
        continue
    filtered_sources.append(s)
sources = filtered_sources

# Include paths for RiveGD sources
# (Most are already in extension_env from rive-runtime SConscript, but we need src/)
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

# Link against Rive Runtime
extension_env.Append(LIBS=[rive_lib])

# Link against Godot CPP
# godot-cpp SConstruct usually adds the library to LIBS or LIBPATH in the returned env.
# But just in case, we ensure we are linking to it.
# The 'env' returned by godot-cpp SConstruct usually has the library added to LIBS.
# Since rive_env was cloned from env, it should have it too.

# Find gdextension path even if the directory or extension is renamed (e.g. project/addons/example/example.gdextension).
(extension_path,) = glob("project/addons/*/*.gdextension")

# Get the addon path (e.g. project/addons/example).
addon_path = Path(extension_path).parent

# Get the project name from the gdextension file (e.g. example).
project_name = Path(extension_path).stem

scons_cache_path = os.environ.get("SCONS_CACHE")
if scons_cache_path != None:
    CacheDir(scons_cache_path)
    print("Scons cache enabled... (path: '" + scons_cache_path + "')")

# Create the library target (e.g. libexample.linux.debug.x86_64.so).
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
