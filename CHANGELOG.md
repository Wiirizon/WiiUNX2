# WiiUNX2 changelog

The oldest recoverable release is v0.3. No v0.1 or v0.2 release is present in
the available tags or project history. Tags v0.3 through v0.6.10 are reviewed,
reconstructed source snapshots rather than the original commit-by-commit
history; the entries below consolidate the release details that could be
recovered, but may not list every interim change.

## 0.7.1 — Stable release

- Detect the currently running RPX with RPXLoadingModule API v2+, so StyleMiiU
  installations update the active `SD:/wiiu/themes/.../men.rpx` rather than
  requiring a second copy in Aroma's environment.
- Show the detected destination before installation, recheck it before
  replacing the file, and keep the verified backup beside that RPX. If the
  active path cannot be safely identified, automatic installation fails
  closed. The updater does not update the StyleMiiU `.wps` plugin.
- Add the experimental [StyleMiiU RPX plugin](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX)
  and [latest `stylemiiu.wps` download](https://github.com/Wiirizon/StyleMiiU-Plugin-RPX/releases/latest/download/stylemiiu.wps)
  to generated build information.

### Validation and known limitations

- Focused updater policy, replacement and target-resolution tests passed;
  Wii U-targeted syntax checks and the public-source check passed.
- The full host runner was blocked by Windows Application Control, and the
  local full build was blocked by a devkitPro `CThread.h` callback-signature
  mismatch. Hardware validation of RPXLoadingModule path reporting and SD
  replacement is still required.

## 0.7.0 — Stable release

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
