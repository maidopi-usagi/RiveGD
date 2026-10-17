"""Generate Rive's embedded desktop and Android shaders from its Makefile rules."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def run(command):
    print(f"Running: {subprocess.list2cmdline([str(arg) for arg in command])}", flush=True)
    subprocess.check_call([str(arg) for arg in command])


def find_tool(name, environment=None):
    explicit = os.environ.get(environment) if environment else None
    if explicit:
        if not Path(explicit).is_file():
            raise RuntimeError(f"{environment} does not name a file: {explicit}")
        return explicit
    return shutil.which(name)


def find_fxc():
    tool = find_tool("fxc", "FXC")
    if tool:
        return tool
    kits = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Windows Kits/10/bin"
    candidates = sorted(kits.glob("*/x64/fxc.exe"), reverse=True)
    candidates.append(kits / "x64/fxc.exe")
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError("fxc not found. Install the Windows SDK or set FXC to fxc.exe.")


def minify_shaders(shaders_dir, out_dir, platform):
    inputs = sorted(path for pattern in ("*.glsl", "*.vert", "*.frag")
                    for path in shaders_dir.glob(pattern))
    if not inputs:
        raise RuntimeError(f"No GLSL shader inputs in {shaders_dir}")
    command = [sys.executable, shaders_dir / "minify.py", "-o", out_dir]
    if platform == "windows":
        command.append("--msvc")
    run(command + inputs)
    return [out_dir / (path.name + suffix) for path in inputs for suffix in (".exports.h", ".hpp")] + [
        out_dir / (path.stem + ".minified" + path.suffix) for path in inputs]


def generate_d3d_shaders(shaders_dir, out_dir):
    fxc = find_fxc()
    output = out_dir / "d3d"
    output.mkdir(parents=True, exist_ok=True)
    inputs = sorted((shaders_dir / "d3d").glob("*.hlsl"))
    if not inputs:
        raise RuntimeError("No D3D shader inputs")
    outputs = []
    for source in inputs:
        variants = [("vert", "VERTEX", [])]
        if source.stem == "render_atlas":
            variants += [("stroke.frag", "FRAGMENT", ["/D", "ATLAS_FEATHERED_STROKE"]),
                         ("fill.frag", "FRAGMENT", ["/D", "ATLAS_FEATHERED_FILL"])]
        else:
            variants.append(("frag", "FRAGMENT", []))
        for variant, stage, defines in variants:
            name = (f"render_atlas_{variant}.h" if variant.startswith(("stroke", "fill"))
                    else f"{source.stem}.{variant}.h")
            target = output / name
            run([fxc, "/D", stage, *defines, "/I", out_dir, "/T",
                 "vs_5_0" if stage == "VERTEX" else "ps_5_0", "/Fh", target, source])
            outputs.append(target)
    target = output / "root.sig.h"
    run([fxc, "/I", out_dir, "/T", "rootsig_1_1", "/E", "ROOT_SIG", "/Fh",
         target, shaders_dir / "d3d/root.sig"])
    return outputs + [target]


def spirv_rules(shaders_dir):
    """Read upstream's simple list variables and make_spirv_rules calls.

    Keeping input/variant lists in Rive's Makefile prevents new shader names or
    backend variants from silently being skipped when the submodule is updated.
    This is deliberately not a general Make interpreter: unsupported syntax
    fails immediately instead of generating an incomplete shader set.
    """
    source = re.sub(r"\\\r?\n", " ", (shaders_dir / "Makefile").read_text(encoding="utf-8"))
    source = re.sub(r"(?m)^\s*#.*$", "", source)
    variables = dict(re.findall(r"^([A-Za-z_][A-Za-z_0-9]*)\s*:?=\s*(.*)$", source, re.MULTILINE))

    def closing_paren(text, start):
        depth = 1
        for index in range(start, len(text)):
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
                if depth == 0:
                    return index
        raise RuntimeError("Unbalanced shader Makefile expression")

    def expand(expression, stack=()):
        while "$" in expression:
            start = expression.find("$(")
            if start == -1:
                raise RuntimeError(f"Unsupported shader Makefile expression: {expression}")
            end = closing_paren(expression, start + 2)
            name = expression[start + 2:end].strip()
            if name.startswith("wildcard "):
                patterns = expand(name[len("wildcard "):], stack).split()
                value = " ".join(path.relative_to(shaders_dir).as_posix()
                                 for pattern in patterns for path in sorted(shaders_dir.glob(pattern)))
            elif name in variables and name not in stack:
                value = expand(variables[name], (*stack, name))
            else:
                raise RuntimeError(f"Unsupported shader Makefile variable: {name}")
            expression = expression[:start] + value + expression[end + 1:]
        return expression.strip()

    prefix = "$(eval $(call make_spirv_rules,"
    rules = {}
    for match in re.finditer(re.escape(prefix), source):
        start = match.end()
        end = closing_paren(source, start)
        expression = source[start:end]
        arguments = []
        depth = 0
        last = 0
        for index, char in enumerate(expression):
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            elif char == "," and depth == 0:
                arguments.append(expand(expression[last:index]))
                last = index + 1
        arguments.append(expand(expression[last:]))
        if len(arguments) not in (2, 3):
            raise RuntimeError(f"Unsupported make_spirv_rules call: {expression}")
        inputs, variants = arguments[:2]
        defines = arguments[2].split() if len(arguments) == 3 else []
        for variant in variants.split():
            for filename in inputs.split():
                path = shaders_dir / filename
                stage = variant.split("_")[-1]
                if stage not in ("vert", "frag") or not path.is_file():
                    raise RuntimeError(f"Invalid SPIR-V rule: {filename} {variant}")
                if path.suffix == (".frag" if stage == "vert" else ".vert"):
                    continue
                rules[(filename, variant)] = defines
    if not rules:
        raise RuntimeError("No make_spirv_rules calls found in Rive's Makefile")
    return rules


def spirv_optimizer_parameters(filename, variant):
    """Match this Rive revision's driver-safe SPIR-V optimization rules."""
    parameters = ["--preserve-bindings", "--preserve-interface"]
    if variant.endswith("_vert") or variant == "vert" or "webgpu" in variant:
        return parameters + ["-O"]
    if "atomic" in filename:
        return parameters + [
            "--wrap-opkill", "--simplify-instructions", "--eliminate-dead-branches",
            "--merge-return", "--inline-entry-points-exhaustive", "--eliminate-dead-inserts",
            "--eliminate-dead-members", "--merge-blocks", "--redundancy-elimination",
            "--cfg-cleanup", "--eliminate-dead-const", "--eliminate-dead-variables",
            "--eliminate-dead-functions", "--eliminate-dead-code-aggressive",
        ]
    # Adreno needs these passes; -O's simplify-instructions breaks its drivers.
    return parameters + [
        "--wrap-opkill", "--eliminate-dead-branches", "--merge-return",
        "--inline-entry-points-exhaustive", "--eliminate-dead-functions",
        "--eliminate-dead-code-aggressive", "--private-to-local",
        "--eliminate-local-single-block", "--eliminate-local-single-store",
        "--eliminate-dead-code-aggressive", "--scalar-replacement=100",
        "--convert-local-access-chains", "--eliminate-local-single-block",
        "--eliminate-local-single-store", "--eliminate-dead-code-aggressive",
        "--ssa-rewrite", "--eliminate-dead-code-aggressive", "--ccp",
        "--eliminate-dead-code-aggressive", "--loop-unroll", "--eliminate-dead-branches",
        "--redundancy-elimination", "--combine-access-chains", "--scalar-replacement=100",
        "--convert-local-access-chains", "--eliminate-local-single-block",
        "--eliminate-local-single-store", "--eliminate-dead-code-aggressive",
        "--ssa-rewrite", "--eliminate-dead-code-aggressive", "--vector-dce",
        "--eliminate-dead-inserts", "--eliminate-dead-branches", "--if-conversion",
        "--copy-propagate-arrays", "--reduce-load-size", "--eliminate-dead-code-aggressive",
        "--merge-blocks", "--redundancy-elimination", "--eliminate-dead-branches",
        "--merge-blocks",
    ]


def shader_generation_policy(platform):
    optimizer = find_tool("spirv-opt", "SPIRV_OPT") if platform in ("windows", "linux", "android") else None
    if platform == "android" and not optimizer:
        raise RuntimeError(
            "Android shader generation requires spirv-opt for Adreno driver workarounds. "
            "Install SPIRV-Tools or set SPIRV_OPT to spirv-opt."
        )
    return {"spirv_optimizer": optimizer, "spirv_policy": "rive-driver-safe" if optimizer else "unoptimized-desktop"}


def generate_vulkan_shaders(shaders_dir, out_dir, optimizer=None):
    glslang = find_tool("glslangValidator", "GLSLANG_VALIDATOR")
    if not glslang:
        raise RuntimeError("glslangValidator not found. Install the Vulkan SDK or set GLSLANG_VALIDATOR.")
    output = out_dir / "spirv"
    output.mkdir(parents=True, exist_ok=True)
    outputs = []
    for (filename, variant), defines in spirv_rules(shaders_dir).items():
        source = shaders_dir / filename
        stage = variant.split("_")[-1]
        target = output / f"{source.stem}.{variant}"
        binary, header = Path(str(target) + ".spv"), Path(str(target) + ".h")
        parameters = ["-DVERTEX"] if stage == "vert" else ["-DFRAGMENT"]
        if stage == "frag":
            implementation = ("STORAGE_BUFFER" if "webgpu" in variant else
                              "STORAGE_TEXTURE" if "clockwise" in filename and "clockwise_atomic" not in filename
                              else "SUBPASS_LOAD")
            parameters.append(f"-DPLS_IMPL_{implementation}")
        unoptimized = Path(str(binary) + ".unoptimized") if optimizer else binary
        run([glslang, "-S", stage, "-DTARGET_SPIRV", *parameters, "-I" + str(out_dir),
             "-V", *defines, "-o", unoptimized, source])
        if optimizer:
            run([optimizer, *spirv_optimizer_parameters(filename, variant), unoptimized, "-o", binary])
            unoptimized.unlink()
        run([sys.executable, shaders_dir / "spirv_binary_to_header.py", binary, header,
             f"{source.stem}_{variant}"])
        outputs += [binary, header]
    return outputs


def generate_metal_shaders(shaders_dir, out_dir):
    xcrun = find_tool("xcrun")
    if not xcrun:
        raise RuntimeError("xcrun not found. Install Xcode Command Line Tools.")
    output = out_dir / "macosx"
    output.mkdir(parents=True, exist_ok=True)
    combinations = out_dir / "draw_combinations.metal"
    run([sys.executable, shaders_dir / "metal/generate_draw_combinations.py", combinations])
    air_files = []
    for source in sorted((shaders_dir / "metal").glob("*.metal")):
        target = output / f"{source.stem}.air"
        run([xcrun, "-sdk", "macosx", "metal", "-std=macos-metal2.3", "-mmacosx-version-min=11.0",
             "-I", out_dir, "-ffast-math", "-ffp-contract=fast", "-fpreserve-invariance",
             "-fvisibility=hidden", "-c", source, "-o", target])
        air_files.append(target)
    if not air_files:
        raise RuntimeError("No Metal shader inputs")
    library = output / "rive_pls_macosx.metallib"
    run([xcrun, "-sdk", "macosx", "metallib", *air_files, "-o", library])
    data = library.read_bytes()
    header = out_dir / "rive_pls_macosx.metallib.c"
    rows = ["  " + ", ".join(f"0x{byte:02x}" for byte in data[start:start + 12])
            for start in range(0, len(data), 12)]
    header.write_text("unsigned char rive_pls_macosx_metallib[] = {\n" + ",\n".join(rows) +
                      f"\n}};\nunsigned int rive_pls_macosx_metallib_len = {len(data)};\n", encoding="utf-8")
    return [combinations, *air_files, library, header]


def source_digest(shaders_dir, generation_policy):
    digest = hashlib.sha256(Path(__file__).read_bytes())
    digest.update(json.dumps(generation_policy, sort_keys=True).encode("utf-8"))
    for path in sorted(shaders_dir.rglob("*")):
        if path.is_file() and "out" not in path.relative_to(shaders_dir).parts and "__pycache__" not in path.parts:
            digest.update(path.relative_to(shaders_dir).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shaders_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("platform", nargs="?", default={"win32": "windows", "darwin": "macos"}.get(sys.platform, "linux"),
                        choices=("windows", "linux", "linuxbsd", "android", "macos"))
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    shaders_dir, out_dir = args.shaders_dir.resolve(), args.out_dir.resolve()
    platform = "linux" if args.platform == "linuxbsd" else args.platform
    generation_policy = shader_generation_policy(platform)
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = out_dir / ".rivegd-shaders.json"
    digest = source_digest(shaders_dir, generation_policy)
    if manifest.exists() and not args.force:
        try:
            cache = json.loads(manifest.read_text(encoding="utf-8"))
            if cache["platform"] == platform and cache["source_digest"] == digest and cache["outputs"] and all(
                    (out_dir / name).is_file() for name in cache["outputs"]):
                print("Rive shaders are up to date.")
                return
        except (ValueError, KeyError, TypeError):
            pass
    outputs = minify_shaders(shaders_dir, out_dir, platform)
    if platform == "windows":
        outputs += generate_d3d_shaders(shaders_dir, out_dir)
    if platform in ("windows", "linux", "android"):
        if not generation_policy["spirv_optimizer"]:
            print("spirv-opt not found; using unoptimized desktop SPIR-V shaders.")
        outputs += generate_vulkan_shaders(shaders_dir, out_dir, generation_policy["spirv_optimizer"])
    elif platform == "macos":
        outputs += generate_metal_shaders(shaders_dir, out_dir)
    manifest.write_text(json.dumps({"platform": platform, "source_digest": digest,
                                   "outputs": [path.relative_to(out_dir).as_posix() for path in outputs]}, indent=2), encoding="utf-8")
    print(f"Generated {len(outputs)} shader files for {platform}.")


if __name__ == "__main__":
    main()
