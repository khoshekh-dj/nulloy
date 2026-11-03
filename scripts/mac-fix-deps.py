#!/usr/bin/env python3
"""Make every Mach-O file in a .app bundle resolvable without Homebrew.

macdeployqt does not follow @rpath references of non-Qt libraries (for
example Homebrew's libbrotlidec -> @rpath/libbrotlicommon), so some
dependencies are never copied. This walks all Mach-O files, copies any
missing dylib into Contents/Frameworks, rewrites references to
@executable_path/../Frameworks/<name>, and finally checks that every
dependency resolves to a file inside the bundle or to the system.

Usage: mac-fix-deps.py <path/to/App.app> <homebrew prefix>
"""
import glob
import os
import shutil
import subprocess
import sys

APP, BREW = sys.argv[1], sys.argv[2]
FRAMEWORKS = os.path.join(APP, "Contents", "Frameworks")
SYSTEM_PREFIXES = ("/System/", "/usr/lib/")


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def is_macho(path):
    with open(path, "rb") as f:
        magic = f.read(4)
    return magic in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xce\xfa\xed\xfe")


def macho_files():
    for root, _, files in os.walk(APP):
        for name in files:
            path = os.path.join(root, name)
            if not os.path.islink(path) and is_macho(path):
                yield path


def own_id(path):
    out = run("otool", "-D", path).splitlines()
    return out[1].strip() if len(out) > 1 else None


def deps(path):
    me = own_id(path)
    for line in run("otool", "-L", path).splitlines()[1:]:
        dep = line.strip().split(" (")[0]
        if dep != me:
            yield dep


def find_source(name):
    for pattern in (f"{BREW}/lib/{name}", f"{BREW}/opt/*/lib/{name}"):
        hits = glob.glob(pattern)
        if hits:
            return os.path.realpath(hits[0])
    return None


def needs_fix(dep):
    if dep.startswith(SYSTEM_PREFIXES) or ".framework/" in dep:
        return False
    if dep.startswith("@executable_path/../Frameworks/"):
        return not os.path.exists(os.path.join(FRAMEWORKS, os.path.basename(dep)))
    return dep.startswith(("@rpath/", "@loader_path/", BREW, "/usr/local/"))


changed = True
while changed:
    changed = False
    for path in list(macho_files()):
        for dep in deps(path):
            if not needs_fix(dep):
                continue
            name = os.path.basename(dep)
            target = os.path.join(FRAMEWORKS, name)
            if not os.path.exists(target):
                src = find_source(name)
                if not src:
                    sys.exit(f"cannot find {name} (needed by {path})")
                shutil.copy2(src, target)
                os.chmod(target, 0o755)
                run("install_name_tool", "-id", f"@executable_path/../Frameworks/{name}", target)
                print(f"added {name}")
            run("install_name_tool", "-change", dep, f"@executable_path/../Frameworks/{name}", path)
            changed = True

# final check: every dependency must resolve inside the bundle or to the system
macos_dir = os.path.join(APP, "Contents", "MacOS")
bad = []
for path in macho_files():
    for dep in deps(path):
        if dep.startswith(SYSTEM_PREFIXES):
            continue
        if dep.startswith("@executable_path/"):
            resolved = os.path.join(macos_dir, dep[len("@executable_path/"):])
        elif dep.startswith("@loader_path/"):
            resolved = os.path.join(os.path.dirname(path), dep[len("@loader_path/"):])
        else:
            resolved = None  # @rpath or absolute non-system path: not allowed
        if not resolved or not os.path.exists(resolved):
            bad.append(f"{os.path.relpath(path, APP)} -> {dep}")
if bad:
    print("UNRESOLVED dependencies:\n  " + "\n  ".join(sorted(set(bad))))
    sys.exit(1)
print("all dependencies resolve inside the bundle")
