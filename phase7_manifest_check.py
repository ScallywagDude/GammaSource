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


def mapping(value, label):
    if not isinstance(value, dict):
        raise ValueError(label + ": expected a JSON object")
    return value


def names(value, label):
    if not isinstance(value, list) or any(not isinstance(x, str) or not x for x in value):
        raise ValueError(label + ": expected a list of classnames")
    if len(value) != len(set(value)):
        raise ValueError(label + ": duplicate classname")
    return set(value)


def weapon_rows(value, label):
    if not isinstance(value, list) or any(not isinstance(x, dict) for x in value):
        raise ValueError(label + ": expected weapon rows")
    names([row.get("classname") for row in value], label)
    return {row["classname"]: row for row in value}


def boolean(value, label):
    if type(value) is not bool:
        raise ValueError(label + ": expected a JSON boolean")
    return value

def read(source):
    source = Path(source)
    if source.is_file():
        with zipfile.ZipFile(source) as z:
            return [json.loads(z.read(name)) for name in FILES]
    return [json.loads((source / name).read_text("utf-8")) for name in FILES]

def validate(data):
    if not isinstance(data, (list, tuple)) or len(data) != len(FILES):
        raise ValueError("expected four Phase 7 report documents")
    inventory, manifests, assets, builds = data
    inventory = mapping(mapping(inventory, "inventory")["branches"], "inventory branches")
    assets = mapping(mapping(assets, "assets")["branches"], "asset branches")
    builds = mapping(mapping(builds, "builds")["branches"], "build branches")
    problems, rows = [], []
    if set(inventory)!=set(BRANCH.values()) or set(assets)!=set(BRANCH.values()) or set(builds)!=set(BRANCH.values()):
        return ["incorrect branch set"], []
    if not isinstance(manifests, list) or any(not isinstance(x, dict) for x in manifests):
        raise ValueError("grant manifest: expected branch rows")
    manifest_branches = names([entry.get("branch") for entry in manifests], "grant manifest branches")
    if len(manifests)!=4 or manifest_branches!=set(BRANCH):
        return ["missing/duplicate grant manifest branch"], []
    for entry in manifests:
        b = BRANCH[entry["branch"]]
        weapons = weapon_rows(inventory[b], b+": inventory")
        build_report = mapping(builds[b], b+": build report")
        asset_report = mapping(assets[b], b+": asset report")
        known = set(weapons)
        if not known:
            problems.append(b+": empty weapon inventory")
        grant = names(entry["compiled_impulse101_extra"], b+": grants")
        requested = names(entry["impulse101_extra"], b+": requested grants")
        excluded = names(entry["excluded_not_compiled"], b+": uncompiled exclusions")
        reasons = entry.get("excluded_source_or_internal", {})
        if not isinstance(reasons, dict) or any(not isinstance(v, str) or not v for v in reasons.values()):
            raise ValueError(b+": expected source exclusion reasons")
        internal = set(reasons) | names(entry.get("base_or_abstract_excluded", []), b+": internal exclusions")
        declared_internal = {name for name, w in weapons.items() if w.get("grant_exclusion_reason")}
        if declared_internal != internal:
            problems.append(b+": inventory/source exclusions mismatch")
        build_rows = weapon_rows(build_report["weapons"], b+": build report")
        asset_rows = weapon_rows(asset_report["weapons"], b+": asset report")
        if set(build_rows) != known or type(build_report["registered"]) is not int or build_report["registered"] != len(known):
            problems.append(b+": build report mismatch")
        if set(asset_rows) != known:
            problems.append(b+": asset report mismatch")
        omitted, client_omitted = set(), set()
        for name, build in build_rows.items():
            configurations = {mode: mapping(build[mode], b+": "+name+" "+mode)
                              for mode in ("debug", "release")}
            for side in ("server", "client"):
                included = [boolean(configurations[mode][side], b+": "+name+" "+mode+" "+side)
                            for mode in ("debug", "release")]
                aggregate = boolean(build[side+"_compiled_all_configs"], b+": "+name+" "+side+" aggregate")
                if aggregate != all(included):
                    problems.append(b+": "+name+" inconsistent "+side+" build summary")
                if not all(included):
                    (omitted if side == "server" else client_omitted).add(name)
            if name in weapons and build["source"] != weapons[name]["implementation_file"]:
                problems.append(b+": "+name+" build source mismatch")
        if names(build_report["server_excluded"], b+": server exclusions") != omitted:
            problems.append(b+": server exclusions disagree with build rows")
        if names(build_report["client_source_not_shared"], b+": client exclusions") != client_omitted:
            problems.append(b+": client exclusions disagree with build rows")
        if excluded - requested or (excluded & known) - omitted:
            problems.append(b+": uncompiled grant exclusions disagree with requested/build rows")
        bad = grant - known
        bad |= grant & (excluded | internal | omitted)
        expected_grant = requested - (excluded | internal | omitted)
        if bad or grant != expected_grant or type(entry["registered"]) is not int or entry["registered"] != len(known):
            problems.append(b+": grant/registration inconsistency: "+repr(sorted(bad)))
        branch_evidence = [boolean(entry["windows_build_verified"], b+": Windows build evidence")]
        for field in ("assets_verified", "viewmodels_verified", "hud_icons_verified", "texture_scale_verified"):
            branch_evidence.append(boolean(entry[field], b+": "+field))
        verified = 0
        for name, weapon in weapons.items():
            if not isinstance(weapon.get("implementation_file"), str) or not weapon["implementation_file"]:
                raise ValueError(b+": "+name+": expected an implementation source path")
            checks = [boolean(weapon[key], b+": "+name+" "+key) for key in EVIDENCE]
            asset = asset_rows.get(name)
            if asset is None:
                continue
            if asset["implementation_file"] != weapon["implementation_file"]:
                problems.append(b+": "+name+" asset source mismatch")
            asset_runtime = boolean(asset["runtime_verified"], b+": "+name+" asset runtime evidence")
            if not isinstance(asset.get("status"), str) or not asset["status"]:
                raise ValueError(b+": "+name+": expected an asset status")
            issues = asset.get("issues", [])
            if not isinstance(issues, list) or any(not isinstance(x, str) for x in issues):
                raise ValueError(b+": "+name+": expected asset issues as a list of strings")
            # This is the existing asset auditor's positive dependency state.
            # It still needs separate gameplay evidence; failure/unknown states
            # and absent issue lists cannot be overridden by a boolean flag.
            asset_ready = asset["status"] == "assets_present_unverified_runtime" and asset.get("issues") == []
            if asset_runtime and not asset_ready:
                problems.append(b+": "+name+" runtime evidence contradicts asset dependencies")
            if checks[0] != asset_runtime:
                problems.append(b+": "+name+" runtime evidence disagrees with asset report")
            # Project membership alone cannot establish a successful Windows build.
            # Boolean evidence is reported evidence; this gate does not run gameplay.
            if all(branch_evidence) and name not in omitted and name in build_rows and all(checks) and asset_runtime and asset_ready:
                verified += 1
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
