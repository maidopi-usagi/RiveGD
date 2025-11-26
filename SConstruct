#!/usr/bin/env python
import os
from glob import glob
from pathlib import Path

# TODO: Do not copy environment after godot-cpp/test is updated <https://github.com/godotengine/godot-cpp/blob/master/test/SConstruct>.
env = SConscript("third-party/godot-cpp/SConstruct")

# --- Rive Setup ---
rive_dir = os.path.abspath("third-party/rive-runtime/")

# --- Install Dependencies (HarfBuzz & SheenBidi) ---
import urllib.request
import zipfile
import io
import shutil

deps_dir = os.path.join(rive_dir, "dependencies")
harfbuzz_dir = os.path.join(deps_dir, "harfbuzz")
sheenbidi_dir = os.path.join(deps_dir, "sheenbidi")
yoga_dir = os.path.join(deps_dir, "yoga")
miniaudio_dir = os.path.join(deps_dir, "miniaudio")
libpng_dir = os.path.join(deps_dir, "libpng")
zlib_dir = os.path.join(deps_dir, "zlib")
libjpeg_dir = os.path.join(deps_dir, "libjpeg")
libwebp_dir = os.path.join(deps_dir, "libwebp")

def install_dependency(name, url, target_dir):
    if os.path.exists(target_dir):
        return

    print(f"RIVE: Downloading {name} from {url}...")
    try:
        with urllib.request.urlopen(url) as response:
            data = response.read()
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                # Find the root folder in the zip
                root = z.namelist()[0].split('/')[0]
                # Extract to dependencies dir
                z.extractall(deps_dir)
                # Rename to target_dir
                extracted_path = os.path.join(deps_dir, root)
                if os.path.exists(target_dir):
                    shutil.rmtree(target_dir)
                os.rename(extracted_path, target_dir)
                print(f"RIVE: Installed {name} to {target_dir}")
    except Exception as e:
        print(f"RIVE: Failed to install {name}: {e}")
        Exit(1)

install_dependency("HarfBuzz", "https://github.com/rive-app/harfbuzz/archive/refs/heads/rive_10.1.0.zip", harfbuzz_dir)
install_dependency("SheenBidi", "https://github.com/Tehreer/SheenBidi/archive/refs/tags/v2.6.zip", sheenbidi_dir)
install_dependency("Yoga", "https://github.com/rive-app/yoga/archive/refs/heads/rive_changes_v2_0_1_2.zip", yoga_dir)
install_dependency("Miniaudio", "https://github.com/rive-app/miniaudio/archive/refs/heads/rive_changes_5.zip", miniaudio_dir)
install_dependency("LibPNG", "https://github.com/glennrp/libpng/archive/refs/heads/libpng16.zip", libpng_dir)
install_dependency("ZLib", "https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip", zlib_dir)
install_dependency("LibJPEG", "https://github.com/rive-app/libjpeg/archive/refs/tags/v9f.zip", libjpeg_dir)
install_dependency("LibWebP", "https://github.com/webmproject/libwebp/archive/refs/tags/v1.4.0.zip", libwebp_dir)

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

# --- Generate pnglibconf.h ---
pnglibconf_h = os.path.join(rive_dir, "include", "libpng", "pnglibconf.h")
if not os.path.exists(pnglibconf_h):
    print("RIVE: Generating pnglibconf.h...")
    os.makedirs(os.path.dirname(pnglibconf_h), exist_ok=True)
    shutil.copy(os.path.join(libpng_dir, "scripts", "pnglibconf.h.prebuilt"), pnglibconf_h)

env.Append(CPPPATH=[
    os.path.abspath("src/"),
    os.path.join(rive_dir, "include"),
    rive_dir,
    os.path.join(rive_dir, "renderer"),
    os.path.join(rive_dir, "renderer", "src"),
    os.path.join(rive_dir, "renderer", "include"),
    os.path.join(rive_dir, "renderer", "glad", "include"),
    os.path.join(rive_dir, "renderer", "glad"),
    os.path.join(harfbuzz_dir, "src"),
    deps_dir, # For rive_harfbuzz_renames.h
    os.path.join(sheenbidi_dir, "Headers"),
    yoga_dir,
    miniaudio_dir,
    libpng_dir,
    zlib_dir,
    os.path.join(rive_dir, "include", "libpng"), # For generated pnglibconf.h
    libjpeg_dir,
    libwebp_dir,
    os.path.join(libwebp_dir, "src"),
    os.path.join(rive_dir, "decoders", "include")
])
env.Append(CPPDEFINES=[
    '_RIVE_INTERNAL_', 'RIVE_DESKTOP_GL', 'WITH_RIVE_TEXT', 'WITH_RIVE_TOOLS',
    'WITH_RIVE_LAYOUT', 'YOGA_EXPORT=',
    'WITH_RIVE_AUDIO', 'MA_NO_RESOURCE_MANAGER',
    'RIVE_PNG', 'RIVE_JPEG', 'RIVE_WEBP', 'RIVE_DECODERS',
    'HB_ONLY_ONE_SHAPER',
    'HAVE_OT',
    'HB_NO_FALLBACK_SHAPE',
    'HB_NO_WIN1256',
    'HB_NO_EXTERN_HELPERS',
    'HB_DISABLE_DEPRECATED',
    'HB_NO_COLOR',
    'HB_NO_BITMAP',
    'HB_NO_BUFFER_SERIALIZE',
    'HB_NO_BUFFER_VERIFY',
    'HB_NO_BUFFER_MESSAGE',
    'HB_NO_SETLOCALE',
    'HB_NO_VERTICAL',
    'HB_NO_LAYOUT_COLLECT_GLYPHS',
    'HB_NO_LAYOUT_RARELY_USED',
    'HB_NO_LAYOUT_UNUSED',
    'HB_NO_OT_FONT_GLYPH_NAMES',
    'HB_NO_PAINT',
    'HB_NO_MMAP',
    'HB_NO_META',
    'SB_CONFIG_UNITY'
])

# Force include rive_harfbuzz_renames.h
if env["platform"] == "windows":
    env.Append(CCFLAGS=['/FIrive_harfbuzz_renames.h'])
    env.Append(CCFLAGS=['/FIrive_yoga_renames.h'])
    env.Append(CCFLAGS=['/FIrive_libjpeg_renames.h'])
    env.Append(CCFLAGS=['/FIrive_png_renames.h'])
else:
    env.Append(CCFLAGS=['-include', 'rive_harfbuzz_renames.h'])
    env.Append(CCFLAGS=['-include', 'rive_yoga_renames.h'])
    env.Append(CCFLAGS=['-include', 'rive_libjpeg_renames.h'])
    env.Append(CCFLAGS=['-include', 'rive_png_renames.h'])

# Platform specific flags
if env["platform"] == "macos":
    env.Append(FRAMEWORKS=["Metal", "Foundation", "QuartzCore", "AppKit"])
    env.Append(LINKFLAGS=["-framework", "Metal", "-framework", "Foundation", "-framework", "QuartzCore", "-framework", "AppKit"])
    env.Append(CPPDEFINES=["RIVE_METAL"])
elif env["platform"] in ["windows", "linuxbsd", "android"]:
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

        # D3D12 configuration (Windows only)
        env.Append(CPPDEFINES=["RIVE_D3D12", "D3D12_ENABLED", "_USE_MATH_DEFINES", "NOMINMAX"])
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
else:
    # macOS: explicitly exclude all D3D directories
    exclude_dir_names.update({'d3d', 'd3d11', 'd3d12'})

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

# On macOS exclude D3D12 renderer source(s) that live in the project `src/` directory.
# This prevents compilation of files that include DirectX headers on macOS.
if env["platform"] == "macos":
    def _basename(node):
        try:
            return os.path.basename(str(node))
        except Exception:
            return str(node)

    d3d_excludes = {"rive_renderer_d3d12.cpp", "rive_renderer_d3d12.mm", "rive_renderer_d3d12.c"}
    filtered_sources = []
    for s in sources:
        name = _basename(s)
        if name in d3d_excludes:
            print(f"RIVE: Excluding {name} on macOS")
            continue
        filtered_sources.append(s)
    sources = filtered_sources

sources += rive_sources
sources.append(os.path.join(rive_dir, "renderer", "glad", "glad_custom.c"))
sources.append(os.path.join(rive_dir, "renderer", "glad", "src", "gles2.c"))
sources.append(os.path.join(rive_dir, "renderer", "glad", "src", "egl.c"))

# Add HarfBuzz and SheenBidi sources
harfbuzz_sources = Glob(os.path.join(harfbuzz_dir, "src", "*.cc"))
harfbuzz_sources.extend(Glob(os.path.join(harfbuzz_dir, "src", "graph", "*.cc")))
harfbuzz_sources.extend(Glob(os.path.join(harfbuzz_dir, "src", "OT", "Var", "VARC", "*.cc")))

filtered_hb_sources = []
excluded_hb_files = {
    "harfbuzz.cc",
    "harfbuzz-subset.cc",
    "main.cc",
    "hb-icu.cc",
    "hb-glib.cc",
    "hb-gobject-structs.cc",
    "hb-cairo.cc",
    "hb-cairo-utils.cc",
    "hb-graphite2.cc",
    "hb-uniscribe.cc",
    "hb-gdi.cc",
    "hb-directwrite.cc",
    "hb-wasm-api.cc",
    "hb-wasm-shape.cc",
    "test-classdef-graph.cc", # Exclude test file in graph/
}

for s in harfbuzz_sources:
    name = os.path.basename(str(s))
    if name in excluded_hb_files:
        continue
    if "hb-coretext" in name:
        if env["platform"] != "macos":
            continue
    if "test" in name: 
        continue
    filtered_hb_sources.append(s)

if env["platform"] == "macos":
     env.Append(CPPDEFINES=['HAVE_CORETEXT'])

sources.extend(filtered_hb_sources)
sources.append(os.path.join(sheenbidi_dir, "Source", "SheenBidi.c"))

# Add Yoga sources
yoga_sources = [
    os.path.join(yoga_dir, "yoga", "Utils.cpp"),
    os.path.join(yoga_dir, "yoga", "YGConfig.cpp"),
    os.path.join(yoga_dir, "yoga", "YGLayout.cpp"),
    os.path.join(yoga_dir, "yoga", "YGEnums.cpp"),
    os.path.join(yoga_dir, "yoga", "YGNodePrint.cpp"),
    os.path.join(yoga_dir, "yoga", "YGNode.cpp"),
    os.path.join(yoga_dir, "yoga", "YGValue.cpp"),
    os.path.join(yoga_dir, "yoga", "YGStyle.cpp"),
    os.path.join(yoga_dir, "yoga", "Yoga.cpp"),
    os.path.join(yoga_dir, "yoga", "event", "event.cpp"),
    os.path.join(yoga_dir, "yoga", "log.cpp"),
]
sources.extend(yoga_sources)

# Add Miniaudio sources
if env["platform"] == "ios":
    sources.append(os.path.join(miniaudio_dir, "miniaudio.m"))
else:
    sources.append(os.path.join(miniaudio_dir, "miniaudio.c"))

# Add LibPNG sources
libpng_sources = [
    "png.c", "pngerror.c", "pngget.c", "pngmem.c", "pngpread.c", "pngread.c",
    "pngrio.c", "pngrtran.c", "pngrutil.c", "pngset.c", "pngtrans.c", "pngwio.c",
    "pngwrite.c", "pngwtran.c", "pngwutil.c",
    "arm/arm_init.c", "arm/filter_neon_intrinsics.c", "arm/palette_neon_intrinsics.c"
]
for s in libpng_sources:
    sources.append(os.path.join(libpng_dir, s))

# Add ZLib sources
zlib_sources = [
    "adler32.c", "compress.c", "crc32.c", "deflate.c", "gzclose.c", "gzlib.c",
    "gzread.c", "gzwrite.c", "infback.c", "inffast.c", "inftrees.c", "trees.c",
    "uncompr.c", "zutil.c", "inflate.c"
]
for s in zlib_sources:
    sources.append(os.path.join(zlib_dir, s))

# Add LibJPEG sources
libjpeg_sources = [
    "jaricom.c", "jcapimin.c", "jcapistd.c", "jcarith.c", "jccoefct.c", "jccolor.c",
    "jcdctmgr.c", "jchuff.c", "jcinit.c", "jcmainct.c", "jcmarker.c", "jcmaster.c",
    "jcomapi.c", "jcparam.c", "jcprepct.c", "jcsample.c", "jctrans.c", "jdapimin.c",
    "jdapistd.c", "jdarith.c", "jdatadst.c", "jdatasrc.c", "jdcoefct.c", "jdcolor.c",
    "jddctmgr.c", "jdhuff.c", "jdinput.c", "jdmainct.c", "jdmarker.c", "jdmaster.c",
    "jdmerge.c", "jdpostct.c", "jdsample.c", "jdtrans.c", "jerror.c", "jfdctflt.c",
    "jfdctfst.c", "jfdctint.c", "jidctflt.c", "jidctfst.c", "jidctint.c", "jquant1.c",
    "jquant2.c", "jutils.c", "jmemmgr.c", "jmemansi.c"
]
for s in libjpeg_sources:
    sources.append(os.path.join(libjpeg_dir, s))

# Add LibWebP sources
libwebp_sources = [
    "src/dsp/alpha_processing.c", "src/dsp/cpu.c", "src/dsp/dec.c", "src/dsp/dec_clip_tables.c",
    "src/dsp/filters.c", "src/dsp/lossless.c", "src/dsp/rescaler.c", "src/dsp/upsampling.c",
    "src/dsp/yuv.c", "src/dsp/cost.c", "src/dsp/enc.c", "src/dsp/lossless_enc.c", "src/dsp/ssim.c",
    "src/dec/alpha_dec.c", "src/dec/buffer_dec.c", "src/dec/frame_dec.c", "src/dec/idec_dec.c",
    "src/dec/io_dec.c", "src/dec/quant_dec.c", "src/dec/tree_dec.c", "src/dec/vp8_dec.c",
    "src/dec/vp8l_dec.c", "src/dec/webp_dec.c",
    "src/dsp/alpha_processing_sse41.c", "src/dsp/dec_sse41.c", "src/dsp/lossless_sse41.c",
    "src/dsp/upsampling_sse41.c", "src/dsp/yuv_sse41.c",
    "src/dsp/alpha_processing_sse2.c", "src/dsp/dec_sse2.c", "src/dsp/filters_sse2.c",
    "src/dsp/lossless_sse2.c", "src/dsp/rescaler_sse2.c", "src/dsp/upsampling_sse2.c",
    "src/dsp/yuv_sse2.c",
    "src/dsp/alpha_processing_neon.c", "src/dsp/dec_neon.c", "src/dsp/filters_neon.c",
    "src/dsp/lossless_neon.c", "src/dsp/rescaler_neon.c", "src/dsp/upsampling_neon.c",
    "src/dsp/yuv_neon.c",
    "src/dsp/dec_msa.c", "src/dsp/filters_msa.c", "src/dsp/lossless_msa.c", "src/dsp/rescaler_msa.c",
    "src/dsp/upsampling_msa.c",
    "src/dsp/dec_mips32.c", "src/dsp/rescaler_mips32.c", "src/dsp/yuv_mips32.c",
    "src/dsp/alpha_processing_mips_dsp_r2.c", "src/dsp/dec_mips_dsp_r2.c", "src/dsp/filters_mips_dsp_r2.c",
    "src/dsp/lossless_mips_dsp_r2.c", "src/dsp/rescaler_mips_dsp_r2.c", "src/dsp/upsampling_mips_dsp_r2.c",
    "src/dsp/yuv_mips_dsp_r2.c",
    "src/dsp/cost_sse2.c", "src/dsp/enc_sse2.c", "src/dsp/lossless_enc_sse2.c", "src/dsp/ssim_sse2.c",
    "src/dsp/enc_sse41.c", "src/dsp/lossless_enc_sse41.c",
    "src/dsp/cost_neon.c", "src/dsp/enc_neon.c", "src/dsp/lossless_enc_neon.c",
    "src/dsp/enc_msa.c", "src/dsp/lossless_enc_msa.c",
    "src/dsp/cost_mips32.c", "src/dsp/enc_mips32.c", "src/dsp/lossless_enc_mips32.c",
    "src/dsp/cost_mips_dsp_r2.c", "src/dsp/enc_mips_dsp_r2.c", "src/dsp/lossless_enc_mips_dsp_r2.c",
    "src/utils/bit_reader_utils.c", "src/utils/color_cache_utils.c", "src/utils/filters_utils.c",
    "src/utils/huffman_utils.c", "src/utils/palette.c", "src/utils/quant_levels_dec_utils.c",
    "src/utils/rescaler_utils.c", "src/utils/random_utils.c", "src/utils/thread_utils.c",
    "src/utils/utils.c",
    "src/utils/bit_writer_utils.c", "src/utils/huffman_encode_utils.c", "src/utils/quant_levels_utils.c",
    "src/demux/anim_decode.c", "src/demux/demux.c"
]
for s in libwebp_sources:
    sources.append(os.path.join(libwebp_dir, s))

# Add Rive Decoders sources
rive_decoders_dir = os.path.join(rive_dir, "decoders", "src")
rive_decoders_sources = [
    "bitmap_decoder.cpp",
    "bitmap_decoder_thirdparty.cpp",
    "decode_png.cpp",
    "decode_jpeg.cpp",
    "decode_webp.cpp"
]
for s in rive_decoders_sources:
    sources.append(os.path.join(rive_decoders_dir, s))

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
