# Spatial regression harness

Run `Tests/Run-SpatialRegression.ps1` in PowerShell after the production Release
x64 SharedCode library has been built. Use `-BuildSharedCode` to build that
library first. The script refuses a stale library, rebuilds the single test
translation unit, links the actual production library, and records the result
in `Tests/out/last-run.log`. It does not install or publish the plugin.

The test project imports production JUCE definitions and the MSVC runtime
configuration; use the script so its global executable/output overrides apply
before the imported build targets are evaluated. Do not run it while production
headers or the SharedCode library are being edited/rebuilt.

Checks cover the bus contract and state roundtrip; measured front/rear KEMAR
waveform identity; left/right ear level, arrival and mirror symmetry; elevation;
stereo source and antiphase survival in headphone mode; every surround input
channel including LFE; native surround identity; 44.1/48/96/192 kHz finite
processing; reset; direction and distance automation; and distance-filter
bypass transitions. The 12 kHz sine transition check measures adjacent
quadrature-sample envelope changes rather than treating normal waveform slope
as a click. Tests use the production processor/renderer, not a copied panning
implementation.

Additional regressions cover 22.05/24/32 kHz neutral and distant/rear/elevated
processing, expired FIR history on headphone-mode re-entry, Height automation
clicks, static shelf transfer-function preservation, and automation independence
from 64 versus 256-sample processing blocks.

Listener-relative placement checks cover the stereo pan law on the default
front line, full left/right images beside the listener at any radius, rear-arc
folding, constant power, the near-field spread in stereo, surround and
headphone output, every bundled elevation row against its own measurement,
overhead placement and continuity across the zenith, interpolation between
measured directions over all 108 gaps of three rows, and headphone level and
12 kHz response at 48/96/192 kHz against 44.1 kHz. `prepareToPlay` time is
gated at 48 kHz and reported at other rates.

## Editor regression

Run `Tests/Run-EditorRegression.ps1` after rebuilding SharedCode. It links the
same production library and instantiates real JUCE components without controlling
the desktop. `-SnapshotPrefix after` writes `Tests/out/after-tests.log` and
default/extreme/150-percent PNG snapshots. Coordinate tests locate the painted
marker rather than duplicating the implementation's inverse mapping. They test
marker-click round trips, host gesture balance, two-axis undo/redo, right-click
protection, equal square planes, equal knob sizes, control overlap, Front/Back
access, attachment values, painted pointer direction, rapid knob/curve edits,
and mode-button undo/redo. Snapshot rendering is not a DAW mouse/keyboard or
operating-system DPI test.

CPU ratios include the test input generation, are specific to the running
machine, and are not DAW real-time guarantees. 192 kHz results are observations;
the broad faster-than-audio-duration gate is applied at 48 kHz. No listening,
desktop/DAW GUI interaction, whole-callback allocation, multi-instance, or
long-duration validation is claimed.
