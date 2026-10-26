"""Restore authored alpha/cubemap blending for six converted beta decal materials.

Original mask and cubemap bytes remain unchanged in a private namespace. The
modern UnlitGeneric shader blends the reflection by the original mask alpha;
zero diffuse color preserves the darkening contribution of non-additive decals.
"""
from pathlib import Path
import argparse, hashlib, json, re, tempfile, os

ROOT = Path(__file__).resolve().parents[2]
MATERIALS = [
    "decals/decalborealispuddle001a", "decals/decalwastelandpuddle001a",
    "decals/decalpuddle001a", "decals/decaldirt001a",
    "decals/shadertest/DecalDarkeningSpecularCubicEnvMap",
    "decals/shadertest/DecalSpecularCubicEnvMap",
]
PREFIX = "gamma_2003/compat_reflective/"

def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as f:
        f.write(data)
        name = f.name
    os.replace(name, path)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=ROOT.parent / "hl2")
    parser.add_argument("--output", type=Path, default=ROOT / "gammasource_content_2018")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    rows = []
    for name in MATERIALS:
        source = args.reference / "materials" / (name + ".vmt")
        text = source.read_text()
        assert re.search(r"Original shader: Decal(?:Darkening)?SpecularCubicEnvMap", text), source
        dependencies = {}
        for var in ("envmap", "envmapmask"):
            match = re.search(r'"\$' + var + r'"\s*"([^"]+)"', text, re.I)
            assert match, (source, var)
            original = match.group(1).replace("\\", "/")
            texture = args.reference / "materials" / (original + ".vtf")
            data = texture.read_bytes()
            private = PREFIX + original
            atomic_write(args.output / "materials" / (private + ".vtf"), data)
            dependencies[var] = {"source": str(texture), "private": private, "sha256": hashlib.sha256(data).hexdigest()}
        # The historical masks carry opacity in alpha, while modern separate
        # envmap masks use RGB (these original RGB channels are solid white).
        text = re.sub(r'"\$envmapmask"\s*"[^"]+"', '"$basetexture" "' + dependencies["envmapmask"]["private"] + '"', text, flags=re.I)
        text = re.sub(r'"\$envmap"\s*"[^"]+"', '"$envmap" "' + dependencies["envmap"]["private"] + '"', text, flags=re.I)
        text = text.replace("{", '{\n\t// Compatibility: original mask alpha controls the decal/reflection blend.\n\t"$translucent" 1\n\t"$color2" "[0 0 0]"', 1)
        destination = args.output / "materials" / (name + ".vmt")
        atomic_write(destination, text.encode("utf-8"))
        rows.append({"source": str(source), "output": str(destination), "dependencies": dependencies,
                     "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                     "output_sha256": hashlib.sha256(destination.read_bytes()).hexdigest()})
    report = {"materials": rows, "texture_bytes_changed": False, "native_shader_rebuild_required": False,
              "reason": "Preserve authored mask alpha rather than treating its white RGB as an opaque envmap mask."}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        atomic_write(args.report, (json.dumps(report, indent=2) + "\n").encode())
    print("Repaired", len(rows), "reflective decal material recipes; original texture bytes preserved.")

if __name__ == "__main__":
    main()
