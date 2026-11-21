# Windows suite update — 2026-11-21

Source and release-governance changes: SVN r92. The complete source/QA report is maintained in the private source working copy as `Guide MD/RELEASE_REPORT_2026-11-21.md`.

15 products; existing catalogue prices, plug-in CIDs, product parameters and macOS downloads retained.

| Product | Version | Automated tests |
| --- | --- | --- |
| [TB SubLow](TB_SubLow.md) | 1.4.0 | TB_SubLow-dsp, TB_SubLow-native-ui, TB_SubLow-ui |
| [TB Disperser](TB_Disperser.md) | 2.0.0 | TB_Disperser-dsp, TB_Disperser-native-ui, TB_Disperser-ui |
| [TB Distortion](TB_Distortion.md) | 2.0.0 | TB_Distortion-dsp, TB_Distortion-native-ui |
| [TB Exciter](TB_Exciter.md) | 1.3.0 | TB_Exciter-dsp, TB_Exciter-native-ui |
| [TB Filter Table](TB_FilterTable.md) | 2.0.0 | TB_FilterTable-dsp, TB_FilterTable-final1, TB_FilterTable-final2, TB_FilterTable-native-ui, TB_FilterTable-quiet1, TB_FilterTable-ui |
| [TB Colorizer](TB_Colorizer.md) | 3.0.0 | TB_Colorizer-dsp, TB_Colorizer-native-ui, TB_Colorizer-ui |
| [TB Inverted Flanger](TB_InvertedFlanger.md) | 3.0.0 | TB_InvertedFlanger-dsp, TB_InvertedFlanger-native-ui, TB_InvertedFlanger-ui |
| [TB Inverted Phaser](TB_InvertedPhaser.md) | 4.0.0 | TB_InvertedPhaser-dsp, TB_InvertedPhaser-native-ui |
| [TB Limiter](TB_Limiter.md) | 1.3.0 | TB_Limiter-native-ui, TB_Limiter-processor |
| [TB Noise Remover](TB_NoiseRemover.md) | 3.0.0 | TB_NoiseRemover-native-ui, TB_NoiseRemover-wav |
| [TB Parallel Reverb](TB_ParallelReverb.md) | 3.2.0 | TB_ParallelReverb-native-ui, TB_ParallelReverb-presets, TB_ParallelReverb-quality |
| [TB Pitch Shifter](TB_PitchShifter.md) | 3.0.0 | TB_PitchShifter-dsp, TB_PitchShifter-native-ui, TB_PitchShifter-processor, TB_PitchShifter-ui |
| [TB Scrambler](TB_Scrambler.md) | 3.0.0 | TB_Scrambler-dsp, TB_Scrambler-native-ui |
| [TB Spectral Transient Shaper](TB_SpectralTransientShaper.md) | 1.5.0 | TB_SpectralTransientShaper-dsp, TB_SpectralTransientShaper-native-ui |
| [TB XYZ Panner](TB_XYZPanner.md) | 1.5.0 | TB_XYZPanner-editor, TB_XYZPanner-native-ui, TB_XYZPanner-spatial |

Native source changes are committed to SVN. This directory documents Windows distribution assets, not a new public source-code release.

Manual DAW listening, interactive GUI checks and installer execution were not run. Installers are not code-signed. Noise Remover has a documented behavior/latency change; Parallel Reverb changes the decay/diffusion sound. See individual notes.
