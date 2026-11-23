<div align="center">
<img width="1280" height="640" alt="TB Audio Plug-ins — 1e DISPLAY icon family" src="assets/social/tb_audio_plugins_social_preview.png" />
</div>

# Update Note

<!-- Keep only the latest three update dates. Add the newest date at the top whenever this repository is updated. -->

## 2026-11-23

Five follow-up Windows x64 VST3 updates: TB De-Esser 1.1.0, TB Shimmer 2.1.0, TB Filter Table 2.0.1, TB Limiter 1.3.1, and TB Pitch Shifter 3.0.1. Existing prices and macOS downloads are unchanged. Vocoder 3.0.0 remains on hold because the current Host probe exceeded its 180-second watchdog; Resonator and Ring Modulation remain in progress. See [the release notes](reporting/releases/2026-11-23/README.md).

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
<img width="1200" alt="TB Center updated UI example" src="Portfolio/Mockups/tb_center.png" />
#### TB Compressor
<img width="1200" alt="TB Compressor updated UI example" src="Portfolio/Mockups/tb_compressor.png" />
#### TB Distortion
<img width="1200" alt="TB Distortion updated UI example" src="Portfolio/Mockups/tb_distortion.png" />
#### TB Disperser
<img width="1200" alt="TB Disperser updated UI example" src="Portfolio/Mockups/tb_disperser.png" />
#### TB EQ
<img width="1200" alt="TB EQ updated UI example" src="Portfolio/Mockups/tb_eq.png" />
#### TB Colorizer
<img width="1200" alt="TB Colorizer updated UI example" src="Portfolio/Mockups/tb_colorizer.png" />
#### TB Inverted Flanger
<img width="1200" alt="TB Inverted Flanger updated UI example" src="Portfolio/Mockups/tb_inverted_flanger.png" />
#### TB Inverted Phaser
<img width="1200" alt="TB Inverted Phaser updated UI example" src="Portfolio/Mockups/tb_inverted_phaser.png" />
#### TB Jewel Digger & Finder
<img width="1200" alt="TB Jewel Digger & Finder updated UI example" src="Portfolio/Mockups/tb_jewel_digger.png" />
#### TB Noise Remover
<img width="1200" alt="TB Noise Remover updated UI example" src="Portfolio/Mockups/tb_noise_remover.png" />
#### TB Parallel Reverb
<img width="1200" alt="TB Parallel Reverb updated UI example" src="Portfolio/Mockups/tb_parallel_reverb.png" />
#### TB Scrambler
<img width="1200" alt="TB Scrambler updated UI example" src="Portfolio/Mockups/tb_scrambler.png" />
#### TB Spectral Transient Shaper
<img width="1200" alt="TB Spectral Transient Shaper updated UI example" src="Portfolio/Mockups/tb_transient_shaper.png" />
#### TB Step Shifter (Beta)
<img width="1200" alt="TB Step Shifter (Beta) updated UI example" src="Portfolio/Mockups/tb_step_shifter.png" />
#### TB Tune
<img width="1200" alt="TB Tune updated UI example" src="Portfolio/Mockups/tb_tune.png" />
#### TB Volume
<img width="1200" alt="TB Volume updated UI example" src="Portfolio/Mockups/tb_volume.png" />
#### TB Vocoder (Beta)
<img width="1200" alt="TB Vocoder (Beta) updated UI example" src="Portfolio/Mockups/tb_vocoder.png" />
#### TB XYZ Panner
<img width="1200" alt="TB XYZ Panner updated UI example" src="Portfolio/Mockups/tb_xyz_panner.png" />
#### TB Limiter
<img width="1200" alt="TB Limiter updated UI example" src="Portfolio/Mockups/tb_limiter.png" />
#### TB SubLow
<img width="1200" alt="TB SubLow updated UI example" src="Portfolio/Mockups/tb_sublow.png" />
#### TB Delay
<img width="1200" alt="TB Delay updated UI example" src="Portfolio/Mockups/tb_delay.png" />
#### TB Pitch Shifter
<img width="1200" alt="TB Pitch Shifter updated UI example" src="Portfolio/Mockups/tb_pitch_shifter.png" />
#### TB Resonator
<img width="1200" alt="TB Resonator updated UI example" src="Portfolio/Mockups/tb_resonator.png" />
#### TB Ring Modulation
<img width="1200" alt="TB Ring Modulation updated UI example" src="Portfolio/Mockups/tb_ring_modulation.png" />
#### TB Recorder
<img width="1200" alt="TB Recorder updated UI example" src="Portfolio/Mockups/tb_recorder.png" />
#### TB Shimmer
<img width="1200" alt="TB Shimmer updated UI example" src="Portfolio/Mockups/tb_shimmer.png" />
#### TB Exciter
<img width="1200" alt="TB Exciter updated UI example" src="Portfolio/Mockups/tb_exciter.png" />
#### TB Tape
<img width="1200" alt="TB Tape updated UI example" src="Portfolio/Mockups/tb_tape.png" />
#### TB De-Esser
<img width="1200" alt="TB De-Esser updated UI example" src="Portfolio/Mockups/tb_deesser.png" />
#### TB Filter Table
<img width="1200" alt="TB Filter Table updated UI example" src="Portfolio/Mockups/tb_filter_table.png" />
#### TB Grain Phaser
<img width="1200" alt="TB Grain Phaser updated UI example" src="Portfolio/Mockups/tb_grain_phaser.png" />
#### TB Multiband Compressor
<img width="1200" alt="TB Multiband Compressor updated UI example" src="Portfolio/Mockups/tb_multiband_compressor.png" />
#### TB Geometry Reverb
<img width="1200" alt="TB Geometry Reverb updated UI example" src="Portfolio/Mockups/tb_geometry_reverb.png" />
#### TB AudioPlayer
<img width="1200" alt="TB AudioPlayer updated UI example" src="Portfolio/Mockups/tb_audio_player.png" />
#### TB Ambient
<img width="1200" alt="TB Ambient updated UI example" src="Portfolio/Mockups/tb_ambient.png" />
### Additional portfolio plug-ins

This project has a completed mockup but is not a catalog release. TB Board remains intentionally excluded.

#### TB Board
<img width="1200" alt="TB Board updated UI example" src="Portfolio/Mockups/tb_board.png" />
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
