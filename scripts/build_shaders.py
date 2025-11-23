import os
import subprocess
import sys
import glob
import shutil

def run(cmd):
    print(f"Running: {' '.join(cmd)}")
    subprocess.check_call(cmd)

def ensure_dir(d):
    if not os.path.exists(d):
        os.makedirs(d)

def find_tool(name):
    return shutil.which(name)

def find_fxc():
    # Check if fxc is in path
    try:
        subprocess.check_call(['fxc', '/?'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return 'fxc'
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    
    # Try common locations
    possible_paths = [
        r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe",
        r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\fxc.exe",
        r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.20348.0\x64\fxc.exe",
        r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\fxc.exe",
        r"C:\Program Files (x86)\Windows Kits\10\bin\x64\fxc.exe"
    ]
    for p in possible_paths:
        if os.path.exists(p):
            print(f"Found fxc at {p}")
            return p
            
    print("Error: fxc not found. Please ensure Windows SDK is installed.")
    return None

def generate_d3d_shaders(shaders_dir, out_dir):
    print(f"Generating D3D shaders from {shaders_dir} to {out_dir}")
    
    d3d_out = os.path.join(out_dir, 'd3d')
    ensure_dir(d3d_out)
    
    fxc_cmd = find_fxc()
    if not fxc_cmd:
        print("fxc not found, skipping D3D shader generation.")
        return
        
    d3d_src_dir = os.path.join(shaders_dir, 'd3d')
    hlsl_files = glob.glob(os.path.join(d3d_src_dir, '*.hlsl'))
    
    for hlsl in hlsl_files:
        filename = os.path.basename(hlsl)
        name_no_ext = os.path.splitext(filename)[0]
        
        # Vertex Shader
        vert_out = os.path.join(d3d_out, f"{name_no_ext}.vert.h")
        # Check timestamp
        if os.path.exists(vert_out) and os.path.getmtime(vert_out) > os.path.getmtime(hlsl):
            pass # Up to date
        else:
            cmd_vert = [fxc_cmd, '/D', 'VERTEX', '/I', out_dir, '/Zi', '/T', 'vs_5_0', '/Fh', vert_out, hlsl]
            run(cmd_vert)
        
        # Fragment Shader
        if filename == 'render_atlas.hlsl':
            # render_atlas_stroke.frag.h
            stroke_out = os.path.join(d3d_out, 'render_atlas_stroke.frag.h')
            if not (os.path.exists(stroke_out) and os.path.getmtime(stroke_out) > os.path.getmtime(hlsl)):
                cmd_stroke = [fxc_cmd, '/D', 'FRAGMENT', '/D', 'ATLAS_FEATHERED_STROKE', '/Zi', '/I', out_dir, '/T', 'ps_5_0', '/Fh', stroke_out, hlsl]
                run(cmd_stroke)
            
            # render_atlas_fill.frag.h
            fill_out = os.path.join(d3d_out, 'render_atlas_fill.frag.h')
            if not (os.path.exists(fill_out) and os.path.getmtime(fill_out) > os.path.getmtime(hlsl)):
                cmd_fill = [fxc_cmd, '/D', 'FRAGMENT', '/D', 'ATLAS_FEATHERED_FILL', '/Zi', '/I', out_dir, '/T', 'ps_5_0', '/Fh', fill_out, hlsl]
                run(cmd_fill)
        else:
            frag_out = os.path.join(d3d_out, f"{name_no_ext}.frag.h")
            if not (os.path.exists(frag_out) and os.path.getmtime(frag_out) > os.path.getmtime(hlsl)):
                cmd_frag = [fxc_cmd, '/D', 'FRAGMENT', '/I', out_dir, '/Zi', '/T', 'ps_5_0', '/Fh', frag_out, hlsl]
                run(cmd_frag)

    # Root Signature
    root_sig_src = os.path.join(d3d_src_dir, 'root.sig')
    if os.path.exists(root_sig_src):
        root_sig_out = os.path.join(d3d_out, 'root.sig.h')
        if not (os.path.exists(root_sig_out) and os.path.getmtime(root_sig_out) > os.path.getmtime(root_sig_src)):
            cmd_sig = [fxc_cmd, '/I', out_dir, '/T', 'rootsig_1_1', '/E', 'ROOT_SIG', '/Fh', root_sig_out, root_sig_src]
            run(cmd_sig)

def generate_metal_shaders(shaders_dir, out_dir):
    print(f"Generating Metal shaders from {shaders_dir} to {out_dir}")
    metal_out = os.path.join(out_dir, 'macosx')
    ensure_dir(metal_out)
    
    if not find_tool('xcrun'):
        print("xcrun not found, skipping Metal shader generation.")
        return

    metal_src_dir = os.path.join(shaders_dir, 'metal')
    metal_files = glob.glob(os.path.join(metal_src_dir, '*.metal'))
    
    # Generate draw_combinations.metal
    draw_combinations_out = os.path.join(out_dir, 'draw_combinations.metal')
    gen_draw_comb_script = os.path.join(metal_src_dir, 'generate_draw_combinations.py')
    if os.path.exists(gen_draw_comb_script):
        run([sys.executable, gen_draw_comb_script, draw_combinations_out])
    
    air_files = []
    for metal_file in metal_files:
        filename = os.path.basename(metal_file)
        name_no_ext = os.path.splitext(filename)[0]
        air_out = os.path.join(metal_out, f"{name_no_ext}.air")
        
        # Check timestamp
        if os.path.exists(air_out) and os.path.getmtime(air_out) > os.path.getmtime(metal_file):
             air_files.append(air_out)
             continue

        cmd = [
            'xcrun', '-sdk', 'macosx', 'metal', '-std=macos-metal2.3',
            '-mmacosx-version-min=10.0', '-I', out_dir, '-ffast-math',
            '-ffp-contract=fast', '-fpreserve-invariance', '-fvisibility=hidden',
            '-c', metal_file, '-o', air_out
        ]
        run(cmd)
        air_files.append(air_out)
        
    # Link
    metallib_out = os.path.join(metal_out, 'rive_pls_macosx.metallib')
    cmd_link = ['xcrun', '-sdk', 'macosx', 'metallib'] + air_files + ['-o', metallib_out]
    run(cmd_link)
    
    # Convert to C header
    c_out = os.path.join(out_dir, 'rive_pls_macosx.metallib.c')
    with open(metallib_out, 'rb') as f:
        data = f.read()
    
    with open(c_out, 'w') as f:
        f.write('unsigned char rive_pls_macosx_metallib[] = {')
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write('\n  ')
            f.write(f'0x{byte:02x}, ')
        f.write('\n};\n')
        f.write(f'unsigned int rive_pls_macosx_metallib_len = {len(data)};\n')

def generate_vulkan_shaders(shaders_dir, out_dir):
    print(f"Generating Vulkan shaders from {shaders_dir} to {out_dir}")
    spirv_out_dir = os.path.join(out_dir, 'spirv')
    ensure_dir(spirv_out_dir)
    
    glslang = find_tool('glslangValidator')
    spirv_opt = find_tool('spirv-opt')
    
    if not glslang or not spirv_opt:
        print("glslangValidator or spirv-opt not found, skipping Vulkan shader generation.")
        return

    spirv_src_dir = os.path.join(shaders_dir, 'spirv')
    spirv_bin_to_header = os.path.join(shaders_dir, 'spirv_binary_to_header.py')

    def compile_spirv(src_file, stage, defines, out_name_no_ext):
        out_spirv = os.path.join(out_dir, f"{out_name_no_ext}.spirv")
        out_h = os.path.join(out_dir, f"{out_name_no_ext}.h")
        
        # Check timestamp
        if os.path.exists(out_h) and os.path.getmtime(out_h) > os.path.getmtime(src_file):
            return

        # Compile
        # Force Vulkan 1.1 to match MoltenVK support
        # Also ensure we use the correct entry point name if needed, but glslang usually handles 'main' correctly.
        # We add -g0 to strip debug info which might confuse MoltenVK.
        cmd_compile = [glslang, '--target-env', 'vulkan1.1', '-S', stage, '-DTARGET_VULKAN', '-g0'] + defines + ['-I' + out_dir, '-V', src_file, '-o', out_spirv + '.unoptimized']
        run(cmd_compile)
        
        # Optimize
        # Note: We are temporarily disabling spirv-opt to rule out optimization issues causing Metal validation errors.
        # The upstream Makefile uses very specific flags for spirv-opt which we are not yet replicating fully.
        # cmd_opt = [spirv_opt, '--preserve-bindings', '--preserve-interface', '-O', out_spirv + '.unoptimized', '-o', out_spirv]
        # run(cmd_opt)
        
        # Use unoptimized shader
        shutil.copy(out_spirv + '.unoptimized', out_spirv)
        
        if os.path.exists(out_spirv + '.unoptimized'):
            os.remove(out_spirv + '.unoptimized')
        
        # Convert to header
        # The variable name logic in Makefile is: $(subst $(suffix $1),_$2,$(notdir $1))
        # $1 is filename, $2 is type (e.g. vert, frag, fixedcolor_frag)
        # Example: draw.main, fixedcolor_frag -> draw_fixedcolor_frag
        
        src_filename = os.path.basename(src_file)
        src_name_no_ext = os.path.splitext(src_filename)[0]
        
        # Determine the type suffix used in Makefile
        # out_name_no_ext is like 'spirv/draw.vert' or 'spirv/draw.fixedcolor_frag'
        # We want 'vert' or 'fixedcolor_frag'
        type_suffix = os.path.splitext(out_name_no_ext)[1][1:] # remove dot
        
        var_name = f"{src_name_no_ext}_{type_suffix}"
        
        run([sys.executable, spirv_bin_to_header, out_spirv, out_h, var_name])

    # Lists from Makefile
    SPIRV_FIXEDCOLOR_FRAG_INPUTS = [
        'spirv/atomic_draw_image_mesh.main',
        'spirv/atomic_draw_image_rect.main',
        'spirv/atomic_draw_interior_triangles.main',
        'spirv/atomic_draw_atlas_blit.main',
        'spirv/atomic_draw_path.main',
        'spirv/atomic_resolve.main',
        'spirv/draw_clockwise_path.main',
        'spirv/draw_clockwise_clip.main',
        'spirv/draw_clockwise_interior_triangles.main',
        'spirv/draw_clockwise_interior_triangles_clip.main',
        'spirv/draw_clockwise_atlas_blit.main',
        'spirv/draw_clockwise_image_mesh.main',
        'spirv/draw_msaa_atlas_blit.main',
        'spirv/draw_msaa_image_mesh.main',
        'spirv/draw_msaa_path.main',
        'spirv/draw_msaa_stencil.main',
    ]

    SPIRV_DRAW_MSAA_INPUTS = [
        'spirv/draw_msaa_path.main',
        'spirv/draw_msaa_image_mesh.main',
        'spirv/draw_msaa_atlas_blit.main',
    ]

    def get_frag_params(filename):
        params = []
        if 'webgpu' in filename:
            params.append('-DPLS_IMPL_NONE')
        elif 'clockwise' in filename:
            params.append('-DPLS_IMPL_STORAGE_TEXTURE')
        else:
            params.append('-DPLS_IMPL_SUBPASS_LOAD')
        return params

    # Standard Inputs
    standard_inputs = glob.glob(os.path.join(spirv_src_dir, '*.main')) + \
                      glob.glob(os.path.join(spirv_src_dir, '*.vert')) + \
                      glob.glob(os.path.join(spirv_src_dir, '*.frag'))
                      
    for f in standard_inputs:
        filename = os.path.basename(f)
        name_no_ext = os.path.splitext(filename)[0]
        ext = os.path.splitext(filename)[1]
        
        if ext == '.vert':
            compile_spirv(f, 'vert', ['-DVERTEX'], os.path.join('spirv', name_no_ext + '.vert'))
        elif ext == '.frag':
            compile_spirv(f, 'frag', ['-DFRAGMENT'] + get_frag_params(filename), os.path.join('spirv', name_no_ext + '.frag'))
        elif ext == '.main':
            compile_spirv(f, 'vert', ['-DVERTEX'], os.path.join('spirv', name_no_ext + '.vert'))
            compile_spirv(f, 'frag', ['-DFRAGMENT'] + get_frag_params(filename), os.path.join('spirv', name_no_ext + '.frag'))

    # Fixed Color Frag
    for rel_path in SPIRV_FIXEDCOLOR_FRAG_INPUTS:
        f = os.path.join(shaders_dir, rel_path)
        if not os.path.exists(f): continue
        filename = os.path.basename(f)
        name_no_ext = os.path.splitext(filename)[0]
        compile_spirv(f, 'frag', ['-DFRAGMENT', '-DFIXED_FUNCTION_COLOR_OUTPUT'] + get_frag_params(filename), os.path.join('spirv', name_no_ext + '.fixedcolor_frag'))

    # MSAA Inputs
    for rel_path in SPIRV_DRAW_MSAA_INPUTS:
        f = os.path.join(shaders_dir, rel_path)
        if not os.path.exists(f): continue
        filename = os.path.basename(f)
        name_no_ext = os.path.splitext(filename)[0]
        
        # noclipdistance_vert
        compile_spirv(f, 'vert', ['-DVERTEX', '-DDISABLE_CLIP_DISTANCE_FOR_UBERSHADERS'], os.path.join('spirv', name_no_ext + '.noclipdistance_vert'))
        
        # webgpu_vert
        compile_spirv(f, 'vert', ['-DVERTEX', '-DSPEC_CONST_NONE'], os.path.join('spirv', name_no_ext + '.webgpu_vert'))
        
        # webgpu_noclipdistance_vert
        compile_spirv(f, 'vert', ['-DVERTEX', '-DSPEC_CONST_NONE', '-DDISABLE_CLIP_DISTANCE_FOR_UBERSHADERS'], os.path.join('spirv', name_no_ext + '.webgpu_noclipdistance_vert'))
        
        # webgpu_frag
        compile_spirv(f, 'frag', ['-DFRAGMENT', '-DSPEC_CONST_NONE', '-DINPUT_ATTACHMENT_NONE'] + get_frag_params('webgpu'), os.path.join('spirv', name_no_ext + '.webgpu_frag'))
        
        # webgpu_fixedcolor_frag
        compile_spirv(f, 'frag', ['-DFRAGMENT', '-DSPEC_CONST_NONE', '-DINPUT_ATTACHMENT_NONE', '-DFIXED_FUNCTION_COLOR_OUTPUT'] + get_frag_params('webgpu'), os.path.join('spirv', name_no_ext + '.webgpu_fixedcolor_frag'))


def main():
    if len(sys.argv) < 3:
        print("Usage: generate_shaders.py <shaders_dir> <out_dir> [platform]")
        sys.exit(1)
        
    shaders_dir = sys.argv[1]
    out_dir = sys.argv[2]
    platform = sys.argv[3] if len(sys.argv) > 3 else None
    
    ensure_dir(out_dir)
    
    # 1. Minify
    print("--- Minifying Shaders ---")
    minify_script = os.path.join(shaders_dir, "minify.py")
    inputs = glob.glob(os.path.join(shaders_dir, "*.glsl")) + \
             glob.glob(os.path.join(shaders_dir, "*.vert")) + \
             glob.glob(os.path.join(shaders_dir, "*.frag"))
    
    cmd = [sys.executable, minify_script, '-H', '-o', out_dir] + inputs
    run(cmd)
    
    # 2. Generate Shaders based on platform
    if platform == 'windows':
        generate_d3d_shaders(shaders_dir, out_dir)
        generate_vulkan_shaders(shaders_dir, out_dir)
    elif platform == 'macos':
        generate_metal_shaders(shaders_dir, out_dir)
        # Vulkan on macOS is disabled for Rive to avoid MoltenVK issues
        # generate_vulkan_shaders(shaders_dir, out_dir)
    elif platform == 'linuxbsd':
        generate_vulkan_shaders(shaders_dir, out_dir)
    elif platform == 'android':
        generate_vulkan_shaders(shaders_dir, out_dir)
    else:
        # Default to trying all if no platform specified or unknown
        generate_d3d_shaders(shaders_dir, out_dir)
        generate_metal_shaders(shaders_dir, out_dir)
        generate_vulkan_shaders(shaders_dir, out_dir)

if __name__ == "__main__":
    main()
