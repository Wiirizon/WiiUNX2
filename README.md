# WiiUNX2

WiiUNX2 is an independent Aroma-compatible Wii U homebrew menu with a
horizontal software row, touch navigation, search, sorting, user selection,
disc discovery and homebrew launching. It is not affiliated with, endorsed by
or sponsored by Nintendo.

The v0.7.4 release does not bundle Nintendo firmware, game files,
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
If no default Wii U account is configured, WiiUNX2 asks who is using the
console at startup and remembers the choice for the rest of that system boot.

WiiUNX2 still enumerates installed titles and scans `SD:/wiiu/apps/` at each
startup so it can notice additions, removals and changed visibility settings.
The persistent `wiiu/WiiUNX2/names.cache` reuses eligible display names only;
it does not cache the app list, icons or GPU textures. Re-discovery and icon
loading on each boot are therefore expected.

WiiUNX2-owned data (recent history, the name cache, account marker, and
opt-in diagnostics) is stored under `SD:/wiiu/WiiUNX2/`. On first launch after
upgrading, files from the previous `SD:/wiiu/` layout are moved there along
with their recovery sidecars. After a successful pass, a versioned completion
marker lets later launches skip the legacy-directory scan; if migration is
incomplete or the marker is missing or invalid, it retries. Existing
destination files are never replaced; if both layouts contain a file, both
are kept and the organized copy is used.
The loader-required `men.rpx` paths, the StyleMiiU plugin, and updater staging
and rollback backups remain outside this folder by design.
The updater's rollback backup contains the RPX, not these data files. If you
restore a pre-v0.7.12 RPX and want its old history/cache/logs, copy the files
back to the legacy `SD:/wiiu/` names.

The on-screen search keyboard uses a QWERTY letter layout. The selected
software tile has a brightness pulse on its gradient outline. Mii avatars sit
on a white circular backing, and the multicolor ring rotates while the profile
or a dock item is selected.

## In-app updates

Open Options from the software row or dock, choose **Check updates**, then
choose **Install** only after reviewing the version prompt. The updater
downloads only the standalone `men.rpx` and `men.rpx.sha256` release assets
over verified HTTPS using the bundled GitHub trust anchors. It verifies both
GitHub asset digests, checks that the
sidecar names `men.rpx` and matches its hash, and validates the Wii U RPX
header before touching the installed file. It confirms the active environment
is standard Aroma, then asks RPXLoadingModule for the running executable path.
For the StyleMiiU route it updates the exact `SD:/wiiu/themes/.../men.rpx`
currently running; for a direct install it updates
`SD:/wiiu/environments/aroma/men.rpx`. A verified timestamped
`men.rpx.backup-*` is kept beside the selected RPX, and the console must be
restarted after a successful update. The in-app updater keeps one verified
backup: the version immediately before the latest update. Older timestamped
in-app backups are removed only after a new backup has been verified; backups
made manually or by `Update-SD.ps1` are left untouched. If the SD filesystem
refuses to replace the existing file directly, the updater uses a same-directory
rename fallback;
keep the console powered on and do not remove the SD card until it finishes.
If power is lost during that brief replacement window, restore the verified
backup. If RPXLoadingModule cannot report the
running theme path (API v2+), the updater fails safely and asks for a manual
update rather than guessing. It does not update the StyleMiiU `.wps` plugin or
change other plugins, modules, settings, or caches.

GitHub releases are not cryptographically signed. HTTPS and matching hashes
provide transport integrity, not independent publisher authentication. The
confirmation prompt states this limitation. The updater bundles ISRG Root X1
and USERTrust ECC for GitHub's currently observed API and asset chains; peer
and hostname verification remain enabled. Wii U TLS trust and SD replacement
behavior have not been proven on hardware. Check the console date/time and
keep a manual SD recovery path.

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

### Install using StyleMiiU

1. Download [`stylemiiu.wps` from the latest release](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX/releases/latest/download/stylemiiu.wps).
2. Install it in `SD:/wiiu/environments/<environment>/plugins/`, following
   the [plugin instructions](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX#readme).
3. Put WiiUNX2's `men.rpx` at `SD:/wiiu/themes/WiiUNX2/men.rpx`.
4. In **WUPS Menu > StyleMiiU RPX > Available Themes**, select WiiUNX2 with
   **Mashup themes** turned off.

This method requires Aroma's RPXLoadingModule. The linked plugin is a
community RPX-compatible fork of [ThemeCafe's StyleMiiU-Plugin](https://github.com/ThemeCafe/StyleMiiU-Plugin).
It launches WiiUNX2 as homebrew through a wrapper rather than as the Wii U
Menu process. Some titles have been reported to hang in this route from both
WiiUNX2 and [Launchiine](https://github.com/vbarros22/launchiine); the same
titles reportedly work with the direct Aroma `men.rpx` install. The cause is
not yet confirmed, so use the direct install for affected titles and keep a
recovery path. Avoid competing menu-patching plugins during testing.

When WiiUNX2 runs through StyleMiiU, the system's software-suspension overlay
may show `wiiu/themes/WiiUNX2/men.rpx` instead of a friendly name. The
RPXLoadingModule API used by WiiUNX2 provides no supported display-name
override. The path is also used to identify the running RPX for updates, so
WiiUNX2 leaves it unchanged; it does not patch system titles or use an
unverified system-menu hook to change the label.

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
