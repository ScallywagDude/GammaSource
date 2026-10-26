#!/usr/bin/env python3
"""Restore repair-gun priority for injured vehicle passengers.

The source encoding and line endings are retained. Reapplication is a no-op.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "game_shared/tf2/weapon_repairgun.cpp"
BEFORE = """\t\t\t\tif ( pPlayer->GetHealth() < pPlayer->GetMaxHealth() )
\t\t\t\t{
\t\t\t\t\tpUnhurtPlayer = pPlayer;
\t\t\t\t}
\t\t\t\telse
\t\t\t\t{
\t\t\t\t\tpHurtPlayer = pPlayer;
\t\t\t\t}"""
AFTER = """\t\t\t\t// HL2MODERN_REPAIRGUN_PRIORITY: heal injured passengers first.
\t\t\t\tif ( pPlayer->GetHealth() < pPlayer->GetMaxHealth() )
\t\t\t\t{
\t\t\t\t\tpHurtPlayer = pPlayer;
\t\t\t\t}
\t\t\t\telse
\t\t\t\t{
\t\t\t\t\tpUnhurtPlayer = pPlayer;
\t\t\t\t}"""


def restore(source):
    if AFTER in source:
        return source
    if source.count(BEFORE) != 1:
        raise ValueError("repair-gun priority patch target missing or ambiguous")
    return source.replace(BEFORE, AFTER, 1)


def main():
    path = ROOT / SOURCE
    original = path.read_bytes()
    crlf = b"\r\n" in original
    source = original.decode("latin1").replace("\r\n", "\n")
    updated = restore(source)
    if updated != source:
        path.write_bytes((updated.replace("\n", "\r\n") if crlf else updated).encode("latin1"))
    print("Repair-gun passenger priority restored; build and gameplay verification still required.")


if __name__ == "__main__":
    main()
