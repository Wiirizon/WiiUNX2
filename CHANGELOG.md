# WiiUNX2 changelog

## 0.7.13 — Stable release

- Write a versioned data-layout completion marker after a fully successful
  migration so later launches skip scanning the legacy Wii U data directory.
- Retry migration if any file operation or the recent-history scan fails, or
  if the completion marker is missing or invalid.

### Validation and known limitations

- Host tests cover marker-content validation, path selection and migration
  error handling. The Windows host runner and Wii U cross-build still need to
  be recorded for this release.
- The marker and migration behavior on an actual Wii U SD card still need
  console testing.

## 0.7.12 — Stable release

- Group WiiUNX2-owned history, name cache, account marker and opt-in
  diagnostics under `SD:/wiiu/WiiUNX2/`.
- Migrate existing files and recovery sidecars from `SD:/wiiu/` without
  overwriting files already present in the new folder.
- Keep `men.rpx`, StyleMiiU plugin files, and updater staging/rollback files
  at their required or transaction-safe locations.
- Note that restoring an older RPX does not automatically move data files back
  to the legacy layout.

### Validation and known limitations

- The [Wii U cross-build](https://github.com/Wiirizon/WiiUNX2/actions/runs/45478735371)
  and [Ubuntu host/source validation](https://github.com/Wiirizon/WiiUNX2/actions/runs/45478742936)
  passed, including path/migration tests and source archive checks.
- The SD directory migration still needs console testing. Existing destination
  files are not overwritten; unresolved legacy files remain available at the
  old path when a migration fails.

## 0.7.11 — Stable release

- Prune older in-app updater backups only after a new verified pre-update
  backup has been safely published.
- Add opt-in diagnostics for empty and failed Wii-disc ODD queries.

### Validation and known limitations

- The [GitHub Wii U cross-build](https://github.com/Wiirizon/WiiUNX2/actions/runs/44969720030)
  and [Ubuntu source/host validation](https://github.com/Wiirizon/WiiUNX2/actions/runs/44969726410)
  passed, including the host regression suite and source archive check.
- Backup rotation and ODD diagnostics have not yet been tested on Wii U
  hardware; this release is intended for that console test.
- The user reports all tested titles launch through the StyleMiiU RPX fork;
  Wii-format discs still do not appear. Use the opt-in ODD diagnostics to
  distinguish an empty result from an MCP query error.

## 0.7.10 — Stable release

- Document the StyleMiiU suspension-overlay path label. The RPXLoadingModule
  API used by WiiUNX2 exposes no supported display-name override, so the
  executable path remains unchanged for correct updater target detection.

## 0.7.9 — Stable release

- Support SD filesystems that reject replacing an existing RPX with `rename()`.
  The updater now uses a same-directory rename fallback after publishing a
  verified backup, and attempts rollback if the new RPX cannot be installed or
  verified.

### Validation and known limitations

- Host transaction tests cover the non-overwriting rename path, install failure,
  rollback, and recovery-required handling. Hardware testing is still needed.
- The fallback briefly moves the old RPX out of the active path. Keep the
  console powered on and do not remove the SD card during installation; a
  power loss in that brief interval may require restoring the verified backup.

## 0.7.8 — Stable release

- Stop the title-loader and pending readers before title handoff; use the Wii U
  no-splash title-launch API for regular titles.
- Ask for an account at startup when no default user is configured, once per
  system boot.
- Document that some title launches still hang through the StyleMiiU RPX
  wrapper and that the attached Launchiine files did not include its UDP log.

### Validation and known limitations

- Focused account-prompt host tests pass. The full local host runner is blocked
  by Windows Application Control, and the local Wii U build is blocked by an
  installed libgui/WUT thread-entry signature mismatch.
- The launch changes and startup account prompt still require console testing.
  Title-specific hangs through StyleMiiU RPX remain unresolved; use direct
  Aroma `men.rpx` replacement for affected titles.

## 0.7.7 — Stable release

- Show RPX download progress and percentage in the in-app updater, with
  separate status messages for verification and installation.
- Animate the selected account rectangle with the same moving gradient used
  by the selected app highlight.
- Document that recent-history filenames are indexed by active ACT account
  slot.

### Validation and known limitations

- Focused host tests for gradient timing and updater core logic pass. The full
  local host runner is blocked by Windows Application Control, and the local
  Wii U build is blocked by an installed libgui/WUT thread-entry signature
  mismatch; the [GitHub Wii U build passed](https://github.com/Wiirizon/WiiUNX2/actions/runs/44937910123).
- Progress rendering, account-highlight appearance, and update installation
  still require Wii U hardware validation.

## 0.7.6 — Stable release

- Detect Wii discs reported as `MCP_APP_TYPE_GAME_WII`, show a dedicated disc
  tile, and request launch through the system's `SYSLaunchTitle` path.
- Keep the existing Wii U disc launch path unchanged; do not apply Wii U title
  patches to Wii discs.

### Validation and known limitations

- Added host coverage for Wii-platform samples, platform swaps, and the Wii
  disc launch-log record. Focused launch-log and version tests pass.
- Windows Application Control blocked execution of the compiled disc-state
  test binary here; GitHub Linux validation must confirm the host suite.
- ODD detection, Wii-mode launch, and return behavior still require Wii U
  hardware validation. Issue #28 remains open pending those tests.

## 0.7.5 — Stable release

- Keep update-check and installation progress visible until the background
  operation reports a result. Allow cancellation requests without interrupting
  protected file operations.

### Validation and known limitations

- Host updater transaction tests and the GitHub trust-bundle test pass.
- A v0.7.3 console detected v0.7.4 and verified its target and checksum, but
  remained on v0.7.3 after the install attempt. The stopping stage is unknown;
  v0.7.5 installation, cancellation, and rollback still require Wii U testing.

## 0.7.4 — Stable release

- Increase the gap between the selected app's animated gradient border and its
  icon to eight logical pixels.
- Bundle the GitHub trust anchors used by the verified in-app update check.

### Validation and known limitations

- The user reports that a Wii U running 0.7.3 now completes the update check
  without an HTTPS error and correctly reports 0.7.3 as up to date. This
  confirms the check-only path in that setup, not downloading or installing
  an update.
- The generated focus-frame clearance and updater trust-bundle checks pass on
  the host. Console validation of the new border spacing, downloading,
  installation, and rollback is still required.

## 0.7.3 — Stable release

- Correct the account-selection heading from "Select a user" to "Select an
  account."
- Keep the update check in the main-menu options view instead of showing it
  as an action inside the selected-game options view.
- Make options controls more explicit and easier to identify.
- Rotate rectangular selected-app borders while cycling their cyan-blue RGB
  phases, matching the existing 480-frame animation cadence.
- Improve HTTPS update-check failures by identifying curl connection and
  certificate errors before suggesting a Wii U internet check.
- Investigate title launch handoffs and change normal installed-title
  launches to use the MCP-resolved title path, matching disc and custom-title
  handoffs. The first release candidate required a build-type correction;
  console confirmation is still required.

### Validation and known limitations

- Theme and source checks cover the account wording, options layout,
  rectangular highlight motion, update transport, and existing menu behavior.
- The Windows host runner could not reliably execute every generated test
  executable in this environment. Wii U cross-build and hardware retesting
  are still required.
- Mario Kart 8 and NSMB2 via 3Dii U remain unconfirmed and may be affected
  by plugins, emulator compatibility, or title handoff behavior.

## 0.7.2 — Stable release

- Make the selected app's cyan-blue focus border cycle more visibly, matching
  the dock/profile ring's 480-frame animation cadence.
- Skip TV-only FXAA post-processing to test whether it causes reported
  softening of small glyphs and punctuation; TV output needs a new console
  comparison.
- Prepare an updater-test release so stable v0.7.1 installations can discover
  v0.7.2 through GitHub's latest-stable-release endpoint.
- Present StyleMiiU installation as a numbered setup path and identify its
  RPX-compatible plugin as a fork of ThemeCafe's StyleMiiU plugin.

### Validation and known limitations

- The release-source GitHub Actions Wii U build and Linux host/source
  validation passed: [build](https://github.com/Wiirizon/WiiUNX2/actions/runs/44154022191),
  [source validation](https://github.com/Wiirizon/WiiUNX2/actions/runs/44154045968).
- The tag-triggered build must also pass before it publishes release assets.
- TV sharpness, tile animation, and online update installation still need
  console testing.

## Before the tagged releases — theme-building phase

- Early project records show a Wii U theme builder modifying `Men.pack` and
  `Men2.pack` from US 5.5.5. The builder was updated to accept their FLYT 2.2.0
  layouts and produced a pale-gray, white and cyan theme based on the supplied
  Switch 2 UI reference. That theme retained the Wii U's native grid.
- The project later evolved into the standalone `men.rpx` menu replacement.
  No surviving records establish whether the theme work was released as v0.1
  or v0.2, or what those numbered versions contained. v0.3 is the oldest
  recoverable tagged source release, not necessarily the start of development.

Tags v0.3 through v0.6.10 are reviewed, reconstructed source snapshots rather
than the original commit-by-commit history; the entries below consolidate the
release details that could be recovered, but may not list every interim
change.

## 0.7.1 — Stable release

- Detect the currently running RPX with RPXLoadingModule API v2+, so StyleMiiU
  installations update the active `SD:/wiiu/themes/.../men.rpx` rather than
  requiring a second copy in Aroma's environment.
- Show the detected destination before installation, recheck it before
  replacing the file, and keep the verified backup beside that RPX. If the
  active path cannot be safely identified, automatic installation fails
  closed. The updater does not update the StyleMiiU `.wps` plugin.
- Add StyleMiiU setup instructions for the community
  [RPX-compatible fork](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX) of
  [ThemeCafe's original plugin](https://github.com/ThemeCafe/StyleMiiU-Plugin),
  with a direct [latest `stylemiiu.wps` download](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX/releases/latest/download/stylemiiu.wps).

### Validation and known limitations

- GitHub Actions Wii U cross-build and Linux host/source validation passed,
  including the full host suite, history audit and source archive check.
- Windows Application Control blocked the full host runner on this PC.
  Hardware validation of RPXLoadingModule path reporting and SD replacement
  is still required.

## 0.7 — Stable release

- Add a manual in-app updater for standalone `men.rpx` releases, with
  HTTPS/digest/checksum verification, a verified backup and rollback handling.
- Add the profile backing, ring and selection visual refinements; restore
  bundled menu sounds with optional local overrides.
- Document normal Aroma installation and the experimental StyleMiiU route.
- Track the softer TV rendering compared with the GamePad in issue #38.

### Validation and known limitations

- The GitHub Actions Wii U cross-build passed. Hardware validation of the
  updater and visual/audio changes remains outstanding.

## 0.6.12-rc.3 — Release candidate

- Arrange the search keyboard in QWERTY order and center the D-pad glyph.
- Keep the profile ring visible and add a subtle selected-tile brightness
  pulse, continuous across the 32-bit millisecond clock wrap.
- Update compatibility, maintenance and launch-diagnostic documentation.

### Validation and known limitations

- Wii U cross-build and source-publication/history checks passed; the full host
  suite remained incomplete because Windows App Control blocked a test binary.
- UI changes had not been validated on Wii U hardware. Wii-format discs remain
  unsupported.

## 0.6.12-rc.2 — Release candidate

- Replace optional UI tones with shorter, sharper procedural navigation,
  confirm and back cues.
- Add deterministic generation, duration and sample-bound checks for the cues.

### Validation and known limitations

- Wii U build and release packaging passed. Console listening validation was
  still pending; no recorded Nintendo audio was included.

## 0.6.12-rc.1 — Release candidate

- Add faster held keyboard navigation and original procedural UI cues.
- Add bounded icon prefetch/residency and Wii U disc polling with error backoff
  and removal debounce.
- Add release/source validation and packaging tools; make Windows MSYS Git
  paths work in release metadata.
- Add host-testable return-list reconciliation and persistent metadata-cache
  format helpers; neither helper was wired into the runtime at this point.

### Validation and known limitations

- Wii U game discs were reported working, as were Super Smash Bros. for Wii U
  and Kirby Wii U Game. Meme Run and Bridge Constructor were reported hanging
  at the loading screen; reproducibility and stock-menu behavior were unknown.
- Keyboard repeat, optional sound cues, icon prefetch and disc swaps still
  needed hardware testing. Wii discs were unsupported.

## 0.6.11

- Add hold-to-accelerate L/R software-row navigation while preserving
  immediate single-press behavior.
- Add host regression coverage for repeat timing boundaries.
- Bundle the pinned, OFL-licensed Noto Sans fallback and verify clean-source
  builds; retain the console's shared font as first choice.

## 0.6.10-test

- Reuse installed-title CPU metadata after applet returns while validating
  cached entries against title ID, path and version. Full process restarts
  still reload metadata.
- Add reviewed source publication, host validation and source-only packaging
  workflows.

## 0.6.9-test

- Make recent-history saves safer by writing a replacement before rotating the
  prior file, with backup/recovery tests.
- Tighten public-source exclusions for logs, history, firmware, audio and
  generated artifacts; keep custom diagnostics opt-in.

## 0.6.8-test

- Honor Aroma Homebrew on Menu visibility settings, including “Hide all
  homebrew,” the Homebrew Launcher exception, RPX visibility and WUHB
  preference.

## 0.6.7-test

- Publish installed titles before finishing the SD homebrew scan to reduce
  time-to-first-title.
- Reduce filesystem calls during homebrew scanning.

## 0.6.6-test

- Discover and launch RPX/WUHB homebrew directly through Aroma's loader.
- Read WUHB names/icons and RPX sidecar icons, honor `wiiu/apps/.ignore`, and
  preserve navigation while titles arrive during loading.
- Keep native recent-play sorting.

## 0.6.5-test

- Add native active-user play-history sorting using day-level PDM records.
- Use local launch history to order same-day records and cover homebrew.

## 0.6.4-test

- Start at the left of the software row and preserve selection through loading.
- Add recently-played sorting and GamePad touch-drag scrolling.
- Adjust Aroma app launch preparation; hardware validation was still needed.

## 0.6.3-test

- Add Wii U disc insertion/ejection detection and an initial Aroma RPX/WUHB
  discovery and launch path.
- Require hardware testing of disc and homebrew behavior.

## 0.6.2

- Refine the Friends dock icon so the two silhouettes meet without crossing.
- Runtime behavior was otherwise unchanged from 0.6.1.

## 0.6.1

- Remove TVii from the dock while retaining its software-row entry.
- Refine the Friends and controller dock icons and center the nine-icon dock.

## 0.6

- Add TVii discovery and applet launching.
- Add touch-capable button-badge prompts and refine the dock layout/icons.
- Add menu sound effects and dock captions.

## 0.5.2

- Round account-picker panels, cards, dialogs and search keys.
- Move the HOME profile label beside the avatar and restore the selected-user
  gradient.
- Refine dock icons and browser/Wi-Fi symbols while retaining their Wii U
  functions.

## 0.5.1

- Replace square card edges with transparent rounded corners and softer
  shadows.
- Align footer controls using measured text widths and baselines.
- Prefer the console system font when available.

## 0.5

- Move metadata and icon file reads off the rendering thread.
- Add case/accent-insensitive search, sorting and keyboard controls.
- Add rounded cards, refined selection/dock visuals and an original ambient
  music loop.

## 0.4

- Fix unknown titles, language fallbacks and accented names.
- Improve icon loading and graphics cleanup after startup and Mii Maker
  returns; add a profile/user picker.
- Add a backup-aware SD updater and bounded diagnostics.

## 0.3 — Initial experimental release

- Introduce the horizontal software row, Switch-inspired dock, status
  indicators, controller/touch navigation and Wii U applet shortcuts.
- Package an SD install path that preserved the user's normal Aroma
  environment. Homebrew shortcuts and several stock-menu features were
  incomplete; console testing was outstanding.
