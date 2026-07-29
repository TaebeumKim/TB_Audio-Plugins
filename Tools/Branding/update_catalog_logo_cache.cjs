const fs = require("fs");
const path = require("path");

const repoRoot = path.resolve(__dirname, "..", "..");
const managedLogoNames = new Set(
  fs
    .readdirSync(path.join(repoRoot, "assets", "brand", "plugin-icons"))
    .filter((name) => name.endsWith(".png"))
);
const catalogFiles = [
  path.join(repoRoot, "catalog.json"),
  path.join(repoRoot, "catalog-macos-update-test.json"),
];
const cacheKey = "mark-noise-connected-silver-headphones-20260729";

for (const filePath of catalogFiles) {
  const source = fs.readFileSync(filePath, "utf8");
  const updated = source.replace(
    /(Hub\/Logos\/([^"?]+\.png))\?v=[^"]+/g,
    (match, iconPath, logoName) =>
      managedLogoNames.has(logoName) ? `${iconPath}?v=${cacheKey}` : match
  );
  const catalog = JSON.parse(updated);
  const iconUrls = catalog.plugins?.map((plugin) => plugin.iconUrl) ?? [];
  const managedUrls = iconUrls.filter((url) => {
    const match =
      typeof url === "string"
        ? url.match(/\/Hub\/Logos\/([^"?]+\.png)\?v=[^"?]+$/)
        : null;
    return match && managedLogoNames.has(match[1]);
  });
  const validUrls = managedUrls.filter((url) =>
    url.endsWith(`?v=${cacheKey}`)
  );
  const expectedCount = managedLogoNames.size;
  if (
    managedUrls.length !== expectedCount ||
    validUrls.length !== expectedCount ||
    new Set(validUrls).size !== expectedCount
  ) {
    throw new Error(
      `${path.basename(filePath)}: expected ${expectedCount} unique catalog logo URLs`
    );
  }
  fs.writeFileSync(filePath, updated, "utf8");
  process.stdout.write(
    `${path.basename(filePath)}: updated ${validUrls.length} logo URLs\n`
  );
}
