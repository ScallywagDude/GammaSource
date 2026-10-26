"""Audit every lossless movie frame and rank temporal body-region discontinuities.

Requires Pillow and NumPy. Reads the original TGA files without rewriting/copying
them. Pixel scores rank frames for review; they are not an animation correctness
test and can increase during valid authored motion, occlusion, or camera changes.
"""
import argparse
import csv
import hashlib
import io
import json
import re
import statistics
import struct
from pathlib import Path

import numpy as np
from PIL import Image


# Normalized rectangles from the preserved 1024x640 e3_lab Alyx view.
# The large foreground workstation occludes part of the lower body.
REGIONS = {
    "body": (.439453125, .421875, .6201171875, 1.0),
    "upper": (.439453125, .515625, .6201171875, .796875),
    "legs": (.458984375, .703125, .5810546875, 1.0),
    "head_control": (.4765625, .4375, .5537109375, .5546875),
    "background_control": (.009765625, .015625, .1953125, .28125),
}


def summarize(values):
    if not values:
        return {}
    array = np.asarray(values, dtype=np.float64)
    return {"mean": float(array.mean()), "p50": float(np.percentile(array, 50)),
            "p95": float(np.percentile(array, 95)), "p99": float(np.percentile(array, 99)),
            "maximum": float(array.max())}


def audit(folder, output, fps, expected, prefix):
    selected = []
    for file in folder.glob("*.tga"):
        match = re.match(r"^(.*?)(\d{4,})\.tga$", file.name, re.I)
        if match and (prefix is None or match[1] == prefix):
            selected.append((int(match[2]), match[1], file))
    if not selected:
        raise ValueError("No numbered TGA movie frames found")
    prefixes = {row[1] for row in selected}
    if len(prefixes) != 1:
        raise ValueError("Multiple movie prefixes; specify --prefix")
    selected.sort()
    indexes = [row[0] for row in selected]
    missing = sorted(set(range(indexes[0], indexes[-1] + 1)) - set(indexes))
    duplicate_indexes = sorted({i for i in indexes if indexes.count(i) > 1})
    rows, errors = [], []
    previous, previous_previous = {}, {}
    expected_size = None
    total_bytes = 0
    for index, _, file in selected:
        raw = file.read_bytes()
        total_bytes += len(raw)
        row = {"frame": index, "file": file.name, "relative_seconds": index / fps,
               "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
        try:
            if len(raw) < 18:
                raise ValueError("Truncated TGA header")
            width, height, bits, descriptor = struct.unpack_from("<HHBB", raw, 12)
            row.update(width=width, height=height, bits=bits, image_type=raw[2], descriptor=descriptor)
            with Image.open(io.BytesIO(raw)) as image:
                image.load()
                if expected_size is None:
                    expected_size = image.size
                if image.size != expected_size:
                    raise ValueError("Frame dimensions changed")
                pixels = np.asarray(image.convert("RGB"), dtype=np.int16)
            current = {}
            for name, rect in REGIONS.items():
                x0, y0, x1, y1 = [round(v * (width if k % 2 == 0 else height)) for k, v in enumerate(rect)]
                region = pixels[y0:y1, x0:x1]
                current[name] = region
                if name in previous:
                    change = np.abs(region - previous[name])
                    row[name + "_mean_abs_change"] = float(change.mean())
                    row[name + "_changed_fraction_gt_24"] = float((change.max(axis=2) > 24).mean())
                if name in previous_previous:
                    acceleration = np.abs(region - 2 * previous[name] + previous_previous[name])
                    row[name + "_mean_abs_second_difference"] = float(acceleration.mean())
            previous_previous, previous = previous, current
        except Exception as error:
            errors.append({"frame": index, "file": file.name, "error": str(error)})
            previous, previous_previous = {}, {}
        rows.append(row)
    key = "body_mean_abs_second_difference"
    for position, row in enumerate(rows):
        if key not in row:
            continue
        neighborhood = [near[key] for near in rows[max(0, position - 30):position + 31] if key in near]
        median = statistics.median(neighborhood)
        mad = statistics.median(abs(value - median) for value in neighborhood)
        row["body_local_motion_median"] = median
        row["body_local_motion_mad"] = mad
        row["body_local_motion_robust_z"] = (row[key] - median) / max(.02, 1.4826 * mad)
    ranked = sorted((row for row in rows if "body_local_motion_robust_z" in row),
                    key=lambda row: row["body_local_motion_robust_z"], reverse=True)
    peaks = []
    for row in ranked:
        if all(abs(row["frame"] - existing["frame"]) >= fps / 2 for existing in peaks):
            peaks.append(row)
        if len(peaks) == 24:
            break
    csv_path = output.with_suffix(".csv")
    fields = sorted({key for row in rows for key in row})
    with csv_path.open("w", newline="", encoding="utf8") as handle:
        writer = csv.DictWriter(handle, fields)
        writer.writeheader()
        writer.writerows(rows)
    dimensions = sorted({(row.get("width"), row.get("height"), row.get("bits"), row.get("image_type")) for row in rows})
    checks = {"frames_exist": bool(rows), "starts_at_zero": indexes[0] == 0,
              "no_missing_numbers": not missing, "no_duplicate_numbers": not duplicate_indexes,
              "expected_frame_count": expected is None or len(rows) == expected,
              "all_frames_decode": not errors, "constant_image_format": len(dimensions) == 1}
    report = {
        "folder": str(folder), "prefix": next(iter(prefixes)), "fps": fps,
        "frame_count": len(rows), "first_frame": indexes[0], "last_frame": indexes[-1],
        "recorded_duration_seconds": len(rows) / fps, "bytes": total_bytes,
        "missing_frame_numbers": missing, "duplicate_frame_numbers": duplicate_indexes,
        "frame_formats_width_height_bits_type": dimensions, "checks": checks,
        "continuity_pass": all(checks.values()), "decode_errors": errors,
        "per_frame_manifest_and_metrics": str(csv_path),
        "regions_normalized_xyxy": REGIONS,
        "metrics": {key: summarize([row[key] for row in rows if key in row]) for key in fields
                    if key.endswith(("_mean_abs_change", "_changed_fraction_gt_24", "_mean_abs_second_difference"))},
        "motion_review_peaks": peaks,
        "limits": ["Pixel motion peaks are review candidates, not proof of defective or repaired poses.",
                   "The fixed rectangles match the original e3_lab 1024x640 view and scale with resolution.",
                   "The foreground workstation occludes part of the legs.",
                   "Frame seconds are relative to movie start; use native scene/pose timestamps to align timelines.",
                   "All source frames were read and hashed; none were modified or copied."],
    }
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--fps", type=float, default=60)
    parser.add_argument("--expected-frames", type=int, default=1632)
    parser.add_argument("--prefix")
    args = parser.parse_args()
    report = audit(args.folder, args.out, args.fps, args.expected_frames, args.prefix)
    print(json.dumps({key: report[key] for key in ("frame_count", "recorded_duration_seconds", "bytes", "checks", "continuity_pass")}, indent=2))
    return 0 if report["continuity_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
