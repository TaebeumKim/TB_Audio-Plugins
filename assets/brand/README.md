# TB plug-in icon assets

The GitHub and TB Hub icon family uses the approved **1e DISPLAY** system.

Every product shares one rounded graphite chassis, one recessed display bezel,
and two vertical display-grid lines. Only the product glyph and its registered
colour change.

## Canonical sources

- `assets/brand/source/display-icons/*.svg`: 64-unit vector masters
- `assets/brand/plugin-icons/*.png`: 1024 × 1024 raster masters
- `Hub/Logos/*.png`: 256 × 256 GitHub/TB Hub icons
- `assets/plugins/*.png`: byte-identical 256 × 256 offline fallbacks
- `assets/social/tb_audio_plugins_social_preview.png`: 1280 × 640 family preview

The two 256px copies for a product must be byte-identical.

## Family rules

- Keep the shared chassis, bezel, scale and grid geometry unchanged.
- Do not add product names, initials, numbers, text or a separate mascot.
- Make the product function readable at 32px.
- Keep product colour to a restrained area of the tile.
- Render every PNG directly from its SVG master.

## Filename aliases

| Source product | Repository filename |
| --- | --- |
| TB Tune (`TB_AutoTune` local project) | `tb_tune` |
| TB Jewel Digger & Finder | `tb_jewel_digger` |
| TB Spectral Transient Shaper | `tb_transient_shaper` |

The repository currently carries 30 product icons. `TB_UnityLUFSMeter` remains
excluded until it becomes a built release target.

## Rollback note

Commit `0b2a38f` is the verified 21-product DISPLAY baseline. The fish-mark
generator and its derived assets were introduced by the later incorrect icon
upload and were removed during the DISPLAY content rollback. The three additional
repository assets are Board, Resonator and Ring Modulation. Gender Changer is
permanently retired and replaced by Colorizer.
