#!/usr/bin/env python
import os
from glob import glob
from pathlib import Path

# TODO: Do not copy environment after godot-cpp/test is updated <https://github.com/godotengine/godot-cpp/blob/master/test/SConstruct>.
env = SConscript("third-party/godot-cpp/SConstruct")

# --- Rive Setup ---
rive_dir = "third-party/rive-runtime/"
env.Append(CPPPATH=[
    "src/",
    rive_dir + "include",
    rive_dir,
    rive_dir + "renderer",
    rive_dir + "renderer/src",
    rive_dir + "renderer/include"
])
env.Append(CPPDEFINES=['_RIVE_INTERNAL_'])

# Platform specific flags
if env["platform"] == "macos":
    env.Append(FRAMEWORKS=["Metal", "Foundation", "QuartzCore", "AppKit"])
    env.Append(LINKFLAGS=["-framework", "Metal", "-framework", "Foundation", "-framework", "QuartzCore", "-framework", "AppKit"])
    env.Append(CPPDEFINES=["RIVE_METAL"])

if env["platform"] in ["windows", "linuxbsd", "android"]:
    env.Append(CPPDEFINES=["RIVE_VULKAN", "RIVE_UPSTREAM_VULKAN_IMPL", "VULKAN_ENABLED"])

if env["platform"] == "windows":
    env.Append(CPPDEFINES=["RIVE_D3D12", "D3D12_ENABLED"])

# Collect Rive sources
rive_sources = []
dirs_to_scan = [
    os.path.join(rive_dir, 'src'),
    os.path.join(rive_dir, 'renderer', 'src')
]

exclude_dir_names = {'d3d', 'd3d11', 'd3d12', 'vulkan', 'webgpu', 'dawn', 'gl', 'android', 'ios'}
if env["platform"] != "macos":
    exclude_dir_names.add('metal')

if env["platform"] in ["windows", "linuxbsd", "android"]:
    if 'vulkan' in exclude_dir_names: exclude_dir_names.remove('vulkan')

if env["platform"] == "windows":
    if 'd3d12' in exclude_dir_names: exclude_dir_names.remove('d3d12')

for d in dirs_to_scan:
    if not os.path.exists(d): continue
    for root, dirs, files in os.walk(d):
        dirs[:] = [x for x in dirs if x not in exclude_dir_names]
        for f in files:
            if f.endswith('.cpp') or (f.endswith('.mm') and env["platform"] == "macos"):
                rive_sources.append(os.path.join(root, f))

# Add source files.
sources = Glob("src/*.cpp")
if env["platform"] == "macos":
    sources += Glob("src/*.mm")

sources += rive_sources

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
debug_or_release = "release" if env["target"] == "template_release" else "debug"
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{0}/bin/lib{1}.{2}.{3}.framework/{1}.{2}.{3}".format(
            addon_path,
            project_name,
            env["platform"],
            debug_or_release,
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "{}/bin/lib{}.{}.{}.{}{}".format(
            addon_path,
            project_name,
            env["platform"],
            debug_or_release,
            env["arch"],
            env["SHLIBSUFFIX"],
        ),
        source=sources,
    )

Default(library)
