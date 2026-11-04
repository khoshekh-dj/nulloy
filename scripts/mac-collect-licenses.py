#!/usr/bin/env python3
"""Collect licence texts for every Homebrew library bundled into the app.

Walks the dependency closure of the given Mach-O files (the app binary,
its plugins and the bundled GStreamer plugins) *before* they are copied
and rewritten, maps each Homebrew library to its formula, and copies the
formula's licence files into <app>/Contents/Resources/Licenses/<formula>/.
Formulae that ship no licence file get an INFO.txt with the SPDX licence
and homepage from `brew info`.

Usage: mac-collect-licenses.py <App.app> <homebrew prefix> <project dir> <macho>...
"""
import glob
import json
import os
import re
import shutil
import subprocess
import sys

APP, BREW, PROJECT = sys.argv[1], sys.argv[2], sys.argv[3]
ROOTS = sys.argv[4:]
DEST = os.path.join(APP, "Contents", "Resources", "Licenses")
CELLAR = re.compile(re.escape(os.path.realpath(BREW)) + r"/Cellar/([^/]+)/([^/]+)/")
LICENSE_FILE = re.compile(r"^(licen[cs]e|copying|notice|copyright|authors)", re.I)


def deps(path):
    try:
        out = subprocess.run(["otool", "-L", path], check=True, capture_output=True,
                             text=True).stdout.splitlines()[1:]
    except subprocess.CalledProcessError:
        return []
    return [line.strip().split(" (")[0] for line in out]


def resolve(dep, origin):
    if dep.startswith(("/System/", "/usr/lib/")):
        return None
    if dep.startswith("@loader_path/"):
        cand = os.path.join(os.path.dirname(origin), dep[len("@loader_path/"):])
        return os.path.realpath(cand) if os.path.exists(cand) else None
    if dep.startswith(("@rpath/", "@executable_path/")):
        tail = dep.split("/", 1)[1]
        for pattern in (f"{BREW}/lib/{tail}", f"{BREW}/opt/*/lib/{tail}",
                        f"{BREW}/opt/*/lib/{os.path.basename(tail)}"):
            hits = glob.glob(pattern)
            if hits:
                return os.path.realpath(hits[0])
        return None
    return os.path.realpath(dep) if os.path.exists(dep) else None


seen, formulae, todo = set(), {}, [os.path.realpath(r) for r in ROOTS]
while todo:
    path = todo.pop()
    if path in seen:
        continue
    seen.add(path)
    m = CELLAR.search(path)
    if m:
        formulae[m.group(1)] = os.path.join(os.path.realpath(BREW), "Cellar", m.group(1), m.group(2))
    for dep in deps(path):
        target = resolve(dep, path)
        if target and target not in seen:
            todo.append(target)

# also cover everything that actually ended up in the bundle (e.g. libraries
# pulled in by Qt plugins that macdeployqt deploys): map each bundled dylib
# back to the Homebrew formula that provides it
for root, _, files in os.walk(os.path.join(APP, "Contents")):
    for name in files:
        if not name.endswith(".dylib"):
            continue
        for hit in glob.glob(f"{BREW}/opt/*/lib/{name}"):
            m = CELLAR.search(os.path.realpath(hit))
            if m:
                formulae.setdefault(m.group(1), os.path.join(os.path.realpath(BREW), "Cellar",
                                                             m.group(1), m.group(2)))
                break

os.makedirs(DEST, exist_ok=True)
info = {}
if formulae:
    out = subprocess.run(["brew", "info", "--json=v2", *sorted(formulae)],
                         capture_output=True, text=True).stdout
    try:
        for f in json.loads(out).get("formulae", []):
            info[f["name"]] = f
    except json.JSONDecodeError:
        pass

for name, keg in sorted(formulae.items()):
    target = os.path.join(DEST, name)
    os.makedirs(target, exist_ok=True)
    copied = 0
    for entry in os.listdir(keg):
        src = os.path.join(keg, entry)
        if os.path.isfile(src) and LICENSE_FILE.match(entry):
            shutil.copy2(src, target)
            copied += 1
    meta = info.get(name, {})
    with open(os.path.join(target, "INFO.txt"), "w") as fh:
        fh.write(f"{name} {os.path.basename(keg)}\n")
        fh.write(f"License: {meta.get('license') or 'see project homepage'}\n")
        fh.write(f"Homepage: {meta.get('homepage', '')}\n")
        if not copied:
            fh.write("(No licence file is shipped with this Homebrew package; see the homepage.)\n")
    print(f"  {name}: {copied} licence file(s)")

# the app's own licence
own = os.path.join(DEST, "TrackDeck")
os.makedirs(own, exist_ok=True)
shutil.copy2(os.path.join(PROJECT, "LICENSE.GPL3"), own)
with open(os.path.join(own, "INFO.txt"), "w") as fh:
    fh.write("TrackDeck, based on Nulloy by Sergey Vlasov (https://github.com/nulloy/nulloy)\n"
             "License: GPL-3.0. Source: https://github.com/khoshekh-dj/trackdeck\n"
             "Skins: CC-BY-SA 3.0.\n")
print(f"licences collected for {len(formulae)} Homebrew packages")
