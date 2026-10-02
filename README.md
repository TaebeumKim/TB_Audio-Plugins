<div align="center">
<img width="1280" height="640" alt="TB Audio Plug-ins — 1e DISPLAY icon family" src="assets/social/tb_audio_plugins_social_preview.png" />

**35 free VST3 and AU audio plug-ins for Windows and macOS (Apple Silicon)**<br />
EQ, dynamics, reverb, delay, pitch, vocal, restoration, spatial, and creative effects, with user manuals in 20 languages.

[![Downloads](https://img.shields.io/github/downloads/TaebeumKim/TB_Audio-Plugins/total?label=downloads&color=2ea44f)](https://github.com/TaebeumKim/TB_Audio-Plugins/releases)
[![GitHub stars](https://img.shields.io/github/stars/TaebeumKim/TB_Audio-Plugins?label=stars&color=yellow)](https://github.com/TaebeumKim/TB_Audio-Plugins/stargazers)
![Plug-ins](https://img.shields.io/badge/plug--ins-35-blue)
![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU-blueviolet)
![Platforms](https://img.shields.io/badge/platforms-Windows%20x64%20%7C%20macOS%20arm64-lightgrey)
![Manuals](https://img.shields.io/badge/manuals-20%20languages-orange)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**[⬇️ Download TB Hub for Windows](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/hub-windows-v1.3.18)** · **[All installers (Windows & macOS)](https://github.com/TaebeumKim/TB_Audio-Plugins/releases)** · **[Manuals](Documents/README.md)** · **[YouTube](https://www.youtube.com/@TeamImpulseImpact)** · **[☕ Ko-fi](https://ko-fi.com/teamimpulseimpact)**

⭐ If these plug-ins help your music, please star this repository so other producers can find them.

</div>

# Update Note

<!-- Keep only the latest three update dates. Add the newest date at the top whenever this repository is updated. -->

## 2026-10-02

- Published TB Hub 1.3.18 for Windows: Sort by Update now lists plug-ins with an available update first, long descriptions scroll inside the Info face, and Info descriptions use concise English copy.
- Removed a stray backup binary from the TB Compressor 2.1.0 Hub archive, halving its download from 5.6 MB to 2.8 MB. The installed plug-in is unchanged.
- Removed a duplicate legacy `TB-TransientShaper` macOS bundle that shared its plug-in IDs with `TB-SpectralTransientShaper`, and a stray `NewProject.vst3` binary from the legacy Windows TB XYZ Panner bundle.
- Corrected the platform list (Windows x64 and macOS on Apple Silicon; no Linux builds) and added a plug-in overview table and current download links.

## 2026-09-30

- Released TB EQ 2.0.2: spectral SENSITIVITY and maximum attenuation DEPTH now work independently, and the NIDDLE label is corrected to FOCUS while host parameter IDs and VST3 class IDs are preserved.
- Released TB Noise Remover 2.4.0 with Noise Print capture from noise-only passages, a Focus Band for targeted reduction, and a safe spectral fallback.
- Released TB Parallel Reverb 3.1.0 with the updated Glue, Bright, and Space pedal design and a resizable 600×520 default window.
- Released TB XYZ Panner 1.3.0 with Physical, Near Field, and Linear distance attenuation curves.
- Refined the TB Exciter 1.1.0 DSP for a cleaner, more open upper-air character.

## 2026-09-22

- Rewrote the Korean manuals from the verified plug-in controls, with beginner-friendly theory and knob-by-knob explanations.

# Buy me a coffee?
https://ko-fi.com/teamimpulseimpact

# Contact me
e-mail : ktb7056@gmail.com

homepage : https://ktb-portfolio.netlify.app/






# TB_Audio-Plugins 🎛️

Welcome to **TB_Audio-Plugins**, a collection of professional-grade, free audio plug-ins designed for music producers, mixing engineers, and sound designers. These plug-ins feature high-quality digital signal processing (DSP) and intuitive user interfaces to elevate your audio production workflow.

*Please note: This repository is used exclusively for distributing the pre-compiled plug-in binaries. Source code is not publicly available.*

## ✨ Key Features
* **Professional Audio Quality:** High-resolution DSP tailored for professional studio environments.
* **Optimized Performance:** Lightweight and highly optimized for low CPU consumption.
* **Cross-Platform Support:** Windows 10/11 (x64) and macOS on Apple Silicon (arm64). Linux and Intel Macs are not supported, and TB Geometry Reverb, TB AudioPlayer, and TB Ambient are currently Windows-only.
* **Industry-Standard Formats:** VST3 on Windows and macOS, plus Audio Unit (AU) on macOS.
* **100% Free:** Completely free for both personal and commercial audio production.

## 🎙️ English Description Voiceovers

Professional English scripts and rendered 25–50 second MP3 voiceovers are available in [`Voiceovers/ENG`](Voiceovers/ENG/README.md). The package currently contains 28 narrations with a consistent professional, energetic delivery. Voiceover documentation does not publish or change a plug-in binary.

## 📦 Included Plug-ins

The catalog currently contains 35 plug-ins. One additional project has a portfolio mockup: the intentionally unreleased TB Board.

Detailed manuals for the established catalog set are available in English, Korean, Simplified Chinese, Japanese, Spanish, Russian, German, French, Brazilian Portuguese, Italian, Dutch, Swedish, Polish, Turkish, Arabic, Hindi, Indonesian, Thai, Vietnamese, and Czech. Browse the [multilingual manual index](Documents/README.md) to download PDFs by language.

### At a glance

| Plug-in | Category | What it does | Windows | macOS | Manual |
| --- | --- | --- | :---: | :---: | :---: |
| [TB Center](#tb-center) | Utility | Stereo centering. | ✓ | ✓ | [PDF](Documents/ENG/TB_Center_Detailed_User_Manual_EN.pdf) |
| [TB Compressor](#tb-compressor) | Dynamics | Peak and RMS compression. | ✓ | ✓ | [PDF](Documents/ENG/TB_Compressor_Detailed_User_Manual_EN.pdf) |
| [TB Distortion](#tb-distortion) | Distortion | Multi-stage distortion. | ✓ | ✓ | [PDF](Documents/ENG/TB_Distortion_Detailed_User_Manual_EN.pdf) |
| [TB Disperser](#tb-disperser) | Creative | Phase dispersion. | ✓ | ✓ | [PDF](Documents/ENG/TB_Disperser_Detailed_User_Manual_EN.pdf) |
| [TB EQ](#tb-eq) | EQ | Parametric equalizer. | ✓ | ✓ | [PDF](Documents/ENG/TB_EQ_Detailed_User_Manual_EN.pdf) |
| [TB Colorizer](#tb-colorizer) | Voice | Voice color shaping. | ✓ | ✓ | [PDF](Documents/ENG/TB_Colorizer_Detailed_User_Manual_EN.pdf) |
| [TB Inverted Flanger](#tb-inverted-flanger) | Modulation | Inverted flanging. | ✓ | ✓ | [PDF](Documents/ENG/TB_InvertedFlanger_Detailed_User_Manual_EN.pdf) |
| [TB Inverted Phaser](#tb-inverted-phaser) | Modulation | Subtractive phasing. | ✓ | ✓ | [PDF](Documents/ENG/TB_InvertedPhaser_Detailed_User_Manual_EN.pdf) |
| [TB Jewel Digger & Finder](#tb-jewel-digger--finder) | Creative | Harmonic enhancement. | ✓ | ✓ | [PDF](Documents/ENG/TB_JewelDiggerAndFinder_Detailed_User_Manual_EN.pdf) |
| [TB Noise Remover](#tb-noise-remover) | Restoration | Learnable neural and spectral noise removal. | ✓ | ✓ | [PDF](Documents/ENG/TB_NoiseRemover_Detailed_User_Manual_EN.pdf) |
| [TB Parallel Reverb](#tb-parallel-reverb) | Reverb | Three-engine parallel reverb. | ✓ | ✓ | [PDF](Documents/ENG/TB_ParallelReverb_Detailed_User_Manual_EN.pdf) |
| [TB Scrambler](#tb-scrambler) | Creative | Scramble and glitch effects. | ✓ | ✓ | [PDF](Documents/ENG/TB_Scrambler_Detailed_User_Manual_EN.pdf) |
| [TB Spectral Transient Shaper](#tb-spectral-transient-shaper) | Dynamics | Spectral transient shaping. | ✓ | ✓ | [PDF](Documents/ENG/TB_SpectralTransientShaper_Detailed_User_Manual_EN.pdf) |
| [TB Step Shifter (Beta)](#tb-step-shifter-beta) | Creative | Harmonic pitch shifting. | ✓ | ✓ | [PDF](Documents/ENG/TB_StepShifter_Detailed_User_Manual_EN.pdf) |
| [TB Tune](#tb-tune) | Voice | Vocal pitch correction. | ✓ | ✓ | [PDF](Documents/ENG/TB_Tune_Detailed_User_Manual_EN.pdf) |
| [TB Volume](#tb-volume) | Utility | Simple gain control. | ✓ | ✓ | [PDF](Documents/ENG/TB_Volume_Detailed_User_Manual_EN.pdf) |
| [TB Vocoder (Beta)](#tb-vocoder-beta) | Creative | Vocoder. | ✓ | ✓ | [PDF](Documents/ENG/TB_Vocoder_Detailed_User_Manual_EN.pdf) |
| [TB XYZ Panner](#tb-xyz-panner) | Utility | Multi-axis panning. | ✓ | ✓ | [PDF](Documents/ENG/TB_XYZPanner_Detailed_User_Manual_EN.pdf) |
| [TB Limiter](#tb-limiter) | Dynamics | True peak mastering limiter. | ✓ | ✓ | [PDF](Documents/ENG/TB_Limiter_Detailed_User_Manual_EN.pdf) |
| [TB SubLow](#tb-sublow) | Creative | Sub-bass generator. | ✓ | ✓ | [PDF](Documents/ENG/TB_SubLow_Detailed_User_Manual_EN.pdf) |
| [TB Delay](#tb-delay) | Delay | Creative stereo delay. | ✓ | ✓ | [PDF](Documents/ENG/TB_Delay_Detailed_User_Manual_EN.pdf) |
| [TB Pitch Shifter](#tb-pitch-shifter) | Pitch | Independent pitch and formant shifting. | ✓ | ✓ | [PDF](Documents/ENG/TB_PitchShifter_Detailed_User_Manual_EN.pdf) |
| [TB Resonator](#tb-resonator) | Creative | Tunable modal resonator for pitched, playable textures. | ✓ | ✓ | [PDF](Documents/ENG/TB_Resonator_Detailed_User_Manual_EN.pdf) |
| [TB Ring Modulation](#tb-ring-modulation) | Modulation | Ring modulation from tremolo to metallic sidebands. | ✓ | ✓ | [PDF](Documents/ENG/TB_RingModulation_Detailed_User_Manual_EN.pdf) |
| [TB Recorder](#tb-recorder) | Utility | Insert-point recording with format conversion and splitting. | ✓ | ✓ | [PDF](Documents/ENG/TB_Recorder_Detailed_User_Manual_EN.pdf) |
| [TB Shimmer](#tb-shimmer) | Creative | Granular shimmer with pitch feedback and diffuse tails. | ✓ | ✓ | [PDF](Documents/ENG/TB_Shimmer_Detailed_User_Manual_EN.pdf) |
| [TB Exciter](#tb-exciter) | Creative | Harmonic exciter and enhancer with 30 Factory presets. | ✓ | ✓ | [PDF](Documents/ENG/TB_Exciter_Detailed_User_Manual_EN.pdf) |
| [TB Tape](#tb-tape) | Creative | Custom-curve tape stop and start effects. | ✓ | ✓ | [PDF](Documents/ENG/TB_Tape_Detailed_User_Manual_EN.pdf) |
| [TB De-Esser](#tb-de-esser) | Dynamics | Level-independent de-essing that preserves vocal air and diction. | ✓ | ✓ | [PDF](Documents/ENG/TB_DeEsser_Detailed_User_Manual_EN.pdf) |
| [TB Filter Table](#tb-filter-table) | Filter | Wavetable spectral filtering with morphing frames and phase modes. | ✓ | ✓ | [PDF](Documents/ENG/TB_FilterTable_Detailed_User_Manual_EN.pdf) |
| [TB Grain Phaser](#tb-grain-phaser) | Modulation | Spectral phasing with dense notches and stereo motion. | ✓ | ✓ | [PDF](Documents/ENG/TB_GrainPhaser_Detailed_User_Manual_EN.pdf) |
| [TB Multiband Compressor](#tb-multiband-compressor) | Dynamics | Four-band dynamics with free placement, sidechain, and M/S control. | ✓ | ✓ | [PDF](Documents/ENG/TB_MultiBandCompressor_Detailed_User_Manual_EN.pdf) |
| [TB Geometry Reverb](#tb-geometry-reverb) | Reverb | Mesh-based reverb with controllable scale, materials, and placement. | ✓ | — | [PDF](Documents/ENG/TB_GeometryReverb_Detailed_User_Manual_EN.pdf) |
| [TB AudioPlayer](#tb-audioplayer) | Instrument | One-shot sample playback with automatable triggering. | ✓ | — | [PDF](Documents/ENG/TB_AudioPlayer_Detailed_User_Manual_EN.pdf) |
| [TB Ambient](#tb-ambient) | Utility | Outdoor distance and ground tone without a reverb tail. | ✓ | — | [PDF](Documents/ENG/TB_Ambient_Detailed_User_Manual_EN.pdf) |

Windows builds of every plug-in are available through TB Hub; per-plug-in installers are listed on the [Releases](https://github.com/TaebeumKim/TB_Audio-Plugins/releases) page.

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

An independent pitch and formant processor for transposing audio and changing perceived size with two direct controls.

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

A four-band free-placement dynamics processor with per-band compression or expansion, sidechain, stereo/Mid/Side targeting, signed Range, and Dynamic or Linear modes.

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
* **Windows (recommended):** install [TB Hub](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/hub-windows-v1.3.18), which installs and updates the plug-ins in the catalog.
* **Per-plug-in installers:** Windows x64 VST3 setup files and macOS Apple Silicon VST3/AU `.pkg` installers are published on the [Releases](https://github.com/TaebeumKim/TB_Audio-Plugins/releases) page.
* The `Window/` and `MacOS/` folders in this repository hold older builds. Use TB Hub or the Releases page for current versions.

### 2. Opening the installers
The installers are not code-signed yet, so the operating system may warn you the first time:
* **Windows:** if SmartScreen shows "Windows protected your PC", select **More info**, then **Run anyway**.
* **macOS:** if the `.pkg` is blocked, open **System Settings > Privacy & Security** and select **Open Anyway**.

### 3. Manual Installation
Extract the downloaded zip/archive file and move the plug-in files to your system's designated audio plug-in folders:

#### **Windows:**
* **VST3:** `C:\Program Files\Common Files\VST3`

#### **macOS:**
* **VST3:** `/Library/Audio/Plug-Ins/VST3`
* **AU (Audio Unit):** `/Library/Audio/Plug-Ins/Components`

## 📄 License
The pre-compiled binaries distributed in this repository are licensed under the terms of the **MIT License**. You are free to use these plug-ins in any commercial or non-commercial musical works. For more detailed information, please refer to the [LICENSE](LICENSE) file.

## P.S.

I am an independent developer who doesn't currently earn any income from my work. In Korea, you can buy a coffee for $1 (really!). Would you be willing to buy me a cup of coffee?

https://ko-fi.com/teamimpulseimpact

---

Fab is offering free audio sources. Please search for "TII" on Fab!

https://www.fab.com/search?q=tii

---

*Built with passion for the audio community. Enjoy creating!* 🎧
