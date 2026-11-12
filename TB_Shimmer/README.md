# TB_Shimmer

TB_Shimmer is a JUCE 8 granular shimmer effect for Windows. It layers density-clocked,
windowed grains with pitch-aware resampling and filtered feedback, then passes the
filtered grain signal through a per-channel four-stage Schroeder all-pass early
diffuser and an original fixed-memory eight-line orthogonal late FDN. Individual
particles can stay distinct or bloom into a smooth, evolving spatial wash around
vocals, guitars, pads, and percussion.

The first release target is Standalone + VST3, Windows x64. The 17 public parameters
keep the original 11 IDs in place, including the host-visible Bypass parameter, then
append Level Var, Position Rand, Pitch Spray, Freeze, Sparkle, and Shatter in that
order. Freeze stops history recording so the grain pool keeps replaying whatever it
had captured.

`Sparkle` adds a separate layer of short percussive shards: fragments of the input
re-read one to three octaves up (harmonics 2, 3, 4, 6 and 8), high-passed, and bounced
through a ping-pong echo that follows `Time`, `Feedback` and `Tone`. `Shatter` moves
that layer from an even glitter drizzle to attack-triggered bursts that re-read each
transient and thin out like breaking glass. At Sparkle 0% the layer is skipped and the
output is sample-identical to the engine without it.

`Size` and `Density` also set a smoothed 24% to 58% early-diffusion blend before the
late FDN. The raw/diffused mix is energy-normalised; it feeds both the direct wet path
and the FDN input, but only the pre-diffuser filtered grain signal returns to granular
history. `Density` separately sets a 30% to 80% late-tail weight, while `Scatter`
controls grain-size variation, stereo pan, and deterministic smooth-random tail
motion. Per-grain level, history position, and pitch randomness are controlled by
Level Var, Position Rand, and Pitch Spray. `Feedback` sets an independent tail decay
target up to 45 seconds, and `Tone` damps each loop. `Density` reaches 2000 gr/s and
`Size` reaches 1 ms, so the same engine covers both a sparse grain cloud and audio-rate
granular synthesis on live input; the grain pool holds 64 voices and steals the grain
nearest the end of its envelope rather than dropping a spawn. `Pitch` is continuous at
1 cent resolution. State schema and preset format are
version 4. Valid version 1 data migrates as Level Var = 0, Position Rand = old Scatter,
Pitch Spray = old Scatter × 0.0035, and Freeze off; valid version 2 data migrates with
Freeze off; every older version migrates with Sparkle and Shatter at 0. Malformed and
future data is rejected.

The Factory bank contains exactly 100 programs. Legacy programs 0–11 keep their IDs,
names, groups, values, and order byte-for-value; 88 new programs are appended as eleven
contiguous eight-program groups: UTILITY, DIFFUSE, SPARKLE, RANDOM, PULSE, VOCAL,
GUITAR, KEYS, PADS, DARK, and EXPERIMENTAL. PULSE uses free-running time values and
does not claim host tempo sync. State schema/preset format remain 2/2, but exposing all
100 programs changes the VST3 Program contract to `valueCount=100` and `stepCount=99`;
therefore normalized Program automation uses a different grid than the former 12-program
bank even though parameter state and User preset compatibility are preserved.

The spatial listening target was informed only by Soundtoys' public
[SpaceBlender product page](https://www.soundtoys.com/product/spaceBlender/) and
[official user manual](https://www.soundtoys.com/wp-content/uploads/SpaceBlender-Manual.pdf).
TB_Shimmer does not reproduce SpaceBlender's algorithm, controls, visualizer, presets,
parameter ranges, artwork, or brand assets.

## Project layout

- `Source/GranularShimmerEngine.*` — realtime granular and diffuse-tail DSP engine.
- `Source/PluginProcessor.*` — APVTS, state, presets, and host contract.
- `Source/PluginEditor.*` — fixed 760×430 single-panel editor with all 16 controls in an 8+8 grid, a parameter-driven live Grain Field with engine-driven sparkle fragments, flat matte knobs, and canonical radial logo echoes.
- `Tests/Main.cpp` — deterministic DSP/state harness and QA artifact writer.
- `PRODUCT_BRIEF.md` and `PROCESSING_RESEARCH.md` — product and algorithm decisions.
- `RELEASE_QA.md` — build, automated, host, and manual release gates.

Open `TB_Shimmer.jucer` with `C:\JUCE\Projucer.exe` to generate the Visual Studio
2022 project. Version 1.0.0 was approved for public Windows x64 release on
2026-08-03 after the automated build and host gates passed. The remaining manual
DAW, listening, and performance checks in `RELEASE_QA.md` are not recorded as complete.

Version 2.0.0 is in development. Saved state and User presets from 1.0.0 still restore
exactly, but `Density` and `Size` were widened and `Pitch` changed from a stepped
integer to a continuous parameter, so host automation lanes recorded against 1.0.0
replay against a different normalized mapping. That break is intentional: the 2.0.0
build is the reference, and earlier contracts are updated to match it.
