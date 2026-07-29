# TB plug-in brand assets

The GitHub and TB Hub logo family is derived from the approved Team Impulse Impact
line mark and the integrated-redraw registry in `TB_PLUGIN_UI_DESIGN_GUIDE.md`
1.22 or later.

## Canonical inputs

1. Approved raster reference:
   `assets/brand/source/team-impulse-mark-reference.png`
2. Deterministic vector trace, generated from that exact raster:
   `assets/brand/source/team-impulse-mark.svg`
3. Trace tool:
   `Tools/Branding/trace_team_impulse_mark.cjs`
4. Product redraws:
   `Tools/Branding/generate_plugin_brand_assets.cjs`

The approved reference is a `500 × 500` opaque black square with a single white
line mark. It is not lettering. The verifier pins its SHA-256 so it cannot be
silently replaced.

## Redraw rule

Each product icon must be redrawn as one coherent descendant of the canonical
mark. An effect may reshape, repeat, compress, split, pixelate, colour or extrude
the original body, eyes and folded tail.

Do not place a separate prop, badge, letter, rounded card or decorative panel
beside or on top of an otherwise unchanged mark. Props named in the product brief
must be formed by replacing or extending the mark's own paths. QA contact-sheet
labels are outside the exported icons and are not product artwork.

## Outputs

- `assets/brand/plugin-icons/*.png`: `1024 × 1024` opaque black square masters
- `Hub/Logos/*.png`: `256 × 256` GitHub/TB Hub icons
- `assets/plugins/*.png`: byte-identical `256 × 256` offline fallbacks
- `assets/social/tb_audio_plugins_social_preview.png`: `1280 × 640` repository preview

## Generate and verify

From `Tools/Branding`:

```powershell
npm install
npm run trace-reference
npm run generate
npm run update-catalog-cache
npm run verify
```

The dependency is pinned to `sharp 0.34.5`. Generation also writes three
local-only QA images to the repository sibling `../output/github-logo-qa`
(resolved from the repository root):

- `team-impulse-mark-reference-vs-vector.png`
- `plugin-icons-200px-contact-sheet.png`
- `plugin-icons-64px-contact-sheet.png`

Inspect all three whenever any geometry changes. The reference/vector comparison
must preserve the V-shaped mouth, two unequal white circles, rounded body,
double folded tail, uniform white stroke and roughly 20% black outer margin.
Automatic verification also requires a thresholded raster/vector mask IoU of at
least `0.98`; the current deterministic trace is approximately `0.99`.

## Publishing boundary

Changing tracked PNG files does not rebuild the TB Hub executable. It updates raw
GitHub images and portfolio fallback source only. GitHub repository Social
Preview is a separate setting and must be replaced with the generated
`1280 × 640` file after the asset change is approved.
