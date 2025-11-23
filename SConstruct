#!/usr/bin/env python
import os
from glob import glob
from pathlib import Path

# TODO: Do not copy environment after godot-cpp/test is updated <https://github.com/godotengine/godot-cpp/blob/master/test/SConstruct>.
env = SConscript("third-party/godot-cpp/SConstruct")

# --- Rive Setup ---
rive_dir = os.path.abspath("third-party/rive-runtime/")
env.Append(CPPPATH=[
    os.path.abspath("src/"),
    os.path.join(rive_dir, "include"),
    rive_dir,
    os.path.join(rive_dir, "renderer"),
    os.path.join(rive_dir, "renderer", "src"),
    os.path.join(rive_dir, "renderer", "include")
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
        vulkan_sdk = os.environ.get("VULKAN_SDK")
        if vulkan_sdk:
            env.Append(CPPPATH=[
                os.path.join(vulkan_sdk, "Include"),
                os.path.join(vulkan_sdk, "Include", "vma")
            ])
            env.Append(LIBPATH=[os.path.join(vulkan_sdk, "Lib")])
            env.Append(LIBS=["vulkan-1"])

if env["platform"] == "windows":
    env.Append(CPPDEFINES=["RIVE_D3D12", "D3D12_ENABLED"])
    env.Append(CXXFLAGS=["/std:c++20"])
    env.Append(LIBS=["d3d12", "dxgi", "dxguid", "d3dcompiler"])

    # Prioritize Godot build deps for D3DX12
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        godot_d3d_deps = os.path.join(local_app_data, "Godot", "build_deps", "agility_sdk", "build", "native", "include", "d3dx12")
        if os.path.exists(godot_d3d_deps):
            print(f"RIVE: Using Godot D3D deps from {godot_d3d_deps}")
            env.Append(CPPPATH=[godot_d3d_deps])

    # Add support for external DirectX Headers via env var
    # Matches logic in rive/SCsub but using env vars instead of local paths
    dx_headers = os.environ.get("DIRECTX_HEADERS_PATH")
    if dx_headers:
        print(f"RIVE: Using DirectX Headers from {dx_headers}")
        # Assuming the user points to the root of the DirectX-Headers repo or install
        # We add likely subpaths to cover different structures
        env.Append(CPPPATH=[
            dx_headers,
            os.path.join(dx_headers, "include"),
            os.path.join(dx_headers, "include", "directx"),
            os.path.join(dx_headers, "include", "dxguids")
        ])
    
    # Add support for D3D12MA via env var
    d3d12ma_path = os.environ.get("D3D12MA_PATH")
    if d3d12ma_path:
        print(f"RIVE: Using D3D12MA from {d3d12ma_path}")
        env.Append(CPPPATH=[d3d12ma_path])

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
    if 'd3d' in exclude_dir_names: exclude_dir_names.remove('d3d')

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
