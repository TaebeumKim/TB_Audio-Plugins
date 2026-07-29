# TB plug-in brand assets

The GitHub and TB Hub logo family is derived from the approved Team Impulse Impact
line mark and the integrated-redraw registry in `TB_PLUGIN_UI_DESIGN_GUIDE.md`
1.28 or later and `TB_PLUGIN_ICON_GUIDE.md` 3.7 or later.

## Canonical inputs

1. Approved raster reference:
   `assets/brand/source/team-impulse-mark-reference.png`
2. Deterministic vector trace, generated from that exact raster:
   `assets/brand/source/team-impulse-mark.svg`
3. Pinned XYZ Panner photography source:
   `assets/brand/source/xyz-panner-fish-photography.png`
4. Trace tool:
   `Tools/Branding/trace_team_impulse_mark.cjs`
5. Product redraws:
   `Tools/Branding/generate_plugin_brand_assets.cjs`

The approved reference is a `500 × 500` opaque black square with a single white
line mark. It is not lettering. The verifier pins its SHA-256 so it cannot be
silently replaced.

The XYZ Panner source is a `1024 × 1024` raster pinned to SHA-256
`BEE1E6806F706310682A7545F6BDD7F3D8CC3A3A275319E41AA8A4423CAADB60`.
It is generated and approved once, then treated as an immutable deterministic
raster input. The generator consumes this exact file; it must not regenerate,
reinterpret or procedurally redraw it.

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
folded tail while showing the fish actually wearing headphones. Mask the rear
headband behind `BODY` and the closed tail-ribbon silhouette, draw the complete
base mark, then place a black-filled front ear cup immediately behind the eyes
at the implied ear position. The cup may naturally hide the body and tail below
it. Do not use a global black separator stroke to expose the complete band, and
do not replace, erase or masquerade as the tail.

Transient Shaper must structurally preserve the original fish body and double
folded tail without replacing the tail or treating it as an arm. Grow two
anatomically normal muscular human arms naturally from both sides of the body in
a conventional symmetrical double-biceps flex pose. Preserve and enlarge the
two original eyes, then add inward-descending white eyebrows for an angry face.
Painter order is tail/rear connections, both opaque black-filled arms, black body
fill, a body outline masked only at both attachments, enlarged original eyes,
then angry brows. The body fill hides both roots. Do not add shoulder bridges,
separator halos or trenches. The near-side right arm may naturally occlude the
tail where they cross.

XYZ Panner is the deliberately isolated photography exception. It uses one real
olive flounder / Japanese flounder (`Paralichthys olivaceus`), showing the whole
eyed upper side with both eyes on the same side and a broad, flat, mottled
olive-brown body. The fish is centred on a black seamless background as square
studio/macro wildlife photography. Do not use illustration, CGI, toys, 3D,
vectors, scenery, text, cards or watermarks. At 64 px the flatfish silhouette
must remain legible.

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
XYZ Panner must retain the broad flatfish silhouette.

Automatic verification also requires a thresholded raster/vector mask IoU of at
least `0.98`; the current deterministic trace is approximately `0.99`.

## Publishing boundary

Changing tracked PNG files does not rebuild the TB Hub executable. It updates raw
GitHub images and portfolio fallback source only. GitHub repository Social
Preview is a separate setting and must be replaced with the generated
`1280 × 640` file after the asset change is approved.
