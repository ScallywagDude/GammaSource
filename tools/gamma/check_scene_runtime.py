"""Check native choreography evidence without claiming rendered poses are correct.

Use a fresh, complete console.log from one isolated gamma_vN_*_boot.cfg run.
Screenshots still require visual inspection; sound starts do not prove audible quality.
"""
import argparse
import json
import math
import re
from pathlib import Path


def fields(line):
    return dict(re.findall(r"\b(\w+)=(.*?)(?=\s\w+=|$)", line.strip()))


def records(text, prefix):
    return [fields(line) for line in text.splitlines() if line.startswith(prefix + " ")]


def path(value):
    return value.replace("\\", "/").lower()


def number(row, key):
    return float(row.get(key, "nan"))


def analyze(text, kind):
    events = records(text, "GAMMA_SCENE_EVENT")
    actors = records(text, "GAMMA_ACTOR")
    finished = records(text, "GAMMA_SCENE_FINISHED")
    started = records(text, "GAMMA_SCENE_STARTED")
    classifications = records(text, "GAMMA_SCENE_GESTURE_CLASS")
    positions = [tuple(map(float, match)) for match in re.findall(
        r"gamma_playerinfo: pos ([-\d.]+) ([-\d.]+) ([-\d.]+)", text)]
    checks = {}
    evidence = {}
    marker = {"lab": "LAB_COMPLETE", "hydra": "HYDRA_COMPLETE",
              "walk": "GESTURE_WALK_COMPLETE"}[kind]
    completion = re.search(r"^GAMMA_V(\d+)_" + marker + r"\s*$", text, re.M)
    checks["complete_marker"] = completion is not None
    evidence["capture_fixture_version"] = int(completion.group(1)) if completion else None
    checks["player_stationary"] = len(positions) >= 2 and all(
        math.dist(positions[0], position) <= .2 for position in positions)
    evidence["player_positions"] = positions
    sound_starts = [path(value) for value in re.findall(
        r"^(?:DynamicSound|StaticSound) (.*?) : src ", text, re.M)]
    evidence["engine_sound_starts"] = sorted(set(sound_starts))
    bad = [row for row in events if row.get("stage") in ("rejected", "reject_ai_ownership")]
    checks["no_rejected_scene_events"] = not bad
    evidence["rejected_events"] = bad
    for row in events:
        for key in ("cycle", "weight", "base_cycle", "scene_time"):
            if not math.isfinite(number(row, key)):
                checks["finite_trace_values"] = False
    checks.setdefault("finite_trace_values", bool(events))
    if kind in ("lab", "hydra"):
        if kind == "lab":
            expected_speech = ["alyx_snapout", "alyx_staringagain", "alyx_areyousure",
                               "kleiner_warming", "kleiner_moduli", "alyx_hopeso1",
                               "alyx_imsorrygordon1", "alyx_imsorrygordon2", "alyx_imsorrygordon3",
                               "alyx_imsorrygordon4", "alyx_imsorrygordon5", "alyx_imsorrygordon6",
                               "alyx_cmondoc", "kleiner_massless"]
            expected_speech = ["vo/e3_lab/" + name + ".wav" for name in expected_speech]
        else:
            expected_speech = ["vo/e3_hydra/" + name + ".wav" for name in (
                "alyx_uhoh", "alyx_runintotrouble", "alyx_thereyouare01", "alyx_thereyouare02")]
        missing_audio = [name for name in expected_speech if not any(
            started.lstrip("*#@><^)}$!?") == name for started in sound_starts)]
        checks["authored_speech_waveforms_started"] = not missing_audio
        evidence["missing_authored_speech_starts"] = missing_audio
        names = ("scene_1_acting_intro", "scene_1_sequence2") if kind == "lab" else ("uhoh_lcs",)
        for name in names:
            checks[name + "_started_once"] = sum(row.get("name") == name for row in started) == 1
            ends = [row for row in finished if row.get("name") == name]
            checks[name + "_finished_uncanceled"] = len(ends) == 1 and ends[0].get("canceled") == "0"
        if kind == "lab":
            checks["sequence3_not_started_by_observer"] = not any(
                row.get("name") == "scene_1_sequence3" for row in started)
            subjects = ("alyx", "kleiner")
        else:
            subjects = ("alyx",)
            for name in ("sewer_soldier", "sewer_hydra01", "sewer_hydra02", "sewer_hydra03"):
                samples = [row for row in actors if row.get("name", "").lower() == name
                           and row.get("label", "").lower() == "e3_sewer"]
                cycles = [number(row, "cycle") for row in samples]
                checks[name + "_authored_cycle_advances"] = len(cycles) >= 2 and max(cycles) - min(cycles) > .05
                checks[name + "_positive_rate"] = bool(samples) and all(number(row, "rate") > 0 for row in samples)
                evidence[name + "_cycles"] = cycles
        for name in subjects:
            samples = [row for row in events if row.get("actor", "").lower() == name
                       and row.get("type") == "sequence" and row.get("stage") == "sample"
                       and number(row, "weight") > .01]
            checks[name + "_body_layers_advance"] = len(samples) >= 2 and len(
                {row.get("cycle") for row in samples}) > 1
            end_samples = [row for row in actors if row.get("name", "").lower() == name]
            checks[name + "_scene_released"] = bool(end_samples) and end_samples[-1].get("scene") == "0"
            evidence[name + "_weighted_sequence_samples"] = len(samples)
            evidence[name + "_speaking_snapshots"] = sum(
                row.get("speaking") == "1" for row in end_samples)
    else:
        legacy = [row for row in events if path(row.get("scene", "")).endswith("legacy_gesture_walk.vcd")]
        posture = [row for row in events if path(row.get("scene", "")).endswith("explicit_posture_walk.vcd")]
        for name, rows in (("legacy", legacy), ("posture", posture)):
            moving = [row for row in rows if row.get("stage") == "sample" and row.get("type") == "gesture"
                      and row.get("moving") == "1" and number(row, "scene_time") > .5]
            weights = [number(row, "weight") for row in moving]
            checks[name + "_has_moving_samples"] = bool(moving)
            evidence[name + "_moving_layer_weights"] = weights
            moving_positions = [tuple(map(float, row["origin"].split(","))) for row in rows
                                if row.get("stage") == "sample" and "origin" in row]
            evidence[name + "_position_extent_units"] = max(
                (math.dist(a, b) for a in moving_positions for b in moving_positions), default=0)
            checks[name + "_actor_actually_moved"] = evidence[name + "_position_extent_units"] > 32
            if name == "legacy":
                checks["legacy_gesture_retained_while_moving"] = bool(weights) and min(weights) > .8
            else:
                checks["explicit_posture_suppressed_while_moving"] = bool(weights) and min(weights) < .01
        checks["legacy_classification"] = any(row.get("legacy") == "1" and row.get("gesture") == "1"
            and row.get("classification") == "legacy_vcd_gesture" for row in classifications)
        checks["explicit_metadata_wins"] = any(row.get("sequence") == "posture03" and row.get("legacy") == "1"
            and row.get("gesture") == "0" and row.get("classification") == "faceposer_type" for row in classifications)
        legacy_ends = [row for row in finished if path(row.get("file", "")).endswith("legacy_gesture_walk.vcd")]
        posture_ends = [row for row in finished if path(row.get("file", "")).endswith("explicit_posture_walk.vcd")]
        checks["legacy_interrupt_and_restart"] = (sum(row.get("canceled") == "1" for row in legacy_ends) >= 1
            and sum(row.get("canceled") == "0" for row in legacy_ends) >= 2)
        checks["posture_repeated"] = sum(row.get("canceled") == "0" for row in posture_ends) >= 2
        snapshots = [row for row in actors if row.get("name", "").lower() == "alyx"]
        checks["actor_released_after_repeats"] = bool(snapshots) and snapshots[-1].get("scene") == "0"
    diagnostics = [line for line in text.splitlines() if re.search(
        r"unable to find (?:sequence|gesture)|failed to start (?:sequence|gesture)|PrecacheScriptSound.*failed|Failed to load sound|uses unknown shader|Can't create physics object", line)]
    return {"kind": kind, "pass": all(checks.values()), "checks": checks, "evidence": evidence,
            "scene_finishes": finished, "gesture_classifications": classifications,
            "diagnostic_warnings": diagnostics,
            "limits": ["Rendered poses and skinning require inspection of captured screenshots.",
                       "Speech state and traced sound starts do not prove audible mix quality."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--kind", required=True, choices=("lab", "hydra", "walk"))
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = analyze(args.log.read_text(encoding="utf8", errors="replace"), args.kind)
    output = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.write_text(output, encoding="utf8")
    print(output)
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
