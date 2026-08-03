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
- Filename: `<catalog-id>.jpg`
- Canvas: `1536 x 1024`
- JPEG quality: `92`
- Composition: the real plug-in UI capture placed over a shared DAW session backdrop

The plug-in UI itself is kept from the project QA or Standalone capture. The shared backdrop is used only to present each plug-in in an active music-production context.

When a plug-in is added to or removed from the catalog, update this folder in the same commit so the set of image filenames continues to match the catalog IDs exactly.
