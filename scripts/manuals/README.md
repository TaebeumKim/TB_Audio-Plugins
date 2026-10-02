# User manual generator

Builds the English and Korean PDF manuals in `Documents/ENG` and `Documents/KOR`
from the content files in `content/`.

```sh
cd scripts/manuals
npm install            # Playwright + the Pretendard font (OFL-1.1)
npx playwright install chromium   # only if Chromium is not installed yet
node build.cjs                    # all manuals
node build.cjs tb_eq --lang ko    # one plug-in, one language
node build.cjs tb_eq --out /tmp   # write somewhere else for review
```

## Files

- `content/<catalog-id>.<lang>.json`: the manual text. `file` sets the PDF base name, and
  `body` is a list of blocks: `h2`, `h3`, `p`, `lead`, `ul`, `ol`, `table`, `note`, `tip`,
  `warn`, `new`, `ui`, `flow`, `recipes`, `pagebreak`.
- Inline markup: `**bold**`, `` `PANEL LABEL` ``, and `{{n}}` for an interface callout number.
- `ui/<catalog-id>.jpg`: the interface capture. A `ui` block places numbered callouts on it with
  `x`/`y` percentages of the image size.
- `manual.css`: the page design. Icons come from `assets/brand/plugin-icons`, and the plug-in
  name comes from `catalog.json`.

## Writing rules

- Use the panel labels exactly as the plug-in shows them, in English in every language.
- Every range and default must come from the plug-in source or the shipped binary. Describe
  only controls that exist; mark host-only parameters as such.
- Recipe values are starting points; say so in the text.
