import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", default="godot")
    parser.add_argument("--driver", default="metal" if sys.platform == "darwin" else "vulkan", choices=("vulkan", "d3d12", "opengl3", "metal"))
    parser.add_argument("--addon-path", type=Path)
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--display-driver")
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    executable = Path(args.godot)
    if executable.name.endswith("_console.exe"):
        engine = executable.with_name(executable.name.replace("_console.exe", ".exe"))
        if engine.is_file():
            args.godot = str(engine)
    repository = Path(__file__).resolve().parent.parent
    addon = (args.addon_path or repository / "project/addons/godot-rive").resolve()
    fixtures = (args.fixtures or repository / "third-party/rive-runtime/tests/unit_tests/assets").resolve()
    frameworks = list((addon / "bin").glob("*.framework"))
    libraries = [path for path in (addon / "bin").rglob("*") if path.suffix in (".dll", ".so", ".dylib") and not path.name.startswith("~")]
    if not libraries and not frameworks:
        parser.error("Build the extension before running the Inspector tests")
    for filename in ("data_bind_test_cmdq.riv", "data_bind_font_test.riv", "data_binding_artboards_test.riv", "kablammo.ttf", "nabla.ttf"):
        if not (fixtures / filename).is_file():
            parser.error(f"Missing upstream fixture: {fixtures / filename}")

    with tempfile.TemporaryDirectory(prefix="rivegd-inspector-") as directory:
        project = Path(directory)
        plugin = project / "addons/inspector_test"
        plugin.mkdir(parents=True)
        target = project / "addons/godot-rive"
        target.mkdir()
        shutil.copy2(addon / "godot-rive.gdextension", target)
        for library in libraries:
            destination = target / library.relative_to(addon)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(library, destination)
        for framework in frameworks:
            shutil.copytree(framework, target / framework.relative_to(addon), symlinks=True, dirs_exist_ok=True)
        shutil.copy2(repository / "scripts/inspector_smoke_test.gd", plugin)
        shutil.copy2(repository / "scripts/inspector_smoke_plugin.gd", plugin / "plugin.gd")
        (plugin / "plugin.cfg").write_text(
            '[plugin]\nname="Rive Inspector tests"\ndescription=""\nauthor=""\nversion="1"\nscript="plugin.gd"\n',
            encoding="utf-8",
        )
        method = "gl_compatibility" if args.driver == "opengl3" else "mobile"
        (project / "project.godot").write_text(
            'config_version=5\n\n[application]\nconfig/name="Rive Inspector tests"\n'
            '\n[editor_plugins]\nenabled=PackedStringArray("res://addons/inspector_test/plugin.cfg")\n'
            f'\n[rendering]\nrenderer/rendering_method="{method}"\n',
            encoding="utf-8",
        )
        (project / ".godot").mkdir()
        (project / ".godot/extension_list.cfg").write_text(
            'res://addons/godot-rive/godot-rive.gdextension\n', encoding="utf-8"
        )
        command = [
            args.godot, "--editor", "--path", str(project), "--rendering-method", method,
            "--rendering-driver", args.driver, "--audio-driver", "Dummy",
            "--render-thread", "safe", "--resolution", "1024x768",
        ]
        if args.display_driver:
            command.extend(["--display-driver", args.display_driver])
        command.extend(["--", str(fixtures)])
        try:
            result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=args.timeout)
        except subprocess.TimeoutExpired as error:
            print(error.stdout.decode("utf-8", "replace") if isinstance(error.stdout, bytes) else error.stdout or "")
            print(error.stderr.decode("utf-8", "replace") if isinstance(error.stderr, bytes) else error.stderr or "", file=sys.stderr)
            print("Inspector test timed out", file=sys.stderr)
            return 1
        except OSError as error:
            print(f"Cannot run Godot: {error}", file=sys.stderr)
            return 1
        output = result.stdout + result.stderr
        print(output, end="")
        diagnostics = re.search(
            r"^ERROR:|SCRIPT ERROR:|FATAL|leaked|still in use|not freed|Scan thread aborted", output, re.MULTILINE
        )
        return int(result.returncode != 0 or diagnostics is not None or "RIVE_INSPECTOR_SMOKE_PASS" not in output)


if __name__ == "__main__":
    sys.exit(main())
