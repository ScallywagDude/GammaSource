#!/usr/bin/env python3
"""Independent Phase 7 weapon manifest validator; no proprietary game source."""
import json
from pathlib import Path
import sys
import zipfile

FILES = ("modernize/weapon_branch_inventory.json",
         "modernize/weapon_impulse101_manifest.json",
         "modernize/weapon_asset_report.json",
         "modernize/weapon_build_report.json")
BRANCH = {"hl2":"hl2","hl1":"hl1","cs":"cstrike","tf2":"tf2"}
EVIDENCE = ("runtime_verified","viewmodel_verified","hud_image_verified","texture_scale_verified")

def read(source):
    source = Path(source)
    if source.is_file():
        with zipfile.ZipFile(source) as z:
            return [json.loads(z.read(name)) for name in FILES]
    return [json.loads((source / name).read_text("utf-8")) for name in FILES]

def validate(data):
    inventory, manifests, assets, builds = data
    inventory, assets, builds = inventory["branches"], assets["branches"], builds["branches"]
    problems, rows = [], []
    if set(inventory)!=set(BRANCH.values()) or set(assets)!=set(BRANCH.values()) or set(builds)!=set(BRANCH.values()):
        return ["incorrect branch set"], []
    if len(manifests)!=4 or set(x["branch"] for x in manifests)!=set(BRANCH):
        return ["missing/duplicate grant manifest branch"], []
    for entry in manifests:
        b = BRANCH[entry["branch"]]
        weapons = inventory[b]
        known = [x["classname"] for x in weapons]
        grant = entry["compiled_impulse101_extra"]
        excluded = set(entry.get("excluded_not_compiled", []))
        internal = set(entry.get("excluded_source_or_internal", {})) | set(entry.get("base_or_abstract_excluded", []))
        omitted = set(builds[b].get("server_excluded", []))
        bad = set(grant) - set(known)
        bad |= set(grant) & (excluded | internal | omitted)
        if bad or len(grant)!=len(set(grant)) or len(known)!=len(set(known)) or entry["registered"]!=len(known):
            problems.append(b+": grant/registration inconsistency: "+repr(sorted(bad)))
        report_names = [a["classname"] for a in assets[b]["weapons"]]
        if len(report_names)!=len(known) or set(report_names)!=set(known):
            problems.append(b+": asset report mismatch")
        verified = sum(all(w.get(key) is True for key in EVIDENCE) for w in weapons)
        rows.append((b,len(weapons),len(grant),len(excluded),verified))
    return problems,rows

def main():
    if len(sys.argv)!=2:
        print("Usage: python phase7_manifest_check.py SOURCE_ZIP_OR_DIRECTORY",file=sys.stderr)
        return 2
    try:
        problems,rows=validate(read(sys.argv[1]))
    except (OSError,KeyError,TypeError,ValueError,zipfile.BadZipFile) as e:
        print("Report error:",e,file=sys.stderr)
        return 2
    print("Branch | Registered | Extra impulse 101 | Uncompiled excluded | Verified")
    for row in rows: print(" | ".join(map(str,row)))
    if problems:
        print("Integrity FAIL:","; ".join(problems))
        return 2
    total=sum(row[1] for row in rows); done=sum(row[4] for row in rows)
    print("Integrity PASS; completion evidence: %d/%d"%(done,total))
    return 0 if done==total else 1

if __name__=="__main__":
    raise SystemExit(main())
