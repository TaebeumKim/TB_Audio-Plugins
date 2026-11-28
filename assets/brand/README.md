# TB plug-in icon assets

The GitHub and TB Hub logo slots use the canonical `TB_Logo.png` fish mark:
white glow on a black ground. Product cards may still use their own portfolio
artwork, but every plugin icon slot must use this canonical mark.

## Canonical sources

- `assets/brand/plugin-icons/*.png`: 1024 × 1024 raster masters
- `Hub/Logos/*.png`: 256 × 256 GitHub/TB Hub icons
- `assets/plugins/*.png`: byte-identical 256 × 256 offline fallbacks
- `assets/social/tb_audio_plugins_social_preview.png`: 1280 × 640 family preview

The two 256px copies for a product must be byte-identical. Keep the 1024px
master opaque so the glow is retained by renderers that discard transparent
RGB pixels.

## Filename aliases

| Source product | Repository filename |
| --- | --- |
| TB Tune (`TB_AutoTune` local project) | `tb_tune` |
| TB Jewel Digger & Finder | `tb_jewel_digger` |
| TB Spectral Transient Shaper | `tb_transient_shaper` |

The repository currently carries 37 product icons. `TB_UnityLUFSMeter` remains
excluded until it becomes a built release target.

## Rollback note

The source image for the current logo set is kept outside the repository and
copied into the three canonical slots during a branding update. The generated
standard files are kept in the release-QA staging area for rollback.
