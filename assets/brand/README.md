# TB plug-in brand assets

The GitHub and TB Hub logo family is generated from one canonical Team Impulse Impact fish vector and the product-specific metaphor registry in `TB_PLUGIN_UI_DESIGN_GUIDE.md` 1.20 or later.

## Canonical inputs

- Fish vector: `assets/brand/source/team-impulse-fish.svg`
- Generator: `Tools/Branding/generate_plugin_brand_assets.cjs`
- Product registry and colours: the `products` table inside the generator

Do not redraw the fish with a generative-image model and do not edit exported PNG files by hand.

## Outputs

- `assets/brand/plugin-icons/*.png`: `1024 × 1024` square masters
- `Hub/Logos/*.png`: `256 × 256` GitHub/TB Hub icons
- `assets/plugins/*.png`: byte-identical `256 × 256` offline fallbacks
- `assets/social/tb_audio_plugins_social_preview.png`: `1280 × 640` repository preview

The square icons use transparent corners. The repository preview uses a solid background so it remains predictable on platforms that ignore PNG transparency.

## Generate and verify

From `Tools/Branding`:

```powershell
npm install
npm run generate
npm run update-catalog-cache
npm run verify
```

The dependency is pinned to `sharp 0.34.5`. Generation also writes two local-only QA sheets to the repository sibling `../output/github-logo-qa` (resolved from the repository root):

- `plugin-icons-200px-contact-sheet.png`
- `plugin-icons-64px-contact-sheet.png`

Both sheets must be inspected whenever any metaphor changes. At `64 px`, the fish count, defining prop and major transformation must remain recognizable, except for Scrambler where reduced recognizability is intentional.

## Publishing boundary

Changing the tracked PNG files does not rebuild the TB Hub executable. It updates the raw GitHub images and portfolio fallback source only. GitHub repository Social Preview is a separate repository setting and must be uploaded from the generated `1280 × 640` file after the asset change is approved.
