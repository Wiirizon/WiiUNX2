# UI sound effects

The launcher bundles three original, procedural navigation, click and back cues in `launcher/data/sounds/`. No Nintendo sound recordings are distributed in this repository. Locally supplied effects can override the bundled cues; if an override is missing or invalid, the bundled cue is used.

Use only audio you have permission to use. The importer accepts WAV files named `Select.wav`, `Klick.wav`, and `Enter & Back.wav` for navigation, confirm and back. These can be your own original recordings with those filenames. To create a distinct synthetic set without any reference recordings, run:

    python launcher/tools/Generate-Original-Sounds.py

The generated cues are short, dry synthetic clicks with a sharp noise transient
and a brief resonant tail. Navigation, confirm, and back use different
resonances and durations; these are original procedural sounds, not copies of
supplied or commercial recordings. This writes WAVs under
`launcher/build/original-ui-cues/`. Import them from the `launcher` directory:

    node tools/Import-Sounds.mjs "build/original-ui-cues"

The synthetic cues are generated from simple oscillator parameters with distinct envelopes; the generator does not read or use reference recordings.

For other original WAVs, from the `launcher` directory run:

    node tools/Import-Sounds.mjs "PATH_TO_YOUR_WAV_FOLDER"

Then build the launcher. Locally imported `uiMove.bin`, `uiClick.bin` and `uiBack.bin` remain excluded by `.gitignore`; only the bundled `ui*Default.bin` cues are tracked. The importer’s `SOUND-ASSETS.json` report is local and ignored. Do not commit or redistribute recordings without the necessary rights. The importer does not download sounds or grant a license to them.

To regenerate the bundled originals, generate and import the cues as above, then replace the matching `uiMoveDefault.bin`, `uiClickDefault.bin` and `uiBackDefault.bin` files with those imported outputs.

The generator is covered by `launcher/tests/test_original_sounds.py`:

    python launcher/tests/test_original_sounds.py

The included procedural background music is original and separate from these effects. The Noto Sans fallback font is included under SIL OFL 1.1; its license is in launcher/licenses/NotoSans-OFL.txt.
