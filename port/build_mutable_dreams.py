#!/usr/bin/env python3
"""Compile the shared Mutable Dreams engine against the native VIBE SDK."""
from pathlib import Path
import ast
import json
import re
import subprocess
import zipfile

here = Path(__file__).resolve().parent
work = here.parent
sdk = here / "official/sdk"
import os
_local = here / ".wasi-sdk-path"   # (0.30) a local, unpublished pointer to the toolchain
tools = Path(os.environ.get("WASI_SDK_BIN") or (_local.read_text().strip() if _local.exists() else "/opt/wasi-sdk/bin"))
source = work / "engine-2.1"   # Mutable Dreams 2.1 release sources (build 2467CC38)
app = source / "app"
build = here / "build"
objects = build / "objects"
objects.mkdir(parents=True, exist_ok=True)
includes = [f"-I{p}" for p in (app, app / "third_party", source / "base/lib", source / "base/include",
                              sdk / "include", sdk / "src")]
flags = ["--target=wasm32-wasip1-threads", "-O3", "-ffreestanding", "-ffp-contract=off",
         "-fno-builtin-memset", "-fno-builtin-memcpy", "-fno-builtin-memcmp",
         "-matomics", "-mbulk-memory", "-ffunction-sections", "-fdata-sections",
         # (0.25) only the WebAssembly extensions the VIBE allows: wasi-sdk 34's clang turns on reference types (and with
         # them a long call_indirect encoding) by default; the SDK's own Zig 0.13 does not. vibe-check validates with
         # --disable-reference-types (found by Codex's strict validation run).
         "-mno-reference-types", "-mno-call-indirect-overlong", "-mno-multivalue", "-mno-extended-const", "-mno-tail-call",
         "-DFM1B_UAC_CH=8",   # the engine's six dry track stems (md_render_stems) for the plugin's extra outputs
         *includes]
dsp = ast.literal_eval(re.search(r"^o2\s*=\s*(.+)", (app / "fm1.toml").read_text(), re.M)[1])
compiled = []
for file in [*(app / p for p in dsp), here / "adapter-check.c", here / "pad-performance.c", here / "garden-textures.c", here / "listen.c",
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
subprocess.run([tools / "wasm-ld", "--no-entry", "--compress-relocations", "--strip-debug", "--export-dynamic", "--export=__heap_base",
                "--export=__data_end", "--shared-memory", "--max-memory=67108864",
                "-z", "stack-size=262144", "-o", wasm, *compiled], check=True)

manifest = build / f"{identifier}.json"
manifest.write_text(json.dumps({
    "name": "Mutable Dreams · Performance Garden",
    "version": "0.32.0-preview", "author": "Kid Bunka",
    "description": "The complete Mutable Dreams 2.1 on VIBE FM: 50 pages, 271 Biomes, Itinera dreams, the Mutable Dreams FM radio, 16 performance pads, a stage with all pages, stems, DAW parameters and its own Editor page with the whole Biome library, Kid Bunka's 48 worlds and the night garden of the Brick and the MPC. New on the VIBE: the garden listens - IN 1 / IN 2 into its cloud, echo and room, SING (it answers your voice) and CV / gate.",
    "license": "GPL-3.0-only",
    "stage": {"w": 1280, "h": 720},
}, ensure_ascii=False, indent=2)+"\n")
# SDK 16: the plugin's own Editor page goes into the zip as <id>.editor/ (2 MB at most)
import shutil
page_src, page_dst = here / "editor", build / f"{identifier}.editor"
if page_dst.exists(): shutil.rmtree(page_dst)
if page_src.is_dir():
    shutil.copytree(page_src, page_dst)
    # One self-contained index.html: styles, script and the image inline (the sandbox may refuse the page's own
    # files from an opaque origin; inline is allowed everywhere). vibe-editor.js still comes from the host.
    import base64
    html = (page_dst / "index.html").read_text()
    css = (page_dst / "style.css").read_text()
    for img in (page_dst / "img").glob("*"):
        mime = "image/jpeg" if img.suffix in (".jpg", ".jpeg") else "image/png"
        css = css.replace(f"img/{img.name}", f"data:{mime};base64," + base64.b64encode(img.read_bytes()).decode())
    fonts = page_dst / "fonts"                       # (0.19) Nunito and Barlow, SIL OFL 1.1: inline; the licences stay as files
    if fonts.is_dir():
        for ttf in fonts.glob("*.ttf"):
            css = css.replace(f"fonts/{ttf.name}", "data:font/ttf;base64," + base64.b64encode(ttf.read_bytes()).decode())
            ttf.unlink()
    js = (page_dst / "app.js").read_text()
    kit = page_dst / "kit.js"                          # (0.19) the Visual Kit of the Brick: the night garden behind the page
    if kit.exists():
        html = html.replace('<script src="kit.js"></script>', "<script>\n" + kit.read_text().replace("</script", "<\\/script") + "\n</script>")
        kit.unlink()
    html = html.replace('<link rel="stylesheet" href="style.css">', "<style>\n" + css + "\n</style>")
    html = html.replace('<script src="app.js"></script>', "<script>\n" + js + "\n</script>")
    # Fallback: when a sandbox refuses even the host's vibe-editor.js, the SDK's own copy (Apache-2.0) inline.
    helper = here / "third_party/vibe-editor.js"   # (0.30) the SDK's Apache-2.0 vibe-editor.js, kept beside the port
    if not helper.exists(): helper = sdk.parent / "emulator/js/vibe-editor.js"
    if helper.exists():
        html = html.replace('<script src="vibe-editor.js"></script>', '<script src="vibe-editor.js"></script>\n'
            "<script>/* the VIBE FM SDK's vibe-editor.js (Apache-2.0, Sven Trogus), only if the host's did not load */\n"
            "if (!window.vibe) {\n" + helper.read_text().replace("</script", "<\\/script") + "\n}\n</script>")
    (page_dst / "index.html").write_text(html)
    for extra in ("style.css", "app.js"): (page_dst / extra).unlink()
    shutil.rmtree(page_dst / "img", ignore_errors=True)   # (0.31: the unused picture is gone)
    size = sum(f.stat().st_size for f in page_dst.rglob("*") if f.is_file())
    assert size <= 2 * 1024 * 1024, f"editor page is {size} bytes, 2 MB at most"
# (0.31, Codex review 10 B2) the licences travel with the firmware: inside its Editor folder (which the host already
# carries with its font licences), so the drop-in zip alone holds every licence text and notice it needs.
if page_dst.is_dir():
    lic = page_dst / "licenses"
    lic.mkdir(exist_ok=True)
    def keep(src, name):
        if Path(src).exists(): (lic / name).write_bytes(Path(src).read_bytes())
    keep(source / "LICENSE", "GPL-3.0.txt")
    keep(source / "LICENSE-MIT.txt", "MIT-Mutable-Dreams.txt")
    keep(source / "base/LICENSE", "MPL-2.0-base.txt")
    keep(sdk / "LICENSE", "Apache-2.0-VIBE-FM-SDK.txt")
    keep(sdk / "NOTICE", "NOTICE-VIBE-FM-SDK.txt")
    keep(sdk / "LICENSE-FONTS", "LICENSE-FONTS-VIBE-FM-SDK.txt")
    # the MIT notice of the Mutable Instruments files the build compiles (their copyright years, the licence text):
    # kept as a file beside the port, so every machine packs the same bytes
    keep(here / "third_party/MIT-Mutable-Instruments.txt", "MIT-Mutable-Instruments.txt")
    (lic / "README.txt").write_text(
        "Mutable Dreams · Performance Garden – firmware for VIBE FM, by Kid Bunka. Licence: GPL-3.0-only (GPL-3.0.txt).\n"
        "The complete corresponding source code is offered together with each release of this firmware: a source tree\n"
        "that builds this exact file byte for byte (README.md in it says how). Ask the author if you did not receive it.\n\n"
        "Inside: the Mutable Dreams engine (GPL-3.0-only; MIT-Mutable-Dreams.txt for its MIT-licensed parts),\n"
        "Mutable Instruments DSP code (MIT, MIT-Mutable-Instruments.txt), platform headers (MPL-2.0, MPL-2.0-base.txt),\n"
        "the VIBE FM SDK (Apache-2.0, Apache-2.0-VIBE-FM-SDK.txt and NOTICE-VIBE-FM-SDK.txt; its fonts:\n"
        "LICENSE-FONTS-VIBE-FM-SDK.txt), the Editor page's fonts Nunito and Barlow Semi Condensed (SIL OFL 1.1, ../fonts/).\n"
        "\"VIBE FM\" is a trademark of its owner; this firmware is built with the VIBE FM SDK.\n")
class _Zip(zipfile.ZipFile):                       # (0.31) a reproducible zip: fixed dates and modes, same bytes on every machine
    def write(self, path, arcname):
        info = zipfile.ZipInfo(arcname, date_time=(2026, 1, 1, 0, 0, 0))
        info.compress_type, info.external_attr = zipfile.ZIP_DEFLATED, 0o644 << 16
        self.writestr(info, Path(path).read_bytes())
with _Zip(build / f"{identifier}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
    icon_src = here / "icon" / f"{identifier}.png"            # (0.31, Codex review 10 B5) the icon's source asset
    if icon_src.exists(): (build / f"{identifier}.png").write_bytes(icon_src.read_bytes())
    for path in (wasm, manifest, build / f"{identifier}.png"):
        if path.exists(): archive.write(path, path.name)
    if page_dst.is_dir():
        for f in sorted(page_dst.rglob("*")):
            if f.is_file(): archive.write(f, str(f.relative_to(build)))
print(wasm)
