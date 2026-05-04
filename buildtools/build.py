#!/usr/bin/env python3
import re
import sys
import subprocess
from pathlib import Path


def resolve_build_dir(command: list[str], cwd: Path | None = None) -> Path | None:
    if len(command) < 3 or command[0] != "cmake" or command[1] != "--build":
        return None

    build_dir = Path(command[2])
    if build_dir.is_absolute():
        return build_dir

    base_dir = cwd if cwd is not None else Path.cwd()
    return (base_dir / build_dir).resolve()


def repair_ninja_metadata(build_dir: Path) -> None:
    if not (build_dir / "build.ninja").exists():
        return

    try:
        probe = subprocess.run(
            ["ninja", "-C", str(build_dir), "-t", "recompact"],
            check=False,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        return

    output = (probe.stdout or "") + (probe.stderr or "")
    if "premature end of file; recovering" not in output:
        return

    repaired = False
    for filename in (".ninja_deps", ".ninja_log"):
        metadata_path = build_dir / filename
        if metadata_path.exists():
            metadata_path.unlink()
            repaired = True

    if repaired:
        print(
            "   Repaired corrupt Ninja metadata in "
            f"{build_dir} (.ninja_deps/.ninja_log reset)"
        )


def run_step(description: str, command: list[str], cwd: Path | None = None) -> None:
    """Run a command and print nice status messages"""
    print(f"\n→ {description}")
    print("   " + str(command))

    build_dir = resolve_build_dir(command, cwd)
    if build_dir is not None:
        repair_ninja_metadata(build_dir)

    try:
        subprocess.run(
            command,
            check=True,
            cwd=cwd,
            text=True,
        )
    except KeyboardInterrupt:
        print("\nInterrupted.")
        sys.exit(130)
    except subprocess.CalledProcessError as e:
        print(f"\nERROR: Command failed with exit code {e.returncode}")
        sys.exit(1)


def collect_example_streams_formats(root: Path) -> list[str]:
    examples_dir = root / "examples"
    if not examples_dir.exists():
        return []

    format_pattern = re.compile(
        r"set\s*\(\s*ASTRALIX_STREAMS_FORMATS\s+([^)]+)\)",
        re.IGNORECASE | re.MULTILINE,
    )

    ordered_formats: list[str] = []

    for cmake_file in sorted(examples_dir.glob("*/CMakeLists.txt")):
        contents = cmake_file.read_text()
        for match in format_pattern.finditer(contents):
            for token in match.group(1).split():
                clean = token.strip().strip('"')
                if not clean or clean.startswith("${"):
                    continue
                if clean not in ordered_formats:
                    ordered_formats.append(clean)

    return ordered_formats


def read_cache_variable(cache_path: Path, variable: str) -> str | None:
    if not cache_path.exists():
        return None
    prefix = f"{variable}:"
    for line in cache_path.read_text().splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1]
    return None


def build_examples(root: str, preset: str, install_prefix: str):
    examples_dir = root / "examples"
    example_dirs = [
        p.relative_to(root) for p in examples_dir.iterdir()
        if p.is_dir() and (p / "CMakeLists.txt").exists()
    ]

    if not example_dirs:
        print("Warning: No example subdirectories with CMakeLists.txt found in examples/")

    steps = []

    vcpkg_prefix = root / "vcpkg_installed/x64-linux"
    cmake_prefix_path = f"{install_prefix}"
    if vcpkg_prefix.exists():
        cmake_prefix_path += f";{vcpkg_prefix}"

    main_build_dir = root / "builds" / preset
    main_cache = main_build_dir / "CMakeCache.txt"
    forwarded_flags = []
    hot_reload = read_cache_variable(main_cache, "ASTRA_EDITOR_HOT_RELOAD")
    if hot_reload == "ON":
        forwarded_flags.append("-DASTRA_EDITOR_HOT_RELOAD=ON")
    sandbox_hot_reload = read_cache_variable(main_cache, "ASTRA_SANDBOX_HOT_RELOAD")
    if sandbox_hot_reload == "ON":
        forwarded_flags.append("-DASTRA_SANDBOX_HOT_RELOAD=ON")

    for rel_example_src in example_dirs:
        example_src = root / rel_example_src
        example_build = example_src / "builds" / preset
        example_build.parent.mkdir(parents=True, exist_ok=True)

        configure_example_cmd = [
            "cmake",
            "-S", str(example_src),
            "-B", str(example_build),
            "--preset", preset,
            f"-DCMAKE_PREFIX_PATH={cmake_prefix_path}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            f"-DASTRA_MAIN_BUILD_DIR={main_build_dir}",
            *forwarded_flags,
        ]

        steps.append((f"Configuring example: {rel_example_src}", configure_example_cmd))

        build_example_cmd = ["cmake", "--build",
                             str(example_build), "--parallel"]
        steps.append(
            (f"Building example: {rel_example_src}", build_example_cmd))

    if example_dirs:
        print(f"   Examples found: {', '.join(str(d) for d in example_dirs)}")

        print("\nExample executables (assuming default target name matches folder):")
        for rel_example_src in example_dirs:
            folder_name = rel_example_src.name
            exe_path = root / rel_example_src / "builds" / preset / folder_name
            rel_exe = exe_path.relative_to(root)
            print(f"   {folder_name}: ./{rel_exe}")
        print("\n(Adjust if your CMake target names differ .)\n")
    print("="*60 + "\n")

    return steps


def build_main(root: str,  preset: str, install_prefix: str,
               streams_formats: list[str]):
    steps = []
    main_build_dir = root / "builds" / preset

    main_build_dir.mkdir(parents=True, exist_ok=True)

    if not streams_formats:
        print("Error: no consumer-defined ASTRALIX_STREAMS_FORMATS found in examples/")
        sys.exit(1)

    configure_main_cmd = [
        "cmake",
        "-S", str(root),
        "-B", str(main_build_dir),
        "--preset", preset,
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        f"-DASTRALIX_STREAMS_FORMATS={';'.join(streams_formats)}",
    ]

    steps.append(("Configuring main project", configure_main_cmd))

    build_main_cmd = ["cmake", "--build", str(main_build_dir), "--parallel"]
    steps.append(("Building main project", build_main_cmd))

    install_cmd = [
        "cmake",
        "--install", str(main_build_dir),
        "--prefix", str(install_prefix),
    ]

    steps.append(("Installing main project", install_cmd))

    print(f"\n=== Generating project with preset: {preset} ===")
    print(f"   Root:           {root}")
    print(f"   Install prefix: {install_prefix}")
    print(f"   Main build dir: {main_build_dir}")

    return steps


def main():
    if len(sys.argv) != 2:
        print("Usage: build.py <preset-name>")
        print("Examples:")
        print("   ./build.py debug")
        sys.exit(1)

    preset = sys.argv[1].strip()
    root = Path(__file__).parent.parent.resolve()
    streams_formats = collect_example_streams_formats(root)

    if not streams_formats:
        print("Error: no consumer-defined ASTRALIX_STREAMS_FORMATS found in examples/")
        sys.exit(1)

    run_step(
        "Installing axgen",
        ["./install.sh", "--formats", ";".join(streams_formats)],
        cwd=root / "axgen",
    )

    install_prefix = root / "install"

    steps = build_main(root, preset, str(install_prefix), streams_formats)
    steps += build_examples(root, preset, str(install_prefix))

    for description, command in steps:
        run_step(description, command)

    print("\n" + "="*60)
    print()

    print("SUCCESS! All projects built and installed.")
    print(f"Main install location: {install_prefix}")


if __name__ == "__main__":
    main()
