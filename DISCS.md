# Disc detection

The menu polls the optical drive (`MCP_TitleListByDeviceType`, device `ODD`) from the metadata worker thread once loading has finished. The decision logic is in `launcher/src/menu/DiscState.h` (`nx::DiscMonitor`), a pure state machine covered by `launcher/tests/disc-state.cpp`. The thin reader in `MainWindow.cpp` (`readDisc`) talks to the console API, and the loop reads `meta.xml` from the SD/disc file system; neither is host-tested.

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
- No NAND or system-title changes are made. The code only reads MCP's ODD list, requests the system title launch, and records the existing opt-in launch diagnostic.
- Other states not handled: dirty or unreadable discs (expected to appear as Error or Empty), a disc inserted while the drive is busy, discs of another region, and discs needing an update. The 2 s base interval is not proven to match stock timing.

Diagnostics: each distinct Wii disc app type and unsupported app type gets one debug-log line until the drive state changes; that ODD-type line contains no title ID. The separate opt-in launch log may contain title IDs and paths as documented in MAINTAINING.md. The generic unsupported notice is shown once per unsupported period (deferred while another message or a launch is active; a deferred notice is dropped if the drive reads empty or a game before it is shown) and is independent of the log line. Insertion, launch, Wii-mode return, and polling behavior remain UNVERIFIED on console.

## Hardware checklist for the maintainer

- [ ] Insert and eject a Wii U game disc: tile appears, launches, and disappears about 4 s after eject.
- [ ] Swap two game discs: tile changes name without a stuck tile.
- [ ] Insert a Wii disc: check for a "Wii Game Disc" tile and record its ODD app type without sharing title IDs.
- [ ] Select the Wii disc tile: verify whether the system enters vWii and whether the expected return path works. Do not change NAND or system titles.
- [ ] Insert a scratched or unreadable disc: no flicker and no log spam.
