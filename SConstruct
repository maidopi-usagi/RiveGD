#!/usr/bin/env python
import os
from glob import glob
from pathlib import Path

# TODO: Do not copy environment after godot-cpp/test is updated <https://github.com/godotengine/godot-cpp/blob/master/test/SConstruct>.
env = SConscript("third-party/godot-cpp/SConstruct")

# --- Rive Setup ---
rive_dir = os.path.abspath("third-party/rive-runtime/")

# --- Preprocess Generated Shaders (Fix C2026) ---
import tempfile
import shutil
import re

def preprocess_generated_shaders(env, rive_dir):
    # Target directory containing the problematic headers
    generated_shaders_dir = os.path.join(rive_dir, "renderer", "include", "generated", "shaders")
    if not os.path.exists(generated_shaders_dir):
        return

    # Create a temporary directory for the modified headers
    # We use a consistent path in temp to avoid clutter, but ensure it's fresh
    temp_base = os.path.join(tempfile.gettempdir(), "rivegd_shader_fix")
    if os.path.exists(temp_base):
        try:
            shutil.rmtree(temp_base)
        except OSError:
            pass # Might be in use, but we try our best
    
    # We need to preserve the include structure: generated/shaders/...
    # So if we add temp_base to CPPPATH, we need the files to be in temp_base/generated/shaders
    target_dir = os.path.join(temp_base, "generated", "shaders")
    
    print(f"RIVE: Pre-processing generated shaders to {target_dir} to fix C2026...")
    
    def convert_raw_string_to_hex_array(content):
        # Regex to match: const char name[] = R"delimiter(content)delimiter";
        # We capture the name, the delimiter, and the content.
        # We use re.DOTALL so . matches newlines.
        pattern = r'(?:static\s+)?const\s+char\s+(\w+)\[\]\s*=\s*R"([^"(]*)\((.*?)\)\2";'
        
        def replacer(match):
            name = match.group(1)
            body = match.group(3)
            
            # Convert string to bytes (UTF-8)
            bytes_data = body.encode('utf-8')
            
            # Convert to hex strings
            hex_values = [f"0x{b:02x}" for b in bytes_data]
            hex_values.append("0x00") # Null terminator
            
            # Format with line breaks to be readable/safe
            lines = []
            chunk_size = 16 # 16 bytes per line
            for i in range(0, len(hex_values), chunk_size):
                lines.append(", ".join(hex_values[i:i+chunk_size]))
            
            array_body = ",\n".join(lines)
            
            # Reconstruct as char array initialization
            return f"const char {name}[] = {{\n{array_body}\n}};"

        return re.sub(pattern, replacer, content, flags=re.DOTALL)

    for root, dirs, files in os.walk(generated_shaders_dir):
        rel_path = os.path.relpath(root, generated_shaders_dir)
        dest_root = os.path.join(target_dir, rel_path)
        if not os.path.exists(dest_root):
            os.makedirs(dest_root)
            
        for f in files:
            if f.endswith(".h") or f.endswith(".hpp") or f.endswith(".glsl"):
                src_file = os.path.join(root, f)
                dst_file = os.path.join(dest_root, f)
                
                try:
                    with open(src_file, 'r', encoding='utf-8', errors='ignore') as fin:
                        content = fin.read()
                    
                    # Apply the conversion
                    new_content = convert_raw_string_to_hex_array(content)
                    
                    # If no change (no raw string found), we still write it out 
                    # because we need the file to exist in the include path override.
                    
                    with open(dst_file, 'w', encoding='utf-8') as fout:
                        fout.write(new_content)
                except Exception as e:
                    print(f"Warning: Failed to process {src_file}: {e}")

    # Prepend the temp directory to include paths so modified files are found first
    # Note: We prepend 'temp_base' because the includes are likely <generated/shaders/file.h>
    # and our files are at temp_base/generated/shaders/file.h
    # However, rive_dir/renderer/include contains 'generated'.
    # So if we include "generated/shaders/...", we need to add temp_base to path?
    # No, if the original path is .../renderer/include, and it contains generated/shaders/...
    # Then #include "generated/shaders/..." works.
    # So we need to add temp_base to CPPPATH.
    env.Prepend(CPPPATH=[temp_base])

preprocess_generated_shaders(env, rive_dir)

env.Append(CPPPATH=[
    os.path.abspath("src/"),
    os.path.join(rive_dir, "include"),
    rive_dir,
    os.path.join(rive_dir, "renderer"),
    os.path.join(rive_dir, "renderer", "src"),
    os.path.join(rive_dir, "renderer", "include"),
    os.path.join(rive_dir, "renderer", "glad", "include"),
    os.path.join(rive_dir, "renderer", "glad")
])
env.Append(CPPDEFINES=['_RIVE_INTERNAL_', 'RIVE_DESKTOP_GL'])

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
    env.Append(LIBS=["d3d12", "dxgi", "dxguid", "d3dcompiler", "opengl32"])

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

exclude_dir_names = {'d3d', 'd3d11', 'd3d12', 'vulkan', 'webgpu', 'dawn', 'android', 'ios'}
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
                # Exclude EGL dependent file on non-Android platforms
                if f == "load_gles_extensions.cpp" and env["platform"] != "android":
                    continue
                # Exclude render_context_gl_impl.cpp as we use a patched version in src/patches
                if f == "render_context_gl_impl.cpp":
                    continue
                # Exclude render_buffer_gl_impl.cpp as we use a patched version in src/patches
                if f == "render_buffer_gl_impl.cpp":
                    continue
                rive_sources.append(os.path.join(root, f))

# Add source files.
sources = Glob("src/*.cpp")
sources += Glob("src/patches/*.cpp")
if env["platform"] == "macos":
    sources += Glob("src/*.mm")

sources += rive_sources
sources.append(os.path.join(rive_dir, "renderer", "glad", "glad_custom.c"))
sources.append(os.path.join(rive_dir, "renderer", "glad", "src", "gles2.c"))
sources.append(os.path.join(rive_dir, "renderer", "glad", "src", "egl.c"))

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
