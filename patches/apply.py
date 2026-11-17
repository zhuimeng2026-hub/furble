import os
import shutil
import subprocess
import sys
from os.path import join
from platformio.project.helpers import get_project_dir

Import("env")

framework_dir = env.PioPlatform().get_package_dir("framework-espidf")

# Marker that only exists in the patched NimBLE source
PATCH_MARKER = "Peer LTK not found,  initiate pairing"


def find_patch_tool():
    """Locate a GNU patch executable.

    `patch` is not on PATH on this host (Git for Windows ships it under
    usr/bin, which is not part of the default PATH), so look it up there too.
    """
    found = shutil.which("patch")
    if found:
        return found
    candidates = []
    for env_var in ("ProgramFiles", "ProgramW6432", "ProgramFiles(x86)", "LOCALAPPDATA"):
        base = os.environ.get(env_var)
        if not base:
            continue
        for sub in (
            join("Git", "usr", "bin", "patch.exe"),
            join("Git", "mingw64", "bin", "patch.exe"),
            join("Programs", "Git", "usr", "bin", "patch.exe"),
        ):
            candidates.append(join(base, sub))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def apply_ble_gap_patch():
    src_path = join(
        framework_dir,
        "components", "bt", "host", "nimble", "nimble", "nimble", "host", "src", "ble_gap.c",
    )
    patch_file = join(get_project_dir(), "patches", "ble_gap.patch")

    if not os.path.isfile(src_path):
        sys.stderr.write("ERROR: nimble ble_gap.c not found at %s\n" % src_path)
        env.Exit(1)
    if not os.path.isfile(patch_file):
        sys.stderr.write("ERROR: patch file not found at %s\n" % patch_file)
        env.Exit(1)

    patch_tool = find_patch_tool()
    if not patch_tool:
        sys.stderr.write("ERROR: no GNU patch executable found (PATH or Git usr/bin)\n")
        env.Exit(1)

    cmd = [
        patch_tool,
        "--verbose",
        "-N",
        "-p1",
        "-i", patch_file,
        src_path,
    ]
    sys.stdout.write("Applying NimBLE patch: %s\n" % " ".join(cmd))
    result = subprocess.run(cmd, cwd=get_project_dir())
    # exit code 1 == "previously applied / reversed", which is fine thanks to -N
    if result.returncode not in (0, 1):
        sys.stderr.write("ERROR: patch failed with exit code %d\n" % result.returncode)
        env.Exit(1)

    with open(src_path, "r", encoding="utf-8", errors="replace") as fh:
        content = fh.read()
    if PATCH_MARKER not in content:
        sys.stderr.write(
            "ERROR: ble_gap.c does not contain the expected patch marker (%r)\n" % PATCH_MARKER
        )
        env.Exit(1)
    sys.stdout.write("NimBLE ble_gap.c patch verified as applied.\n")


# patch NimBLE
apply_ble_gap_patch()
