# Disc detection

Installed titles are enumerated only from MLC and USB using `MCP_TitleListByAppAndDeviceType`; this excludes the ODD so a disc-only title cannot appear once as installed software and again as a disc. The menu separately polls the optical drive (`MCP_TitleListByDeviceType`, device `ODD`) from the metadata worker thread once loading has finished. A title genuinely present on both MLC/USB and disc appears twice to distinguish the installed copy from the disc launch option. The selected disc title has a larger disc icon to the left of its name instead of a `(Disc)` suffix. On insertion, it ranks first only while using **Recently played** sort; this ephemeral promotion does not write to play history or change selection. The decision logic is in `launcher/src/menu/DiscState.h` (`nx::DiscMonitor`), a pure state machine covered by `launcher/tests/disc-state.cpp`. The thin reader in `MainWindow.cpp` (`readDisc`) talks to the console API, and the loop reads `meta.xml` from the SD/disc file system; neither is host-tested.

## Rules (the state machine is proven by host tests on synthetic samples)

| Sample | Meaning | Effect |
|---|---|---|
| Game, title ID not 0 | ODD entry has `MCP_APP_TYPE_GAME` (Wii U) or `MCP_APP_TYPE_GAME_WII` (Wii) | First one is reported as Inserted at once; a different ID or platform is Changed |
| Empty | Drive list has no entries | Removed after 2 consecutive samples (2 to 4 s by the code, not measured on hardware). A single empty sample is ignored |
| Other | Entries exist, but none has either supported game app type | With no game held, reported once as `Unsupported` per unsupported period; the menu shows a generic unsupported-disc message and adds no tile. Repeats are silent until an Empty or Game sample arrives. With a game held, two Other samples remove it first (`Removed`), and `Unsupported` can follow after that |
| Error | MCP open failed, call returned an error, or more than 4 entries | State unchanged. The poll delay doubles per consecutive error, 2 s up to 16 s, and resets on a valid sample |
| Game with title ID 0 | Invalid | Treated like an error |

Wii U disc metadata (`meta.xml`) is read only when the monitor reports Inserted or Changed, not on every poll. Wii discs use a generic "Wii Game Disc" tile because Wii disc metadata parsing is not implemented. Removal takes two samples, so a transient empty or error reply does not make the tile flicker.

## Wii-disc launch path and remaining unknowns

- An ODD entry with `MCP_APP_TYPE_GAME_WII` is shown as a Wii game disc. Selecting it calls `SYSLaunchTitle` with the title ID reported by MCP. This requests a system-managed title launch after WiiUNX2 exits; it deliberately does not use the Wii U disc path handoff or `ACPAssignTitlePatch`. The Wii U disc launch path is unchanged.
- The launch request is based on the MCP app type and WUT's documented system title-launch API, but the complete disc-to-vWii transition is unverified on hardware. The ODD entry, returned title ID/path, successful Wii-mode launch, and return behavior must still be confirmed. Keep issue #28 open until those checks pass.
- The installed Wii U title path uses `SYSLaunchTitle` to let the system perform its normal title transition. The previous call explicitly requested the no-splash variant; showing a game splash through the system-managed call is not yet confirmed on hardware.
- No NAND or system-title changes are made. The code only reads MCP's ODD list, requests the system title launch, and records the existing opt-in launch diagnostic.
- Other states not handled: dirty or unreadable discs (expected to appear as Error or Empty), a disc inserted while the drive is busy, discs of another region, and discs needing an update. The 2 s base interval is not proven to match stock timing.

Diagnostics: each distinct Wii disc app type and unsupported app type gets one debug-log line until the drive state changes; that ODD-type line contains no title ID. On the first empty ODD result per run, WiiUNX2 also queries MCP's `AUTO` device list once and logs only Wii-game app types and ODD device labels, without title IDs or paths. For this diagnostic, insert the Wii disc before starting WiiUNX2 so it is present during the first poll. The separate opt-in launch log may contain title IDs and paths as documented in MAINTAINING.md. The generic unsupported notice is shown once per unsupported period (deferred while another message or a launch is active; a deferred notice is dropped if the drive reads empty or a game before it is shown) and is independent of the log line. Insertion, launch, Wii-mode return, and polling behavior remain UNVERIFIED on console.

## Hardware checklist for the maintainer

- [ ] Insert and eject a Wii U game disc: tile appears, launches, and disappears about 4 s after eject.
- [ ] Swap two game discs: tile changes name without a stuck tile.
- [ ] Insert a Wii disc: check for a "Wii Game Disc" tile and record its ODD app type without sharing title IDs.
- [ ] If no tile appears, start the diagnostic build with the Wii disc already inserted and share the fresh `debug.log` lines beginning with `Disc ODD` and `MCP AUTO`; they contain no title IDs or paths.
- [ ] Select the Wii disc tile: verify whether the system enters vWii and whether the expected return path works. Do not change NAND or system titles.
- [ ] Insert a scratched or unreadable disc: no flicker and no log spam.

## Latest direct-Aroma test report (2026-11-25)

- A Wii U Nintendo Land disc appeared once, launched successfully, and was removed from the row after ejection. The user observed a 20–30 second return to the menu after exiting that disc title; the Kirby Wii U title returned quickly. Existing diagnostics do not establish whether the disc-title delay is normal.
- During the Wii-disc insertion, `debug.log` repeatedly recorded that the ODD query returned no entries. There was no Wii-disc ODD app-type diagnostic and `launch.log` contained no `wii-disc` launch record. This points to detection/listing as the current blocker; the launch path was not reached, and the log cannot establish whether MCP or the drive failed to expose the disc.

## Follow-up diagnostic test (2026-11-26)

- Wii Sports and Wii Sports Resort still produced no disc tile in direct Aroma. The ODD query returned no entries.
- The diagnostic's broader MCP `AUTO` query reported 312 entries, more than the initial buffer sized from `MCP_TitleCount`; it logged the count but could not inspect candidate app types or device labels. The diagnostic now uses a 8192-entry bounded buffer for that query so it can safely inspect the returned list. This does not change disc detection or launch behavior.
- The user also reports a menu freeze accompanied by a buzzing sound while navigating. The captured log has no crash trace; one session records two roughly 10-second menu heartbeats before ending, while the latest capture ends immediately after startup and the `AUTO` count diagnostic. The available log does not identify the freeze cause. Treat this as an unresolved runtime issue; stop using this diagnostic build if the freeze recurs and restore the backed-up RPX.
