"""Copy only missing original NPC model dependencies; never mount the whole beta game root."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

from audit_npc_asset_closure import Mounts, canonical, model_dependencies, mounted_roots

MODELS = ("models/spitball_large.mdl", "models/spitball_medium.mdl",
          "models/spitball_small.mdl", "models/combine_elite.mdl")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--overlay", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    original, overlay = args.original.resolve(), args.overlay.resolve()
    roots = [path for path in mounted_roots(args.game) if path != original]
    current = Mounts(roots)
    beta = Mounts([original])
    assets = set()
    model_rows = []
    for model in MODELS:
        row = model_dependencies(beta, model)
        if not row["resolved"] or row.get("inspection_error"):
            raise ValueError("Cannot inspect original model: " + model + ": " + str(row.get("inspection_error")))
        model_rows.append(row)
        assets.add(model)
        assets.update(item["asset"] for item in row["companions"] if item["resolved"])
        assets.update(item["asset"] for item in row["materials"] if item["resolved"])
        for suffix in (".phy", ".ani"):
            if (original / (model[:-4]+suffix)).is_file():
                assets.add(model[:-4]+suffix)
    files = []
    for relative in sorted(assets):
        relative = canonical(relative)
        source, destination = (original/relative).resolve(), (overlay/relative).resolve()
        if not source.is_relative_to(original) or not destination.is_relative_to(overlay):
            raise ValueError("Asset path escaped intended roots")
        payload = source.read_bytes()
        row = {"asset": relative, "original": str(source), "bytes": len(payload),
               "sha256": hashlib.sha256(payload).hexdigest()}
        found = current.locate(relative)
        if destination.is_file():
            if destination.read_bytes() != payload:
                raise ValueError("Refusing to overwrite existing different overlay asset: " + relative)
            row.update(status="already_exact_in_overlay", output=str(destination))
        elif found:
            row.update(status="existing_mount_preserved", resolved=found)
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            if destination.read_bytes() != payload:
                raise ValueError("Copy mismatch: " + relative)
            row.update(status="copied_exact_original", output=str(destination))
        files.append(row)
    report = {"original_root": str(original), "overlay_root": str(overlay),
              "models": model_rows, "files": files,
              "limits": ["Only listed missing original model dependencies are copied; configuration/scripts remain untouched.",
                         "Native v37 conversion, rendering and gameplay require a separate runtime test."]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({"files": len(files), "copied": sum(row["status"]=="copied_exact_original" for row in files),
                      "existing_mount_preserved": sum(row["status"]=="existing_mount_preserved" for row in files)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
