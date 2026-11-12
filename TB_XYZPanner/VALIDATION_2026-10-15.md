# TB XYZ Panner: validity and UI correction

Scope: current `E:\Plugin Projects\TB_XYZPanner\Source`, Release x64. This is a
local maintenance build of 1.4.0, not a new public release. The existing public
1.4.0 archive, catalog, price and Git history were not changed.

This section records the initial local correction. The subsequent user-requested
1.4.1 publication and versioned installation are recorded in `RELEASE_QA_1.4.1.md`.

## Reproduced defects and corrections

| Defect | Correction and evidence |
| --- | --- |
| Side-view click mapping used the entire plot while drawing used an inset travel area. Clicking the current position changed Height/Distance. | Painting and interaction now share the same movement bounds. Painted-marker click tests pass at 300px and 420px. |
| Distance Curve overlapped knob values and reduced the panel's stored height from 150px to 112px. | Separate footer below the unchanged panel; no visible sibling bounds overlap. |
| Front/Back existed as a parameter but had no dedicated control. | Added attachment/knob using the existing parameter. Six identical knob sizes, POSITION 3 / SPACE 2 / LEVEL 1, two 420px square planes, 1080x720 editor. |
| Axis text could obscure the current point; listener head did not coincide with the coordinate origin. | Marker painted last, axis text separated from active colour, head centred on the origin. |
| XY moves emitted a completed host gesture on every mouse event and merged unrelated Undo operations. Rapid knob/curve changes also merged. | One gesture per axis per drag, balanced completion, pending APVTS values flushed at edit boundaries. Independent Undo/Redo for both axes, knobs, curve and rendering mode. Right-click does not move the object. |
| Fixed filter cutoffs exceeded Nyquist at 22.05/24/32 kHz, producing non-finite output even at Distance zero. | Cutoffs capped at 0.45 times sample rate. All 24 low-rate neutral/distant/rear/elevated tests pass. Existing 44.1kHz+ static cutoff values unchanged. |
| Returning to Headphones replayed FIR input retained from before a long Stereo interval. | Reset HRTF histories on effective mode transition. Expired impulse energy reduced from 0.01 to 0. |
| Height filter coefficients jumped directly to block target values and filter state was paused at neutral. | Coefficients follow the existing 20ms smoothing at a 16-sample control interval; filter state stays warm. Reuse prepared coefficient storage. DC-input transition peak step fell from 0.0134278 to 0.000267692 in stereo and 0.0189898 to 0.000378571 in 5.1, below the 0.0005 test gate (1% of the 0.05 input amplitude). |

The stereo M/S pan law, headphone azimuth/elevation mapping, KEMAR data and native
surround speaker layout are preserved. This is not a new spatial model. The two
planes display parameter coordinates, not measured acoustic source positions.

## Verification

- DSP: **164/164 PASS**, production SharedCode. Original 128 checks retained.
  Includes buses/state, left/right ear energy and timing, mirrored response,
  original measured front/back/elevation HRIR comparison, stereo and every
  5.1/7.1 input channel, 44.1/48/96/192kHz movement, low-rate safety and mode re-entry.
- Static height-filter reference comparisons: eight tests, squared error zero.
  64/256-sample block automation comparison: normalized error zero.
- UI: **48/48 PASS**, real JUCE components linked to production SharedCode.
  Includes painted marker round trips, overlap/size checks, values and painted
  pointer angles at minimum/midpoint/maximum, edit gestures, Undo/Redo and mode
  buttons. Default/extreme 100% and default 150% offscreen renders inspected.
- SharedCode, VST3 Manifest Helper, VST3 and Standalone Release builds passed.
- Actual built and installed VST3 host probes passed. Full normalized JSON equals
  the pre-change installed 1.4.0 contract, including both CIDs, 8 source parameters
  plus Bypass, metadata/text, 22 Set/Get cases and 9 state comparisons.
- Installed binary, manifest and KEMAR attribution hashes all equal the build.

Evidence: `Tests/out/20261015-before-validity-fixes.log`,
`Tests/out/last-run.log`, `Tests/out/20261015-before-knob-undo-fix.log`,
`Tests/out/after-tests.log`, `Tests/out/before-host-contract.json`,
`Tests/out/after-host-contract.json`, `Tests/out/installed-host-contract.json`.
Some early editor-test iterations corrected pixel-detection test code; their
aggregate failures must not be interpreted as product defect counts.

Screenshots: `Tests/out/before-default.png`, `Tests/out/after-default.png`,
`Tests/out/after-extreme.png`, `Tests/out/after-150pct.png`.

## Local installation

Target: `C:\Program Files\Common Files\VST3\TB_Plugins\TB_XYZPanner.vst3`

Previous bundle backed up and hash verified at:
`Builds/InstalledBackups/2026-10-15-validity-1.4.0/TB_XYZPanner.vst3`.
Only this installed product's three bundle files were updated. Cubase was left
running and was not controlled or closed; its inspected module list did not
contain XYZPanner before the update.

New binary SHA-256:
`9F0E98436D028F4F6C96B6831DB0A5C160C0151AB61823BE98FF4E241EDB7ECD`

Previous binary SHA-256:
`9594DF0D1B4D7C2B4555B6EC503473A310B2A502DA1392C1598E89DB65DB2806`

## Boundaries and separate finding

- No DAW listening, live desktop mouse/keyboard, OS DPI transition, long-duration,
  multi-instance, or whole-callback allocation test was performed. Offscreen
  component tests and deterministic audio checks do not prove perceptual
  front/back or elevation accuracy for an individual listener.
- Generic KEMAR elevation remains approximately -40 to +40 degrees. Ordinary
  STEREO retains its existing extreme-pan antiphase cancellation limitation.
- **Preset source divergence remains unresolved.** `UPDATES.md`'s 1.2 preset
  claims correspond to the separate `NewProject/Source` lineage. The current
  root build has no preset-save/load UI or factory program bank. The old project
  has a different plug-in code and lacks the current Distance Model and
  multichannel-to-binaural path, so it was not restored wholesale. A selective
  preset recovery needs its own compatibility checks, including old file
  migration and the old save-dialog destination mismatch.
- This turn changed only `Source/PluginEditor.cpp/.h`,
  `Source/SpatialRenderer.cpp/.h`, regression tests and validation documentation.
  Pre-existing SVN changes, the old project and previous backups were preserved.
