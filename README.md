<div align="center">
<img width="1280" height="640" alt="TB Audio Plug-ins — 1e DISPLAY icon family" src="assets/social/tb_audio_plugins_social_preview.png" />
</div>

# Update Note

<!-- Keep only the latest three update dates. Add the newest date at the top whenever this repository is updated. -->

## 2026-11-23

Five follow-up Windows x64 VST3 updates: TB De-Esser 1.1.0, TB Shimmer 2.1.0, TB Filter Table 2.0.1, TB Limiter 1.3.1, and TB Pitch Shifter 3.0.1. Existing prices and macOS downloads are unchanged. Vocoder 3.0.0 remains on hold because the current Host probe exceeded its 180-second watchdog; Resonator and Ring Modulation remain in progress. See [the release notes](reporting/releases/2026-11-23/README.md).

### Latest macOS updates

| Release | Products and highlights |
| --- | --- |
| [SVN r87 — 2026-10-07](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/plugins-macos-arm64-svn-r87-2026-10-07) | 22 changed products with Apple Silicon AU/VST3 builds; macOS 11.0+; AAX omitted; packages are unsigned and not notarized. |
| [SVN r80 — 2026-08-22](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/plugins-macos-arm64-svn-r80-2026-08-22) | TB Delay, Grain Phaser, Parallel Reverb, and Recorder AU/VST3 updates; macOS 11.0+; automated product checks passed. |
| [SVN r66 — 2026-08-14](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/plugins-macos-arm64-svn-r66-2026-08-14) | TB Tune, Delay, EQ, Exciter, FilterTable, MultiBand Compressor, PitchShifter, Vocoder, and Volume AU/VST3 updates. |

## 2026-11-21

Windows x64 VST3 suite update. Each release below includes a Hub ZIP and Windows installer; catalogue prices and existing macOS downloads are unchanged.

The shared raster-part cache now stores software images, fixing a Windows plug-in unload/shutdown hang caused by retaining static Direct2D images. Source and packaged module hashes are checked so a file locked by another process cannot silently ship an older build.

| Plug-in | Version | Main change |
| --- | --- | --- |
| [TB Disperser](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-disperser-windows-v2.0.0) | 2.0.0 | Redesigned editor with actual group-delay response |
| [TB Distortion](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-distortion-windows-v2.0.0) | 2.0.0 | Transfer/Drive-led editor and type plates |
| [TB Exciter](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-exciter-windows-v1.3.0) | 1.3.0 | Signal-reactive Sparkle display and shared materials/fonts |
| [TB Filter Table](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-filter-table-windows-v2.0.0) | 2.0.0 | One-screen editor plus LFO tempo sync, envelope motion and stereo spread |
| [TB Colorizer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-colorizer-windows-v3.0.0) | 3.0.0 | Response-led editor without synthetic harmonic decoration |
| [TB Inverted Flanger](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-inverted-flanger-windows-v3.0.0) | 3.0.0 | New INVERT control, actual-rate comb display and host program-number persistence |
| [TB Inverted Phaser](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-inverted-phaser-windows-v4.0.0) | 4.0.0 | New INVERT control, full-width response display and grouped controls |
| [TB Limiter](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-limiter-windows-v1.3.0) | 1.3.0 | Four-second gain-reduction history and refreshed materials/fonts |
| [TB Noise Remover](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-noise-remover-windows-v3.0.0) | 3.0.0 | Serial speech isolation, adaptive gate and smaller FFT model export |
| [TB Parallel Reverb](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-parallel-reverb-windows-v3.2.0) | 3.2.0 | 16-line FDN, input diffusion/decay compensation and 150 presets |
| [TB Pitch Shifter](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-pitch-shifter-windows-v3.0.0) | 3.0.0 | Redesigned editor, corrected startup pitch and 75 ms tail reporting |
| [TB Scrambler](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-scrambler-windows-v3.0.0) | 3.0.0 | Actual 31-band mapping display and draggable band edges |
| [TB Spectral Transient Shaper](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-transient-shaper-windows-v1.5.0) | 1.5.0 | Shared materials/fonts and corrected knob-travel rendering |
| [TB SubLow](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-sublow-windows-v1.4.0) | 1.4.0 | Real Body/Bloom/Punch/sub-energy rings |
| [TB XYZ Panner](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-xyz-panner-windows-v1.5.0) | 1.5.0 | Correct near-field boundaries/arc anchors and shared materials/fonts |

Compatibility: existing plug-in IDs and product parameter contracts are retained. Noise Remover's Basic mode is stronger at the same REDUCTION value and reports 5120 samples at 48 kHz (previously 2048); review old sessions and use at least 128 samples at 44.1/48 kHz or 256 at 88.2/96 kHz. Parallel Reverb preserves the original 100 preset positions, but its upgraded decay/diffusion model changes the sound. Pitch Shifter preserves its existing pitch/formant coupling, not automatic formant compensation. Manual DAW listening, interactive GUI checks, installer execution and code-signing entitlement checks are not part of automated release verification.

## 2026-11-13

- Released [TB EQ 2.0.3 for Windows x64 VST3](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-eq-windows-v2.0.3) with a unified applied-response curve, cleaner spectral range guides, and tighter analyzer/control layout. Spectral regression 723/723, latency handoff, all-pass, and Analyzer UX checks passed.
- Released [TB Multiband Compressor 2.0.0 for Windows x64 VST3](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-multiband-compressor-windows-v2.0.0) with six bands, schema 5, 126 parameters, Sidechain 1–6, band Lookahead, Linear Slope, Solo, A/B, and EXPERT controls. DSP 76/76 and Processor Integration 11/11 passed.
- Released [TB Delay 1.1.0 for Windows x64 VST3](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-delay-windows-v1.1.0) with cleaner low-frequency Shift, bounded Tape varispeed, safer time crossfades and feedback, high-frequency repeat preservation, wet compensation, and level-independent ducking. Delay validation passed 37/37.

## 2026-10-15

- Released [TB XYZ Panner 1.4.1 for Windows x64 VST3](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-xyz-panner-windows-v1.4.1) with corrected Side View coordinates, a restored Front/Back knob, clearer control layout, and reliable Undo/Redo grouping.
- Improved low-sample-rate filter stability, Height automation transitions, and headphone-mode re-entry. All 164 DSP and 48 editor checks passed; existing parameter/state contracts and macOS downloads are preserved. Live DAW listening remains unverified.

# Buy me a coffee?
https://ko-fi.com/teamimpulseimpact

# contect me
e-mail : ktb7056@gmail.com

homepage : https://ktb-portfolio.netlify.app/






# TB_Audio-Plugins 🎛️

Welcome to **TB_Audio-Plugins**, a collection of professional-grade, free audio plug-ins designed for music producers, mixing engineers, and sound designers. These plug-ins feature high-quality digital signal processing (DSP) and intuitive user interfaces to elevate your audio production workflow.

*Please note: This repository is used exclusively for distributing the pre-compiled plug-in binaries. Source code is not publicly available.*

## ✨ Key Features
* **Professional Audio Quality:** High-resolution DSP tailored for professional studio environments.
* **Optimized Performance:** Lightweight and highly optimized for low CPU consumption.
* **Cross-Platform Support:** Fully compatible with Windows, macOS, and Linux.
* **Industry-Standard Formats:** Available in VST3 and Audio Unit (AU) formats.
* **100% Free:** Completely free for both personal and commercial audio production.

## 🎙️ English Description Voiceovers

Professional English scripts and rendered 25–50 second MP3 voiceovers are available in [`Voiceovers/ENG`](Voiceovers/ENG/README.md). The package currently contains 28 narrations with a consistent professional, energetic delivery. Voiceover documentation does not publish or change a plug-in binary.

## 📦 Included Plug-ins

The catalog currently contains 35 plug-ins. One additional project has a portfolio mockup: the intentionally unreleased TB Board.

Detailed manuals for the established catalog set are available in English, Korean, Simplified Chinese, Japanese, Spanish, Russian, German, French, Brazilian Portuguese, Italian, Dutch, Swedish, Polish, Turkish, Arabic, Hindi, Indonesian, Thai, Vietnamese, and Czech. Browse the [multilingual manual index](Documents/README.md) to download PDFs by language.

### Catalog plug-ins

#### TB Center

A multiband stereo-centering utility that brings energy toward the middle while preserving useful width and phase coherence.

<img width="900" alt="TB Center feature mockup" src="Portfolio/Mockups/tb_center.jpg" />

#### TB Compressor

A Peak/RMS dynamics processor with attack and release shaping, clear gain-reduction metering, and precise level control.

<img width="900" alt="TB Compressor feature mockup" src="Portfolio/Mockups/tb_compressor.jpg" />

#### TB Distortion

A multi-stage distortion effect for building richer harmonics with tone shaping and a controllable wet/dry blend.

<img width="900" alt="TB Distortion feature mockup" src="Portfolio/Mockups/tb_distortion.jpg" />

#### TB Disperser

An all-pass phase-dispersion effect that reshapes transients, focuses processing by frequency, and adds animated modulation.

<img width="900" alt="TB Disperser feature mockup" src="Portfolio/Mockups/tb_disperser.jpg" />

#### TB EQ

A parametric, dynamic, and spectral equalizer for precise cuts, automatic resonance control, and visual frequency shaping.

<img width="900" alt="TB EQ feature mockup" src="Portfolio/Mockups/tb_eq.jpg" />

#### TB Colorizer

A tonal color processor with seven color profiles, a single Transform amount, and preset recall.

<img width="900" alt="TB Colorizer feature mockup" src="Portfolio/Mockups/tb_colorizer.jpg" />

#### TB Inverted Flanger

A phase-inverted flanger for hollow subtractive sweeps, with rate, depth, feedback, and mix controls for metallic motion.

<img width="900" alt="TB Inverted Flanger feature mockup" src="Portfolio/Mockups/tb_inverted_flanger.jpg" />

#### TB Inverted Phaser

A subtractive phaser that carves animated notches into the signal with adjustable stages and tempo-aware movement.

<img width="900" alt="TB Inverted Phaser feature mockup" src="Portfolio/Mockups/tb_inverted_phaser.jpg" />

#### TB Jewel Digger & Finder

A harmonic enhancement and crystal-grain effect for revealing sparkle, adding resonant shimmer, and blending detailed textures.

<img width="900" alt="TB Jewel Digger and Finder feature mockup" src="Portfolio/Mockups/tb_jewel_digger.jpg" />

#### TB Noise Remover

A restoration tool that suppresses steady noise and ambience while preserving voice detail and showing real-time reduction.

<img width="900" alt="TB Noise Remover feature mockup" src="Portfolio/Mockups/tb_noise_remover.jpg" />

#### TB Parallel Reverb

A three-engine parallel reverb that layers independent spaces and builds depth without washing out the dry signal.

<img width="900" alt="TB Parallel Reverb feature mockup" src="Portfolio/Mockups/tb_parallel_reverb.jpg" />

#### TB Scrambler

A formant-scramble effect with adjustable Low and High limits, Scramble Amount, and preset recall.

<img width="900" alt="TB Scrambler feature mockup" src="Portfolio/Mockups/tb_scrambler.jpg" />

#### TB Spectral Transient Shaper

A frequency-dependent transient processor for shaping attack, controlling sustain, and focusing impact on selected bands.

<img width="900" alt="TB Spectral Transient Shaper feature mockup" src="Portfolio/Mockups/tb_transient_shaper.jpg" />

#### TB Step Shifter (Beta)

A beta harmonic pitch shifter with direct Pitch, Step, and Mix controls plus a Colorize mode.

<img width="900" alt="TB Step Shifter feature mockup" src="Portfolio/Mockups/tb_step_shifter.jpg" />

#### TB Tune

A redesigned vocal pitch-correction tool with direct key and scale selection, pitch-status feedback, Auto Musical response control, and formant shifting.

<img width="900" alt="TB Tune feature mockup" src="Portfolio/Mockups/tb_tune.jpg" />

#### TB Volume

A simple gain tool with decibel and percentage modes plus an exact level readout.

<img width="900" alt="TB Volume feature mockup" src="Portfolio/Mockups/tb_volume.jpg" />

#### TB Vocoder (Beta)

A 32-band vocoder with a resynthesis spectrum, configurable filterbank, and Imprint controls for tonal shaping.

<img width="900" alt="TB Vocoder feature mockup" src="Portfolio/Mockups/tb_vocoder.jpg" />

#### TB XYZ Panner

A three-axis spatial panner with pan/depth and height/distance maps plus adjustable room ambience.

<img width="900" alt="TB XYZ Panner feature mockup" src="Portfolio/Mockups/tb_xyz_panner.jpg" />

#### TB Limiter

A true-peak mastering limiter for increasing loudness, catching inter-sample overloads, and monitoring gain reduction clearly.

<img width="900" alt="TB Limiter feature mockup" src="Portfolio/Mockups/tb_limiter.jpg" />

#### TB SubLow

A tunable sub-bass generator for adding controlled low-end weight and blending it beneath the original bass content.

<img width="900" alt="TB SubLow feature mockup" src="Portfolio/Mockups/tb_sublow.jpg" />

#### TB Delay

A creative stereo delay with a visual delay map, rhythm controls, and grain-transform tools for repeat processing.

<img width="900" alt="TB Delay feature mockup" src="Portfolio/Mockups/tb_delay.jpg" />

#### TB Pitch Shifter

A two-control pitch and formant shifter. Pitch also moves the formants; Formant adds a separate envelope shift rather than automatically preserving the original vowel.

<img width="900" alt="TB Pitch Shifter feature mockup" src="Portfolio/Mockups/tb_pitch_shifter.jpg" />

#### TB Resonator

A tunable modal resonator that turns incoming audio into pitched, playable, and musically aligned textures.

<img width="900" alt="TB Resonator feature mockup" src="Portfolio/Mockups/tb_resonator.jpg" />

#### TB Ring Modulation

An internal-carrier ring modulator that moves from tremolo pulses to metallic sidebands with controllable tone and mix.

<img width="900" alt="TB Ring Modulation feature mockup" src="Portfolio/Mockups/tb_ring_modulation.jpg" />

#### TB Recorder

An insert-point recorder with automatic output-format conversion and file splitting for long recording sessions.

<img width="900" alt="TB Recorder feature mockup" src="Portfolio/Mockups/tb_recorder.jpg" />

#### TB Shimmer

A granular shimmer effect that creates floating grain clouds, octave-up reflections, and spacious feedback tails with tone and width control.

<img width="900" alt="TB Shimmer feature mockup" src="Portfolio/Mockups/tb_shimmer.jpg" />

#### TB Exciter

A harmonic exciter and enhancer with 30 Factory presets for controlled brightness, clarity, focus, stereo width, and level.

<img width="900" alt="TB Exciter feature mockup" src="Portfolio/Mockups/tb_exciter.jpg" />

#### TB Tape

A custom-curve tape-motor effect that slows audio to silence and accelerates it back to speed with independent brake and spin-up timing.

<img width="900" alt="TB Tape feature mockup" src="Portfolio/Mockups/tb_tape.jpg" />

#### TB De-Esser

A level-independent, language-neutral de-esser that uses stereo-linked spectral reduction to tame harsh sibilance while preserving vocal air and diction.

<img width="900" alt="TB De-Esser feature mockup" src="Portfolio/Mockups/tb_deesser.jpg" />

#### TB Filter Table

A wavetable-derived spectral filter that morphs up to 256 frames into moving response contours with four phase modes, nonlinear drive, and a frame LFO.

<img width="900" alt="TB Filter Table feature mockup" src="Portfolio/Mockups/tb_filter_table.jpg" />

#### TB Grain Phaser

A spectral phaser that moves a dense FFT notch grid with controlled stereo orbit, visualized by a black-green phosphor analyzer whose multilingual grain-glyph density follows the GRAIN control and live spectral energy.

<img width="900" alt="TB Grain Phaser feature mockup" src="Portfolio/Mockups/tb_grain_phaser.jpg" />

#### TB Multiband Compressor

A six-band free-placement dynamics processor with per-band compression or expansion, Sidechain 1–6, stereo/Mid/Side targeting, signed Range, band Lookahead, Linear Slope, and Dynamic or Linear modes.

<img width="900" alt="TB Multiband Compressor feature mockup" src="Portfolio/Mockups/tb_multiband_compressor.jpg" />

#### TB Geometry Reverb

A geometry-aware reverb that converts closed meshes into playable early reflections and tail shaping through controllable scale, materials, emitter/listener placement, and radiation pattern.

<img width="900" alt="TB Geometry Reverb feature mockup" src="Portfolio/Mockups/tb_geometry_reverb.jpg" />

#### TB AudioPlayer

A compact one-shot sample player with file browsing, automatable triggering, sample-rate conversion, and click-free retrigger and stop fades.

<img width="900" alt="TB AudioPlayer feature mockup" src="Portfolio/Mockups/tb_audio_player.jpg" />

#### TB Ambient

A delayless outdoor-placement processor that shapes distance, air absorption, ground colour, height, and far-field stereo focus across eight environmental profiles.

<img width="900" alt="TB Ambient feature mockup" src="Portfolio/Mockups/tb_ambient.jpg" />

### Additional portfolio plug-ins

This project has a completed mockup but is not a catalog release. TB Board remains intentionally excluded.

#### TB Board

A visual signal-routing workspace and VST3 host for building insert chains and animating parameters with live modulation.

<img width="900" alt="TB Board feature mockup" src="Portfolio/Mockups/tb_board.jpg" />

## 🚀 Installation Guide

### 1. Download
Go to the page of this repository and download the latest version compatible with your Operating System.

### 2. Manual Installation
Extract the downloaded zip/archive file and move the plug-in files to your system's designated audio plug-in folders:

#### **Windows:**
* **VST3:** `C:\Program Files\Common Files\VST3`

#### **macOS:**
* **VST3:** `/Library/Audio/Plug-Ins/VST3`
* **AU (Audio Unit):** `/Library/Audio/Plug-Ins/Components`

## 📄 License
The pre-compiled binaries distributed in this repository are licensed under the terms of the **MIT License**. You are free to use these plug-ins in any commercial or non-commercial musical works. For more detailed information, please refer to the [LICENSE](LICENSE) file.

#P.S
---
I am an independent developer who doesn't currently earn any income from my work. In Korea, you can buy a coffee for $1 (really!). Would you be willing to buy me a cup of coffee?

https://ko-fi.com/teamimpulseimpact
---
Fab is offering free audio sources. Please search for "TII" on Fab!

https://www.fab.com/search?q=tii
---
*Built with passion for the audio community. Enjoy creating!* 🎧
