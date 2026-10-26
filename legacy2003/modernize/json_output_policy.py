"""Maintain the line endings of published JSON manifests during replay.

The original Windows package shipped CRLF manifests. Python's write_text()
implicitly normalized them on other platforms, causing clean-room replay drift.
"""
import json
from pathlib import Path

def write_json(path, obj):
    path = Path(path)
    previous = path.read_bytes() if path.exists() else b''
    payload = json.dumps(obj, indent=2) + '\n'
    if b'\r\n' in previous:
        payload = payload.replace('\n', '\r\n')
    new = payload.encode('utf-8')
    if previous != new:
        path.write_bytes(new)
