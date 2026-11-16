# WiiUNX2 changelog

## Unreleased

- Restore menu sound effects in clean builds by bundling the original generated cues, with local effects retained as optional overrides.
- Add a white circular outline around the profile icon while its row is focused, and make the selected-tile brightness pulse more visible.
- Updater core (not wired into the menu): the install path is now one exact archive entry, `sd-layout/wiiu/environments/aroma-wiiunx2/men.rpx`, mapped to `wiiu/environments/aroma-wiiunx2/men.rpx`. Other entries, including module files, are rejected instead of accepted by prefix.

## 0.6.12-rc.3 — Release candidate

- Arrange the search keyboard in QWERTY order and center the D-pad glyph in its badge.
- Keep the profile ring visible and add a subtle, original brightness pulse to the selected tile outline.
- Keep the selected tile outline pulse continuous across the 32-bit millisecond clock wrap (about 49.7 days of uptime) instead of stepping once.
- Record the latest Wii U disc and launch-diagnostic reports in the project documentation.

### Validation and known limitations

- Wii U RPX cross-build and source-publication/history checks passed.
- Standalone Node tests: 33 passed, 3 skipped due to Windows filesystem limitations.
- The full host suite remains incomplete: Windows App Control blocked the generated `library.exe` test binary.
- UI changes have not been validated on Wii U hardware. Wii-disc support remains unimplemented.

## 0.6.12-rc.2 — Release candidate

- Replace the optional synthetic UI tones with shorter, sharper procedural click cues for navigation, confirm, and back.
- Validate the click generator for deterministic output, duration, and sample bounds.

### Validation and known limitations

- Host tests and release packaging pass; Wii U RPX rebuilt with the generated cues.
- The revised sounds still need listening validation on Wii U hardware. This prerelease does not claim that they match any specific console UI sounds.
- No Nintendo firmware, game files, console system files, or recorded Nintendo audio are included.

## 0.6.12-rc.1 — Release candidate

- Add faster held D-pad/left-stick navigation in the search keyboard.
- Add original synthetic move, click, and back cues for local builds.
- Add bounded directional icon prefetch and residency planning.
- Add Wii U disc polling with error backoff and removal debounce. Wii U game disc use is reported working on hardware; Wii disc support is not implemented.
- Add release packaging and source-validation tooling.
- Normalize MSYS-style Git paths on Windows so release metadata can capture the source commit.
- Include host-testable return-list reconciliation and persistent metadata-cache format helpers. Neither is wired into the runtime; persistent metadata caching is not provided.

### Validation and known limitations

- Built for Wii U and locally packaged as a release candidate.
- Hardware report: Wii U game discs work; Super Smash Bros. for Wii U and Kirby Wii U Game launched.
- Meme Run and Bridge Constructor hung at the Wii U loading screen in reported attempts. Repeatability and stock-menu behavior remain untested; see issue #17.
- Keyboard repeat, optional synthetic sound cues, icon-prefetch performance, and disc removal/swap behavior need further hardware validation.
- The package-release tests now pass on this Windows environment (20 passed, 3 skipped). The full host runner could not complete because Windows Application Control blocked a generated test executable.
- This project is unofficial. This candidate bundles no Nintendo firmware, game files, console system files, or recorded Nintendo audio. Its optional UI cues are synthesized without reference recordings. This is not a legal-clearance statement for the UI design or inherited upstream assets.
