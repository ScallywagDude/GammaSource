"""Evaluate actual isolated NPC probe logs; source registration is not gameplay proof."""
import argparse
import json
import math
import re
from pathlib import Path


def fields(line):
    return dict(re.findall(r"\b(\w+)=(.*?)(?=\s\w+=|$)", line.strip()))


def analyze(text, classname, combat):
    rows = [fields(line) for line in text.splitlines() if line.startswith("GAMMA_NPC_INFO ")]
    subject = [row for row in rows if row.get("name") == "gamma_npc_subject" and row.get("class") == classname]
    targets = [row for row in rows if row.get("name") == "gamma_npc_target"]
    point = classname == "npc_spotlight"
    checks = {
        "probe_started": "GAMMA_NPC_PROBE_BEGIN " + classname in text,
        "probe_completed": "GAMMA_NPC_PROBE_COMPLETE " + classname in text,
        "factory_exists": not re.search(r"GAMMA_NPC_SPAWN class=" + re.escape(classname) + r" factory_missing=1", text),
        "native_spawn_succeeded": bool(re.search(r"GAMMA_NPC_SPAWN class=" + re.escape(classname) + r" result=0 entity=\d+", text)),
        "subject_snapshots_present": len(subject) >= 3,
        "npc_identity": bool(subject) and all(row.get("npc") == "1" for row in subject),
        "positive_health": bool(subject) and (point or all(int(row.get("health", "0")) > 0 for row in subject)),
        "alive": bool(subject) and all(row.get("alive") == "1" for row in subject),
        "model_loaded": bool(subject) and all(int(row.get("bones", "0")) > 0 and int(row.get("sequences", "0")) > 0
            and "error" not in row.get("model", "").lower() for row in subject),
        "save_load_completed": "GAMMA_NPC_PROBE_RESTORED " + classname in text,
        "no_engine_fatal": not re.search(r"ED_Alloc:|Host_Error:|Sys_Error:|Engine Error", text),
    }
    for row in subject:
        for key in ("cycle", "time"):
            if not math.isfinite(float(row.get(key, "nan"))):
                checks["finite_animation_values"] = False
    checks.setdefault("finite_animation_values", bool(subject))
    cycles = {tuple(row.get(key, "") for key in ("sequence", "cycle")) for row in subject}
    if not point:
        checks["animation_advanced"] = len(cycles) >= 2
    healths = [int(row["health"]) for row in targets if "health" in row]
    healths += [int(row["enemy_health"]) for row in subject
                if row.get("enemy") == "npc_bullseye" and int(row.get("enemy_health", "-1")) >= 0]
    if combat:
        checks["target_present"] = bool(healths)
        checks["attack_inflicted_damage"] = bool(healths) and min(healths) < 100000
    restored = text.split("GAMMA_NPC_PROBE_RESTORED " + classname, 1)
    checks["subject_survived_restore"] = len(restored) == 2 and any(
        line.startswith("GAMMA_NPC_INFO ") and "name=gamma_npc_subject " in line
        and "class=" + classname + " " in line for line in restored[-1].splitlines())
    warnings = [line for line in text.splitlines() if re.search(
        r"could not be converted|couldn't find.*model|has no sequence|no sequence for act|missing material|material .* not found|"
        r"PrecacheScriptSound.*failed|Failed to load sound|Can't create physics object|no model name|WARNING:.*no eye offset", line, re.I)]
    return {"classname": classname, "pass": all(checks.values()), "checks": checks,
            "evidence": {"subject_snapshots": subject, "target_snapshots": targets,
                         "minimum_target_health": min(healths) if healths else None},
            "warnings": warnings,
            "limits": ["A passing spawn/combat probe does not certify every navigation map or AI schedule.",
                       "Rendered model, beam, projectile, sound quality and physics interactions require screenshot/runtime inspection.",
                       "Spotlight is an invisible point NPC; Hydra needs its separate procedural-chain and impale validation."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--classname", required=True)
    parser.add_argument("--combat", action="store_true")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = analyze(args.log.read_text(encoding="utf8", errors="replace"), args.classname, args.combat)
    encoded = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(encoded)
    print(encoded)
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
