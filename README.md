# WiiUNX2

WiiUNX2 is an independent Aroma-compatible Wii U homebrew menu with a
horizontal software row, touch navigation, search, sorting, user selection,
disc discovery and homebrew launching. It is not affiliated with, endorsed by
or sponsored by Nintendo.

The v0.6.12 release candidate does not bundle Nintendo firmware, game files,
console system files, or recorded Nintendo audio. Its optional UI cues are
original synthetic sounds generated without reference recordings. Game icons
are read from software installed on the user's console and are not distributed.
This statement does not establish legal clearance for the overall UI design or
inherited upstream assets; no legal outcome is guaranteed.

## Documentation map

- [MAINTAINING.md](MAINTAINING.md): architecture, lifecycle rules, build
  dependencies, handoff status and remaining hardware work.
- [AGENTS.md](AGENTS.md): contributor and AI-agent rules.
- [HOST-VALIDATION.md](HOST-VALIDATION.md): reproducible host checks and
  source-only packaging.
- [BUILD-VALIDATION.md](BUILD-VALIDATION.md): cross-compilation evidence and
  build limitations.
- [SECURITY.md](SECURITY.md): diagnostics, privacy boundaries and incident
  handling.
- [launcher/COMPATIBILITY.md](launcher/COMPATIBILITY.md): observed console
  compatibility and unverified plugin interactions.
- [launcher/THIRD-PARTY.md](launcher/THIRD-PARTY.md): upstream and dependency
  attribution.

## Architecture

The executable is built from `launcher/`. `Application` owns ProcUI lifecycle
and graphics recreation; `menu/` owns the row, dock, dialogs and controls;
`game/` discovers installed titles and SD homebrew; and `utils/` provides
bounded parsing, asynchronous work and atomic recent-history storage.
`dependencies/` contains the modified libgui renderer and the Aroma
RPXLoader/WUHB utility clients. The runtime modules are supplied by the
user's Aroma environment, not this source distribution.

The menu's normal path keeps GPU resources on the render thread and performs
metadata work on workers. Native play history is read-only. Recent homebrew
launch dates are stored locally on SD and never uploaded. Custom diagnostics
are disabled by default; see [SECURITY.md](SECURITY.md).

The on-screen search keyboard uses a QWERTY letter layout. The selected
software tile has a brightness pulse on its gradient outline. Mii avatars sit
on a white circular backing, and the multicolor ring rotates while the profile
or a dock item is selected.

## In-app updates

Open Options from the software row or dock, choose **Check updates**, then
choose **Install** only after reviewing the version prompt. The updater
downloads only the standalone `men.rpx` and `men.rpx.sha256` release assets
over verified HTTPS. It verifies both GitHub asset digests, checks that the
sidecar names `men.rpx` and matches its hash, and validates the Wii U RPX
header before touching the installed file. It installs only when
the active environment is confirmed as the standard Aroma environment at
`SD:/wiiu/environments/aroma`; it does not change plugins, modules, settings,
or caches. A verified timestamped `men.rpx.backup-*` is kept beside the
installed menu, and the console must be restarted after a successful update.

GitHub releases are not cryptographically signed. HTTPS and matching hashes
provide transport integrity, not independent publisher authentication. The
confirmation prompt states this limitation. Wii U TLS trust and SD replacement
behavior have not been proven on hardware; keep a manual SD recovery path.

## Build and validate

For host-only validation, install Node.js 18+ and a C++17 compiler, then run
from the repository root:

```powershell
node launcher/tools/Check-Public-Source.mjs
node launcher/tools/Test-Host.mjs
node launcher/tools/Audit-History.mjs
node launcher/tools/Package-Public-Source.mjs
```

For the Wii U binary, install devkitPro's Wii U toolchain and required
portlibs, including `wiiu-curl` for HTTPS updates. In devkitPro MSYS2, run
`pacman -S wiiu-curl`, then build with:

```powershell
powershell -ExecutionPolicy Bypass -File .\launcher\Build.ps1
```

Use `-DevkitPro` when devkitPro is installed outside `C:\devkitPro`.
Cross-build output is `launcher/men.rpx`; do not commit generated binaries.
The source-validation workflow is manual and does not publish binaries.

## Use on console

Build and verify `launcher/men.rpx`, keep its SHA-256 file beside it, and
review the target SD card before installation. Install it into the normal
Aroma environment:

```powershell
powershell -ExecutionPolicy Bypass -File .\launcher\Install-SD.ps1 -SDRoot E:\
```

This installs `men.rpx` at `SD:/wiiu/environments/aroma/men.rpx`, backing up
an existing file before replacing it. To install manually, power off the
console, back up any existing `men.rpx`, and copy the new file to that path.
To remove WiiUNX2, power off and restore the backup created during
installation; if there was no previous `men.rpx`, delete the one you added.

GitHub releases provide `men.rpx` and `men.rpx.sha256` as direct downloads.

For the experimental StyleMiiU RPX route, [download `stylemiiu.wps` directly
from the fork's latest release](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX/releases/latest/download/stylemiiu.wps).
Install it in `SD:/wiiu/environments/<environment>/plugins/`, put WiiUNX2 at
`SD:/wiiu/themes/WiiUNX2/men.rpx`, then select it under **WUPS Menu >
StyleMiiU RPX > Available Themes** with **Mashup themes** turned off. This
method requires Aroma's RPXLoadingModule and remains experimental; follow the
[fork's instructions](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX),
avoid competing menu-patching plugins during testing, and keep a recovery
path. Choose either this route or the direct Aroma `men.rpx` install.

`Update-SD.ps1` also updates the normal `aroma` environment and backs up an
existing `men.rpx` before replacing it. Never replace internal system files.

## Assets and licensing

Source includes modified libgui and the Aroma RPX loader/WUHB utility client
libraries. Retain all copyright and license notices. Noto Sans Regular is
included under SIL OFL 1.1 as a fallback; the console shared font remains
preferred. The original procedural background music and UI effects are
included. Local UI effects can override the bundled cues; see [SOUNDS.md](SOUNDS.md).

Nintendo firmware, game artwork, console dumps, recordings, logs and personal
play-history files are excluded. Existing customized test ZIPs are not
cleared public releases. The source licenses do not establish legal clearance
of the overall visual design.
