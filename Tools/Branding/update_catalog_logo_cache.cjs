const fs = require("fs");
const path = require("path");

const repoRoot = path.resolve(__dirname, "..", "..");
const catalogFiles = [
  path.join(repoRoot, "catalog.json"),
  path.join(repoRoot, "catalog-macos-update-test.json"),
];
const cacheKey = "brand-20260729";

for (const filePath of catalogFiles) {
  const source = fs.readFileSync(filePath, "utf8");
  const updated = source.replace(
    /(Hub\/Logos\/[^"?]+\.png)\?v=[^"]+/g,
    `$1?v=${cacheKey}`
  );
  const catalog = JSON.parse(updated);
  const iconUrls = catalog.plugins?.map((plugin) => plugin.iconUrl) ?? [];
  const validUrls = iconUrls.filter(
    (url) =>
      typeof url === "string" &&
      /\/Hub\/Logos\/[^"?]+\.png\?v=[^"?]+$/.test(url) &&
      url.endsWith(`?v=${cacheKey}`)
  );
  if (
    iconUrls.length !== 20 ||
    validUrls.length !== 20 ||
    new Set(validUrls).size !== 20
  ) {
    throw new Error(
      `${path.basename(filePath)}: expected 20 unique catalog logo URLs`
    );
  }
  fs.writeFileSync(filePath, updated, "utf8");
  process.stdout.write(
    `${path.basename(filePath)}: updated ${validUrls.length} logo URLs\n`
  );
}
