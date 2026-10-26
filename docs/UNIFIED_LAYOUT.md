# Unified GammaSource directory

All development now starts inside **one `GammaSource/` directory**. There is no separate `source-engine-master/` or `GammaSource-main/` checkout.

```text
GammaSource/
  README.md                     Updated user-provided project README
  waf, waf.bat, wscript         Active x64 compiler entry points
  build_gammasource.cmd         One-command x64 build wrapper
  stage_gammasource.ps1         One-command packaging wrapper
  app/, engine/, game/, ...     Active game and engine code
  gammasource_content_2018/    Live beta resource overlay
  legacy2003/                   Preserved 2003 game source and conversion scripts
  tools/                        Independent project helper tools
  verification/                 Previous recorded validation and visual evidence
  docs/                         Handoff, historical READMEs and migration notes
```

The original archive's active engine files were moved **without changing their relative internal layout**, so Waf subproject resolution still uses the root `wscript`. The independent 2003 tree is retained under `legacy2003/` rather than overwrite active files that have the same names (such as `engine/`, `gameui/`, `materialsystem/`, `public/`, and `README.md`).

## Build from this single root

In an x64 Native Tools Command Prompt:

```bat
cd /d C:\path\to\GammaSource
build_gammasource.cmd configure
build_gammasource.cmd
```

Once configured, subsequent builds need only `build_gammasource.cmd`. You can use `py -3 waf build -j16 -k` directly if preferred. The provided October 25 handoff recorded a successful **incremental** build on the user's Windows installation. A clean build in this consolidated folder has **not** been executed here.

## Stage the game

Run `stage_gammasource.ps1` as shown in [README.md](../README.md). It sets `-Repo` to the root automatically. It still needs the external compiled engine install and original game assets; this source ZIP **does not include** the approximately 16 GB playable game content. The supporting historical build/staging logic remains in `legacy2003/modernize/port2018/make_build.ps1`, but developers normally use the new root wrapper.

The `legacy2003/` folder is reference source, **not a second Waf build**, and some archived old scripts refer to historical directory layouts. A small set of **reference-only historical HUD/viewmodel files** from an earlier Round 8 source archive has been restored under `legacy2003/gammasource_content/` so the old source tests remain usable. These files are **not staged into the active game** by the unified packaging command. Do not run old broad port/sync scripts blindly: they can be non-idempotent and may affect running processes.

## Preserved behavior and remaining verification

The packaging preserves the October 25 friendly-fire source patch and the beta overlay. No gameplay, renderer, or animation code was changed for this consolidation. The required loading screen is the original **green dialog on a plain black background**, not the older branded artwork. Normal choreographed scene body animation, `zoo_item` sky/physgun/HEV visual checks, and friendly-fire runtime toggle remain unverified here.

## Historical manifest

`docs/GammaSource_2018_FRIENDLY_FIRE_MANIFEST.json` retains the original names and hashes from before consolidation. Its old paths reflect the input ZIP, not the new directory; use `UNIFIED_LAYOUT_MANIFEST.json` for consolidated file mapping.
