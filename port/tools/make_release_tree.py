#!/usr/bin/env python3
"""Mutable Dreams · Performance Garden for VIBE FM – assemble the public source tree (GPL-3.0-only), by Kid Bunka.

usage: python3 tools/make_release_tree.py <out-dir>
Copies exactly what the build needs – the port, the engine files the compiler actually reads (clang -MM), the VIBE FM
SDK's sources and its Apache-2.0 notices – plus README, LICENSE and the third-party overview. It publishes nothing.
"""
import ast, re, shutil, subprocess, sys, os
from pathlib import Path

here = Path(__file__).resolve().parent.parent          # port/
work = here.parent                                      # VIBE-FM/
out = Path(sys.argv[1]).resolve()
sdk, source = here / "official/sdk", work / "engine-2.1"
app = source / "app"
_local = here / ".wasi-sdk-path"   # (0.30) a local, unpublished pointer to the toolchain
tools = Path(os.environ.get("WASI_SDK_BIN") or (_local.read_text().strip() if _local.exists() else "/opt/wasi-sdk/bin"))
if out.exists(): shutil.rmtree(out)

# 1. what the compiler reads
includes = [f"-I{p}" for p in (app, app / "third_party", source / "base/lib", source / "base/include", sdk / "include", sdk / "src")]
flags = ["--target=wasm32-wasip1-threads", "-ffreestanding", "-DFM1B_UAC_CH=8", "-DMD_PORT_PERFORMANCE", *includes]
dsp = ast.literal_eval(re.search(r"^o2\s*=\s*(.+)", (app / "fm1.toml").read_text(), re.M)[1])
files = [*(app / p for p in dsp), here / "adapter-check.c", here / "pad-performance.c", here / "garden-textures.c", here / "listen.c",
         sdk / "src/vibe_sdk.c", sdk / "src/vibe_font.c"]
deps = set()
for f in files:
    cxx = f.suffix != ".c"
    extra = ["-std=c++11", "-DTEST", f"-I{app / 'compat'}"] if cxx else ["-std=c11"]
    mm = subprocess.run([tools / ("clang++" if cxx else "clang"), *flags, *extra, "-MM", str(f)], capture_output=True, text=True, check=True).stdout
    for tok in mm.replace("\\\n", " ").replace("\\ ", "\0").split()[1:]:
        deps.add(Path(tok.replace("\0", " ")).resolve())

def put(src, rel):
    dst = out / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)

for d in sorted(deps):
    if str(d).startswith(str(work.resolve())): put(d, d.relative_to(work.resolve()))
# 2. the engine's and the SDK's licences, the engine's build list
for rel in ("engine-2.1/LICENSE", "engine-2.1/LICENSE-MIT.txt", "engine-2.1/LICENSING-GPL.txt", "engine-2.1/app/fm1.toml",
            "engine-2.1/base/LICENSE", *(f"engine-2.1/base/LICENSES/{p.name}" for p in sorted((source / "base/LICENSES").glob("*")))):
    put(work / rel, rel)
for rel in ("LICENSE", "NOTICE", "LICENSE-FONTS"):
    put(sdk / rel, f"port/official/sdk/{rel}")
for rel in ("tools/vibe-check.mjs", "tools/media-host.mjs"):                   # the SDK's own checker (Apache-2.0)
    put(here / "official" / rel, f"port/official/{rel}")
# 2b. (0.31, Codex review 10 B1) the engine's generators: the Biome presets, the Satin wavetables and the lo-fi kit samples
# are written by these scripts (python3 engine-2.1/tools/generate_presets.py etc.); their local imports come along
gen = ["generate_presets.py", "generate_satin.py", "generate_lofi.py"]
seen = set()
while gen:
    g = gen.pop()
    if g in seen or not (source / "tools" / g).exists(): continue
    seen.add(g)
    for m in re.findall(r"^\s*(?:from\s+(\w+)\s+import|import\s+([\w, ]+))", (source / "tools" / g).read_text(), re.M):
        for name in (m[0] or m[1]).replace(" ", "").split(","):
            if (source / "tools" / f"{name}.py").exists(): gen.append(f"{name}.py")
for g in sorted(seen): put(source / "tools" / g, f"engine-2.1/tools/{g}")
for j in ("catalog.json", "lofi-samples.json"):          # their second outputs: the readable Biome catalogue, the kit's list
    put(source / "presets" / j, f"engine-2.1/presets/{j}")
# 3. the port: sources, Editor page, tests, video, tools
for p in sorted(here.iterdir()):
    if p.is_file() and p.suffix in (".c", ".h", ".inc", ".mjs", ".py"): put(p, Path("port") / p.name)
for d in ("editor", "icon", "tests-codex", "tests-data", "tests-listen", "video", "tools", "third_party"):
    if (here / d).is_dir():
        for p in sorted((here / d).rglob("*")):
            if p.is_file() and p.name != ".DS_Store": put(p, Path("port") / p.relative_to(here))
# 4. the top level
for name in ("README.md", "THIRD-PARTY.md", "CHANGELOG.md"):
    put(here / "tools/release" / name, name)
put(work / "engine-2.1/LICENSE", "LICENSE")
n = sum(1 for _ in out.rglob("*") if _.is_file())
size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
print(f"{n} files, {size // 1024} KB -> {out}")
