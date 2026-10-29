# GammaSource launcher entry

`GammaSource Launcher.exe` is a standalone launcher. It loads the Source tools'
VGUI, material system and studio model renderer through `bin/gamma_launcher_ui.dll`.
It does not load `engine.dll`, the game client/server, a map, or the game process
while choosing a name and model. The window uses the game's actual VGUI controls
and `SourceScheme.res`, including its borders, with a live animated MDL preview.

Place the executable next to `hl2_launcher.exe`, with `gammasource/gameinfo.txt`
below that folder. Only clicking Play starts `hl2_launcher.exe`, with
`-game gammasource -novid`, the selected name/model/third-person preference,
and any additional user launch options. The game starts in its own folder,
independent of the shortcut's working directory. Closing the launcher exits
without starting the game. Choices are saved in `gammasource/cfg/gamma_launcher.vdf`.

Build the entry with `py -3 waf build --targets=gamma_launcher`. Normal Windows
game builds also include this target. The output is
`build/launcher_main/GammaSource Launcher.exe` and
`build/launcher_main/gamma_launcher_ui.dll`. The UI uses the renderer/toolkit
DLLs already present in the game's `bin` directory; it needs no separate runtime.
`stage_gammasource.ps1`'s underlying staging script copies this entry when built,
and the staged `play.cmd` opens it. Older installations without this executable
retain their existing direct-game entry.

The command-line test exercises the production quoting helper against Windows'
own `CommandLineToArgvW`, including embedded quotes, trailing backslashes,
whitespace, empty values and Unicode. Native UI and model preview verification
is performed separately with the standalone UI DLL.

## Renderer integration checks

The optional `-launcher_verify <absolute-output-directory>` mode exercises the
launcher's own controls and renderer. It selects every roster entry, saves two
TGA frames per model directly from the material-system framebuffer, and reports
the selected model, neutral sequence, bone count, animation clock and preview-only
pixel hashes in `launcher_verification.json`. It does not capture the desktop or
send operating-system input. It also checks that engine/client/server modules
are absent, then exits without starting the game.

Adding `-launcher_verify_play` invokes the same Play handler after those captures,
using the explicit fixture profile `Gamma Launcher Test`, `chell_2010`, third person
enabled. The report records the actual child process and command line. Additional
game options are forwarded normally, allowing an isolated map fixture to verify
the resulting in-game profile. Verification flags are never forwarded to the game.
