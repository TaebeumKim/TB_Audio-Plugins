const fs = require("fs");
const path = require("path");

const repoRoot = path.resolve(__dirname, "..", "..");
const catalogFiles = [
  path.join(repoRoot, "catalog.json"),
  path.join(repoRoot, "catalog-macos-update-test.json"),
];
const cacheKeys = new Map([
  ["tb_noise_remover.png", "mark-noise-white-top-connected-20260729"],
  ["tb_parallel_reverb.png", "mark-reverb-softer-decay-20260729"],
  ["tb_delay.png", "mark-delay-shrinking-echoes-20260729"],
  ["tb_tune.png", "mark-tune-symmetric-mic-20260729"],
]);

for (const filePath of catalogFiles) {
  const source = fs.readFileSync(filePath, "utf8");
  const updated = source.replace(
    /(Hub\/Logos\/([^"?]+\.png))\?v=[^"]+/g,
    (match, iconPath, logoName) =>
      cacheKeys.has(logoName)
        ? `${iconPath}?v=${cacheKeys.get(logoName)}`
        : match
  );
  const catalog = JSON.parse(updated);
  const iconUrls = catalog.plugins?.map((plugin) => plugin.iconUrl) ?? [];
  const pluginIds = new Set(
    catalog.plugins?.map((plugin) => plugin.id) ?? []
  );
  const expectedLogoNames = new Set(
    [...cacheKeys.keys()].filter((logoName) =>
      pluginIds.has(logoName.replace(/\.png$/, ""))
    )
  );
  const managedUrls = iconUrls.filter((url) => {
    const match =
      typeof url === "string"
        ? url.match(/\/Hub\/Logos\/([^"?]+\.png)\?v=[^"?]+$/)
        : null;
    return match && expectedLogoNames.has(match[1]);
  });
  const validUrls = managedUrls.filter((url) => {
    const match = url.match(/\/Hub\/Logos\/([^"?]+\.png)\?v=([^"?]+)$/);
    return match && match[2] === cacheKeys.get(match[1]);
  });
  const expectedCount = expectedLogoNames.size;
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
