# TB plug-in brand assets

The GitHub and TB Hub logo family is derived from the approved Team Impulse Impact
line mark and the integrated-redraw registry in `TB_PLUGIN_UI_DESIGN_GUIDE.md`
1.24 or later and `TB_PLUGIN_ICON_GUIDE.md` 3.3 or later.

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
mark. The folded head-tail is a canonical brand signature, not the default
semantic carrier for every product. Do not distinguish the family by turning
only that tail into a different object each time. Distribute the effect across
the body, face or expression, whole silhouette, guide planes, repetition or an
explicitly approved background effect.

Do not place an unregistered prop, badge, letter, rounded card or decorative
panel beside or on top of an otherwise unchanged mark. The product registry
explicitly controls the few scene elements that are allowed, including Tune's
front microphone, Transient Shaper's anatomically normal flexed arm, Step
Shifter's stairs, Jewel Digger's single sparkle and Parallel Reverb's gradient
bloom. QA contact-sheet labels are outside the exported icons and are not
product artwork.

XYZ Panner is the deliberately isolated rendering exception: it must be a fully
rendered in-game 3D toy fish object with smooth coloured material, directional
lighting, specular and rim highlights, visible extrusion and depth, a thick
material tail, a projected ground glow or shadow, and volumetric eyes. It must
not be reduced to white outline art, wireframe, flat polygons or facet-line
illustration, and must remain immediately and humorously different from the
other marks at 64 px. This exception does not authorize a suite-wide 3D style.

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
