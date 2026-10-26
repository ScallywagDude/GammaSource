"""Audit compiled NPC literal model/sound dependencies against actual mounts and a proposed beta fallback.

VPK entries are read directly; nothing is extracted or modified. Literal references
include conditional and fallback paths, so missing entries are review findings,
not automatic proof that every NPC's active path fails.
"""
import argparse
import collections
import functools
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "legacy2003/modernize/port2018"))
from vpkshaders import entries, read_entry
from merge_soundscripts import blocks, manifest_files
from audit_legacy_animation_samples import normalize_original, i, name


def canonical(value):
    return value.replace("\\", "/").lstrip("/").lower()


def token_hash(text):
    tokens = re.findall(r'"[^"\n]*"|[{}]', re.sub(r"//[^\n]*", "", text))
    return hashlib.sha256(" ".join(tokens).encode()).hexdigest()


class Mounts:
    def __init__(self, roots):
        self.roots = roots
        self.archives = {}
        self.index_errors = []
        for root in roots:
            if root.suffix.lower() == ".vpk":
                archive = root if root.is_file() else root.with_name(root.stem + "_dir.vpk")
                try:
                    self.archives[root] = {canonical(row[0]): row[1:] for row in entries(str(archive))}
                    self.archives[root]["__archive"] = archive
                except Exception as error:
                    self.index_errors.append({"archive": str(archive), "error": str(error)})

    @functools.lru_cache(maxsize=None)
    def locate(self, relative):
        relative = canonical(relative)
        for root in self.roots:
            if root in self.archives:
                entry = self.archives[root].get(relative)
                if entry is not None:
                    return {"kind": "vpk", "path": str(self.archives[root]["__archive"]), "entry": relative}
            else:
                candidate = root / relative
                if candidate.is_file():
                    return {"kind": "loose", "path": str(candidate)}
        return None

    @functools.lru_cache(maxsize=None)
    def data(self, relative):
        found = self.locate(relative)
        if not found:
            return None
        if found["kind"] == "loose":
            return Path(found["path"]).read_bytes()
        for root, table in self.archives.items():
            if str(table["__archive"]) == found["path"]:
                return read_entry(found["path"], *table[canonical(relative)])
        raise ValueError("Located archive disappeared")


def mounted_roots(game):
    text = (game / "gameinfo.txt").read_text()
    values = re.findall(r'^\s*game(?:\+[\w_]+)*\s+(?:"([^"]+)"|([^\s]+))', text, re.M | re.I)
    return [Path((a or b).replace("|gameinfo_path|", str(game) + "/")).resolve() for a, b in values]


def material_dependencies(mounts, material, seen=None):
    seen = seen if seen is not None else set()
    relative = canonical("materials/" + material.removesuffix(".vmt") + ".vmt")
    if relative in seen:
        return []
    seen.add(relative)
    data = mounts.data(relative)
    result = [{"asset": relative, "resolved": mounts.locate(relative)}]
    if not data:
        return result
    text = data.decode("latin1")
    for key, value in re.findall(r'"?(\$\w+|include)"?\s+"([^"]+)"', text, re.I):
        if key.lower() == "include":
            result.extend(material_dependencies(mounts, re.sub(r"^materials/", "", canonical(value)), seen))
        elif key.lower() in ("$basetexture", "$bumpmap", "$normalmap", "$detail", "$envmap", "$envmapmask", "$dudvmap"):
            if value.lower() in ("env_cubemap", "_rt_waterreflection", "_rt_waterrefraction", "_rt_fullframefb"):
                continue
            texture = canonical("materials/" + value.removesuffix(".vtf") + ".vtf")
            result.append({"asset": texture, "resolved": mounts.locate(texture)})
    return result


def model_dependencies(mounts, model):
    row = {"asset": model, "resolved": mounts.locate(model), "companions": [], "materials": []}
    data = mounts.data(model)
    if not data:
        return row
    try:
        version = i(data, 4)
        row["version"] = version
        row["sha256"] = hashlib.sha256(data).hexdigest()
        old = version in (36, 37)
        try:
            normalized = normalize_original(data)[0] if old else data
            offset = 4 if old else 0
        except ValueError as error:
            if version != 37:
                raise
            # Three original spitballs have a stale declared file length after
            # their final material-directory string was shortened. Read only
            # their bounded texture metadata here; the native converter still
            # validates the complete model before creating its runtime cache.
            normalized, offset = data, 20
            row["normalizer_warning"] = str(error)
            row["declared_length"] = i(data, 76)
            row["actual_length"] = len(data)
        texture_count, texture_start = i(normalized, 204 + offset), i(normalized, 208 + offset)
        cd_count, cd_start = i(normalized, 212 + offset), i(normalized, 216 + offset)
        directories = [name(normalized, i(normalized, cd_start + 4*k)) for k in range(cd_count)] or [""]
        suffixes = (".dx80.vtx", ".dx7_2bone.vtx", ".dx90.vtx") if old else (".vvd", ".dx90.vtx", ".sw.vtx")
        row["companions"] = [{"asset": model[:-4]+suffix, "resolved": mounts.locate(model[:-4]+suffix)} for suffix in suffixes]
        for k in range(texture_count):
            start = texture_start + (32 if old else 64)*k
            texture = name(normalized, start + i(normalized, start))
            candidates = [canonical(directory + "/" + texture).replace("//", "/") for directory in directories]
            chosen = next((value for value in candidates if mounts.locate("materials/"+value+".vmt")), candidates[0])
            row["materials"].extend(material_dependencies(mounts, chosen))
    except Exception as error:
        row["inspection_error"] = str(error)
    return row


def sound_table(mounts):
    data = mounts.data("scripts/game_sounds_manifest.txt")
    table = collections.defaultdict(list)
    missing_scripts = []
    for script in manifest_files(data.decode("latin1") if data else ""):
        content = mounts.data(script)
        if content is None:
            missing_scripts.append(script)
            continue
        for event, body in blocks(content.decode("latin1")):
            waves = re.findall(r'"wave"\s+"([^"]+)"', body, re.I)
            table[event.lower()].append({"script": script, "waves": waves,
                "token_sha256": token_hash(body)})
    return table, missing_scripts


def audit(mounts, sources):
    models = set()
    sound_names = set()
    references = collections.defaultdict(list)
    for source in sources:
        text = (ROOT/source).read_text(encoding="latin1")
        for model in re.findall(r'"((?:models|sprites)/[^"\n]+\.(?:mdl|vmt))"', text, re.I):
            model = canonical(model)
            models.add(model)
            references[model].append(source)
        for sound in re.findall(r'(?:PrecacheScriptSound|EmitSound|SoundCreate)\s*\([^\n;]*?"([^"\n]+)"', text):
            if "%" not in sound:
                sound_names.add(sound)
    table, missing_scripts = sound_table(mounts)
    model_rows = []
    for model in sorted(models):
        row = model_dependencies(mounts, model) if model.endswith(".mdl") else {"asset": model, "resolved": mounts.locate("materials/"+model), "materials": material_dependencies(mounts, model)}
        row["referenced_by"] = references[model]
        model_rows.append(row)
    sound_rows = []
    for event in sorted(sound_names):
        if event.lower().endswith(".wav"):
            waves, definitions = [event], []
        else:
            definitions = table.get(event.lower(), [])
            waves = sorted({wave for definition in definitions for wave in definition["waves"]})
        wave_rows = [{"wave": wave, "resolved": mounts.locate("sound/"+wave.lstrip("*#@><^)}$!?"))} for wave in waves]
        sound_rows.append({"event": event, "definitions": definitions, "waveforms": wave_rows,
                           "entry_missing": not definitions and not event.lower().endswith(".wav")})
    return {"models": model_rows, "sounds": sound_rows, "missing_manifest_files": missing_scripts,
            "missing_model_literals": [r["asset"] for r in model_rows if not r["resolved"]],
            "missing_material_assets": sorted({r["asset"] for model in model_rows for r in model.get("materials",[]) if not r["resolved"]}),
            "missing_sound_events": [r["event"] for r in sound_rows if r["entry_missing"]],
            "missing_sound_waveforms": sorted({r["wave"] for event in sound_rows for r in event["waveforms"] if not r["resolved"] and "$" not in r["wave"]}),
            "dynamic_waveform_templates": sorted({r["wave"] for event in sound_rows for r in event["waveforms"] if "$" in r["wave"]}),
            "archive_index_errors": mounts.index_errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--shared-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads((ROOT/"verification/beta_npc_variant_audit.json").read_text())
    sources = sorted({item["file"] for row in inventory["entities"] if row["branch"]=="hl2_dll" for item in row["compiled_implementations"]})
    vpc = (ROOT / "game/server/server_hl2.vpc").read_text().lower()
    for filename in ("npc_combot.cpp", "npc_cremator.cpp", "npc_alienassassin.cpp"):
        source = "game/server/gamma/" + filename
        if (ROOT/source).is_file() and ('"gamma\\' + filename + '"') in vpc and source not in sources:
            sources.append(source)
    actual_roots = mounted_roots(args.game)
    # Keep a stable before/after comparison even if the coordinator has already
    # installed the shared fallback while this read-only audit is running.
    roots = [root for root in actual_roots if root != args.shared_root.resolve()]
    proposed = roots.copy()
    if args.shared_root.resolve() not in proposed:
        index = next((k+1 for k,value in enumerate(proposed) if value.name.lower()=="hl2_2003"), next((k for k,value in enumerate(proposed) if value.suffix.lower()==".vpk"),len(proposed)))
        proposed.insert(index,args.shared_root.resolve())
    before, after = audit(Mounts(roots),sources), audit(Mounts(proposed),sources)
    original_events = collections.defaultdict(set)
    for script in (args.shared_root / "scripts").glob("*.txt"):
        for event, body in blocks(script.read_text(encoding="latin1")):
            original_events[event.lower()].add(token_hash(body))
    for row in after["sounds"]:
        for definition in row["definitions"]:
            definition["matches_shared_original_tokens"] = definition["token_sha256"] in original_events[row["event"].lower()]
        for waveform in row["waveforms"]:
            relative = "sound/" + waveform["wave"].lstrip("*#@><^)}$!?")
            shared = args.shared_root / relative
            if shared.is_file():
                # Locations already record the active mount; only loose assets
                # are hashed here to avoid repeatedly reopening VPK archives.
                found = waveform["resolved"]
                if found and found["kind"] == "loose":
                    active_bytes = Path(found["path"]).read_bytes()
                    original_bytes = shared.read_bytes()
                    waveform["active_sha256"] = hashlib.sha256(active_bytes).hexdigest()
                    waveform["shared_original_sha256"] = hashlib.sha256(original_bytes).hexdigest()
                    waveform["matches_shared_original_bytes"] = active_bytes == original_bytes
    report = {"compiled_npc_sources": sources, "actual_gameinfo_mounts": [str(p) for p in actual_roots],
              "baseline_mounts_without_shared": [str(p) for p in roots], "proposed_mounts": [str(p) for p in proposed],
              "baseline_without_shared": before, "with_shared_beta": after,
              "recovered_model_literals": sorted(set(before["missing_model_literals"])-set(after["missing_model_literals"])),
              "recovered_material_assets": sorted(set(before["missing_material_assets"])-set(after["missing_material_assets"])),
              "recovered_waveforms": sorted(set(before["missing_sound_waveforms"])-set(after["missing_sound_waveforms"])),
              "limits": ["Literal references include conditionals and fallbacks; available files do not prove native behavior.",
                         "VPK presence is checked; loose original variant hashes are recorded for provenance.",
                         "Dynamic model/sound names and extra optional physics/gibs need native runtime evidence."]}
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({"sources":len(sources),"recovered_models":report["recovered_model_literals"],"recovered_material_assets":len(report["recovered_material_assets"]),"recovered_waveforms":len(report["recovered_waveforms"]),"remaining_model_literals":after["missing_model_literals"],"remaining_sound_events":after["missing_sound_events"],"remaining_waveforms":after["missing_sound_waveforms"]},indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
