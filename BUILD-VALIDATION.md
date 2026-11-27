# Source build verification — 2026-10-29

A fresh copy containing the modified dependencies, generated original icons, procedural background music and pinned OFL-licensed Noto Sans Regular font compiled with the installed devkitPro/wut toolchain. Nintendo UI effects and the previous fallback font were absent. No original upstream image files were included; only generated nx images were copied.

This confirms a clean source build succeeds, not byte-for-byte reproducibility across toolchains or build directories. No hardware validation of the fallback font or applet-return metadata optimization is claimed. The console font remains preferred. This 2026-10-29 validation record did not include or check in the generated RPX. The current GitHub Actions build uploads `men.rpx` and its checksum as a workflow artifact, and attaches them to releases created from version tags.

Build with launcher/Build.ps1 on Windows with devkitPro installed, or build libgui, librpxloader and libwuhbutils followed by launcher with the Wii U toolchain. Optional effects can be supplied according to SOUNDS.md. Release packages are built and checked as described in the Release packaging section of HOST-VALIDATION.md.
