# Portfolio plug-in mockups

Upload one portfolio image per plug-in using its catalog `id` as the filename.

Supported extensions:

- `.png`
- `.webp`
- `.jpg`
- `.jpeg`

Examples:

- `tb_center.png`
- `tb_eq.webp`
- `tb_parallel_reverb.jpg`

The portfolio reads this folder dynamically. A matching image overrides the catalog icon; plug-ins without a mockup continue to use their existing local artwork or catalog icon.

## Current mockup set

- Coverage: all 29 plug-ins in `catalog.json`
- Additional coverage: 2 projects that are not in `catalog.json`
- Filename: `<catalog-id>.jpg`
- Canvas: `1536 x 1024`
- JPEG quality: `92`
- Composition: the real plug-in UI capture centered without a frame or backing panel
- Background: a blurred black gradient with a soft central glow and dark vignette
- Focus: a diffuse glow makes the plug-in appear to float above the background
- Callouts: exactly three concise English feature explanations with short leader lines pointing into the UI
- Target priority: analyzer, meter, or graph first; then core knobs; then buttons or selectors
- Overlap handling: labels that cross the UI use a compact borderless glass panel, and label collisions are automatically rerouted

The plug-in UI itself is kept crisp from the latest project QA capture or a fresh snapshot of the installed VST3. Callout copy is based on the plug-in brief or release notes and uses a short uppercase title plus one plain-English sentence. Every target must exist in the captured UI; never invent a control or feature to fill a callout. Labels stay close to their target and may overlay non-critical parts of the UI only when the local glass panel keeps the copy readable.

The additional portfolio projects are:

- `tb.audio-player.jpg` — TB AudioPlayer, prepared ahead of catalog registration
- `tb_board.jpg` — TB Board, visual coverage only; catalog and release remain intentionally excluded

These files become active portfolio images only after a matching `id` is added to `catalog.json`. When a plug-in is added to or removed from the catalog, update this folder in the same commit so every catalog ID continues to have a matching image.

TB Gender Changer is permanently retired and must not be regenerated or added back to the catalog. TB Colorizer is its replacement.
