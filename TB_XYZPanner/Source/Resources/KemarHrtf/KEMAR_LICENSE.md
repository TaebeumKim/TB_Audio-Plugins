# MIT KEMAR compact HRTF measurements

`kemar_hrir.zip` is an unchanged copy of the official `compact.zip` archive of KEMAR HRTF measurements by Bill Gardner and Keith Martin, MIT Media Laboratory (1994). The original 15 separately named WAV files are retained for compatibility/reference; the renderer's embedded archive now uses the original MIT entry names.

The source archive states: Copyright 1994 by the MIT Media Laboratory. The data is provided free with no restrictions on use, including research and commercial applications, provided the authors are cited when the data is used.

Source: https://sound.media.mit.edu/resources/KEMAR.html

Downloaded archive: https://sound.media.mit.edu/resources/KEMAR/compact.zip

Technical report: https://sound.media.mit.edu/resources/KEMAR/hrtfdoc.txt

Verified archive size: 231461 bytes.

Verified archive SHA-256: `0BCD69F8E8760CF8EACFF4A89594CECA7D44FB94F28ECF0BF19970003BBFB3E0`.

Suggested attribution: Bill Gardner and Keith Martin, MIT Media Laboratory, KEMAR HRTF Measurements, 1994.

The compact responses are 44.1 kHz, 16-bit stereo, 128-sample HRIR WAV files. MIT derived the compact data from the left-ear measurements, symmetrised it, and compensated for the measurement speaker's frequency response. This is measured KEMAR data, not a personalised listener HRTF.

The renderer uses all 368 measured directions in the archive: fourteen elevation rows from -40 to +90 degrees in 10-degree steps. Each row covers azimuths from 0 up to at most 180 degrees; negative azimuths use the corresponding response with the ears exchanged. Zero azimuth is in front, +90 degrees is to the listener's right, and 180 degrees is behind. +90 degrees elevation is directly overhead. Nothing was measured below -40 degrees, so lower directions use the -40 degree row.

Archive paths have the form `elev{elevation}/H{elevation}e{azimuth:000}a.wav`, for example `elev-40/H-40e006a.wav` and `elev0/H0e180a.wav`. File names round each measured angle to an integer.

| Elevation | Directions round the circle | Step | Files (0 to 180 degrees) |
| --- | --- | --- | --- |
| -40, +40 | 56 | 6.43 degrees | 29 |
| -30, +30 | 60 | 6 degrees | 31 |
| -20 to +20 | 72 | 5 degrees | 37 |
| +50 | 45 | 8 degrees | 23 (ends at 176 degrees) |
| +60 | 36 | 10 degrees | 19 |
| +70 | 24 | 15 degrees | 13 |
| +80 | 12 | 30 degrees | 7 |
| +90 | 1 | - | 1 |

To regenerate the embedded C++ resources from the pinned local archive, run `powershell -File Tools/GenerateKemarResources.ps1` from the project root. Add `-Download` to retrieve the official source again. The script verifies the pinned archive hash and all 368 WAV formats before replacing generated files. The embedded symbols `kemar_hrir_zip` and `kemar_hrir_zipSize` are preserved.
