"""Create and verify release provenance without trusting filenames or mutable refs."""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess

ASSETS = {"QontrolPanel_win64_msvc2022.zip", "QontrolPanel_Installer.exe"}
ROOT = pathlib.Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def version():
    match = re.search(r"project\s*\(\s*QontrolPanel\s+VERSION\s+(\d+\.\d+\.\d+)", (ROOT / "CMakeLists.txt").read_text())
    if not match:
        raise ValueError("Project version is missing")
    return match[1]


def sha256(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def verify(manifest, directory, source, run_id, expected_version, dependency):
    expected = {"schema": 1, "source": source, "run_id": str(run_id),
                "version": expected_version, "headsetcontrol": dependency}
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f"Provenance mismatch: {key}")
    assets = manifest.get("assets", {})
    if set(assets) != ASSETS:
        raise ValueError("Unexpected or missing release assets")
    for name, metadata in assets.items():
        path = directory / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Missing regular artifact: {name}")
        if metadata.get("size") != path.stat().st_size or metadata.get("sha256") != sha256(path):
            raise ValueError(f"Artifact verification failed: {name}")
    return expected_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("create")
    create.add_argument("--output", type=pathlib.Path, required=True)
    create.add_argument("--run", required=True)
    create.add_argument("--qt", required=True)
    create.add_argument("--artifact", type=pathlib.Path, action="append", required=True)
    check = commands.add_parser("verify")
    check.add_argument("--manifest", type=pathlib.Path, required=True)
    check.add_argument("--directory", type=pathlib.Path, required=True)
    check.add_argument("--source", required=True)
    check.add_argument("--run", required=True)
    args = parser.parse_args()
    dependency = git("ls-tree", "HEAD", "dependencies/headsetcontrol").split()[2]
    if args.command == "create":
        if set(path.name for path in args.artifact) != ASSETS:
            raise ValueError("Both release assets are required")
        if git("-C", "dependencies/headsetcontrol", "rev-parse", "HEAD") != dependency:
            raise ValueError("Dependency checkout does not match the recorded revision")
        manifest = {"schema": 1, "source": git("rev-parse", "HEAD"), "run_id": args.run,
                    "version": version(), "headsetcontrol": dependency, "qt": args.qt,
                    "toolset": "v143", "vcpkg": (ROOT / "cmake/vcpkg-baseline.txt").read_text().strip(),
                    "assets": {path.name: {"size": path.stat().st_size, "sha256": sha256(path)} for path in args.artifact}}
        args.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    else:
        if git("rev-parse", "HEAD") != args.source:
            raise ValueError("Checkout does not match selected build source")
        print(verify(json.loads(args.manifest.read_text(encoding="utf-8")), args.directory,
                     args.source, args.run, version(), dependency))


if __name__ == "__main__":
    main()
