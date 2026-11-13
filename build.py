#!/usr/bin/env python3
"""Compile the shared Mutable Dreams engine against the native VIBE SDK."""
from pathlib import Path
import ast
import base64
import os
import shutil
import json
import re
import subprocess
import zipfile

here = Path(__file__).resolve().parent
work = here
sdk = here / "official/sdk"
tools = Path(os.environ["WASI_SDK_BIN"])
source = work / "engine-2.1"
app = source / "app"
build = here / "build"
objects = build / "objects"
objects.mkdir(parents=True, exist_ok=True)
includes = [f"-I{p}" for p in (app, app / "third_party", source / "base/lib", source / "base/include",
                              sdk / "include", sdk / "src")]
flags = ["--target=wasm32-wasip1-threads", "-O3", "-ffreestanding", "-ffp-contract=off",
         "-fno-builtin-memset", "-fno-builtin-memcpy", "-fno-builtin-memcmp",
         "-matomics", "-mbulk-memory", "-ffunction-sections", "-fdata-sections",
         "-DFM1B_UAC_CH=8",   # the engine's six dry track stems (md_render_stems) for the plugin's extra outputs
         *includes]
dsp = ast.literal_eval(re.search(r"^o2\s*=\s*(.+)", (app / "fm1.toml").read_text(), re.M)[1])
compiled = []
for file in [*(app / p for p in dsp), here / "adapter-check.c", here / "pad-performance.c", here / "garden-textures.c",
             sdk / "src/vibe_sdk.c", sdk / "src/vibe_font.c"]:
    dest = objects / (str(file.relative_to(work)).replace("/", "_") + ".o")
    cxx = file.suffix != ".c"
    compiler = tools / ("clang++" if cxx else "clang")
    extra = ["-std=c++11", "-fno-exceptions", "-fno-rtti", "-fno-c++-static-destructors", "-DTEST",
             f"-I{app / 'compat'}"] if cxx else ["-std=c11"]
    subprocess.run([compiler, *flags, *extra, "-DMD_PORT_PERFORMANCE", "-c", file, "-o", dest], check=True)
    compiled.append(dest)

identifier = "mutable-dreams-garden"
wasm = build / f"{identifier}.wasm"
subprocess.run([tools / "wasm-ld", "--no-entry", "--export-dynamic", "--export=__heap_base",
                "--export=__data_end", "--shared-memory", "--max-memory=67108864",
                "-z", "stack-size=262144", "-o", wasm, *compiled], check=True)

manifest = build / f"{identifier}.json"
manifest.write_text(json.dumps({
    "name": "Mutable Dreams · Performance Garden",
    "version": "0.18.0-preview", "author": "Kid Bunka",
    "description": "The complete Mutable Dreams 2.1 on VIBE FM: 47 pages, 271 Biomes, Itinera dreams, the Mutable Dreams FM radio, 16 performance pads, a stage with all pages, stems, DAW parameters and its own Editor page with the whole Biome library.",
    "license": "GPL-3.0-only",
    "stage": {"w": 1280, "h": 720},
}, ensure_ascii=False, indent=2)+"\n")
# The host accepts a self-contained custom editor (at most 2 MiB).
page_dst = build / f"{identifier}.editor"
if page_dst.exists():
    shutil.rmtree(page_dst)
page_dst.mkdir()
html = (here / "editor.html").read_text()
css = (here / "editor.css").read_text()
fern = base64.b64encode((here / "editor-fern.jpg").read_bytes()).decode()
css = css.replace("img/fern.jpg", "data:image/jpeg;base64," + fern)
js = (here / "editor.js").read_text()
html = html.replace('<link rel="stylesheet" href="style.css">', "<style>\n" + css + "\n</style>")
html = html.replace('<script src="app.js"></script>', "<script>\n" + js + "\n</script>")
helper = sdk.parent / "emulator/js/vibe-editor.js"
html = html.replace(
    '<script src="vibe-editor.js"></script>',
    '<script src="vibe-editor.js"></script>\n'
    "<script>/* the VIBE FM SDK's vibe-editor.js (Apache-2.0, Sven Trogus), only if the host's did not load */\n"
    "if (!window.vibe) {\n" + helper.read_text().replace("</script", "<\\/script") + "\n}\n</script>",
)
(page_dst / "index.html").write_text(html)
assert (page_dst / "index.html").stat().st_size <= 2 * 1024 * 1024
shutil.copyfile(here / f"{identifier}.png", build / f"{identifier}.png")
with zipfile.ZipFile(build / f"{identifier}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
    for path in (wasm, manifest, build / f"{identifier}.png"):
        if path.exists(): archive.write(path, path.name)
    if page_dst.is_dir():
        for f in sorted(page_dst.rglob("*")):
            if f.is_file(): archive.write(f, str(f.relative_to(build)))
print(wasm)
