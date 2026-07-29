# TB plug-in brand assets

The GitHub and TB Hub logo family is derived from the approved Team Impulse Impact
line mark and the integrated-redraw registry in `TB_PLUGIN_UI_DESIGN_GUIDE.md`
1.31 or later and `TB_PLUGIN_ICON_GUIDE.md` 3.10 or later.

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

The approved raster is the structural source for the body, eyes, rear connection
and folded tail, not an instruction to expose every contour simultaneously.
Integrated additions must use real painter-order occlusion: rear parts disappear
behind opaque front body, tail or accessory surfaces. Do not force every rail to
remain visible with global black separator halos or trenches.

Do not place an unregistered prop, badge, letter, rounded card or decorative
panel beside or on top of an otherwise unchanged mark. The product registry
explicitly controls the few scene elements that are allowed, including Tune's
front microphone, Transient Shaper's integrated anatomically normal muscular
arms sprouting from both sides of the fish body, Step
Shifter's stairs, Jewel Digger's single sparkle, Noise Remover's worn headphones
and Parallel Reverb's gradient bloom. QA contact-sheet labels are outside the
exported icons and are not product artwork.

Noise Remover must structurally preserve the original fish body and double
folded tail while showing the fish actually wearing headphones. Draw the tail
first, lower its upper apex by only `8` units to suggest gentle pressure, and
place one closed, black-filled headband over it. The band path is
`M181 211 C186 151 209 119 240 120 C276 121 302 151 306 201 L281 204
C278 168 262 146 241 146 C219 146 205 169 202 215 Z`, with a white `7`
outline. Its black face must occlude the crossed tail contours so the band reads
as resting on the tail rather than piercing it. Draw the body and eyes next,
then the black-filled outer ear cup and inner pad immediately behind the eyes.
The band's right endpoint `(306,201)` must overlap the outer cup curve near
`(307,205)` for a seamless attachment. Do not add a separate yoke, global
separator halo or trench. The folded tail remains distinct from the headphone;
do not mistake, replace, erase or masquerade it as the band.

Transient Shaper must structurally preserve the original fish body and double
folded tail without replacing the tail or treating it as an arm. Grow two
anatomically normal muscular human arms naturally from both sides of the body in
a conventional symmetrical double-biceps flex pose. Both arm transforms use a
Y translation of `128`, lowering the shoulders and body attachments. Preserve
and enlarge the two original eyes, then add inward-descending white eyebrows for
an angry face. Painter order is tail/rear connections, both opaque black-filled
arms, black body fill, a body outline masked only at both attachments, enlarged
original eyes, then angry brows. The body fill naturally hides both lowered
roots. Do not add shoulder bridges, separator halos or trenches. The near-side
right arm may naturally occlude the tail where they cross.

XYZ Panner preserves the approved original fish's V mouth, two unequal eyes,
rounded body and folded tail as a volumetric extruded object in 3D coordinate
space. Do not rotate the whole icon or fish 45 degrees in the image plane.
Instead, use a camera-yaw view like looking diagonally across an FFT analyzer
waterfall: project the receding scene axis exactly 45 degrees up and right on
screen (`|dx| = |dy|`). Align the fish extrusion with that same up-right
45-degree depth direction and apply directional shading. Surround it with three
non-text, differently coloured axes ending in arrowheads, a perspective floor
grid and wireframe volume, plus a ground projection/glow. The FFT analyzer is a
viewpoint analogy only; do not add FFT bars, waveforms, spectrum decoration or
text. Do not use axis labels or `X/Y/Z` text, real fish, photography, a
polygon-only replacement, cards or bezels.

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
For 64 px product variants, require recognisable original lineage and the product
accessory or transformation, not simultaneous visibility of every occluded
canonical contour.

Transient Shaper must retain two muscular arms, the angry face and fish lineage;
XYZ Panner must retain the original V mouth, two eyes and folded tail plus
recognisable three-colour axes at 64 px. Its receding axis and fish extrusion
must share the same on-screen up-right 45-degree direction. Noise Remover must
retain a recognisable over-tail closed band-to-cup wearing structure; the band
must visibly interrupt the tail contours and join the upper/rear cup.

Automatic verification also requires a thresholded raster/vector mask IoU of at
least `0.98`; the current deterministic trace is approximately `0.99`.

## Publishing boundary

Changing tracked PNG files does not rebuild the TB Hub executable. It updates raw
GitHub images and portfolio fallback source only. GitHub repository Social
Preview is a separate setting and must be replaced with the generated
`1280 × 640` file after the asset change is approved.
