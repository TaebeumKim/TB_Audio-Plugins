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

- Coverage: all 25 plug-ins in `catalog.json`
- Additional coverage: 4 installed plug-ins that are not yet in `catalog.json`
- Filename: `<catalog-id>.jpg`
- Canvas: `1536 x 1024`
- JPEG quality: `92`
- Composition: the real plug-in UI capture placed over a shared DAW session backdrop
- Background: blurred and darkened so the feature copy remains readable
- Callouts: three concise English feature explanations with leader lines pointing into the UI

The plug-in UI itself is kept crisp from the project QA or Standalone capture. The shared backdrop is used only to present each plug-in in an active music-production context. Callout copy should be based on the plug-in brief or release notes, use a short uppercase title plus one plain-English sentence, and avoid covering important controls.

The installed plug-ins currently staged ahead of catalog registration are:

- `tb.audio-player.jpg` — TB AudioPlayer
- `tb_board.jpg` — TB Board
- `tb_gender_changer.jpg` — TB Gender Changer
- `tb_shimmer.jpg` — TB Shimmer

These four files become active portfolio images only after a matching `id` is added to `catalog.json`. When a plug-in is added to or removed from the catalog, update this folder in the same commit so every catalog ID continues to have a matching image.
