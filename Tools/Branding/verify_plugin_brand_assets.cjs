const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const sharp = require("sharp");

const repoRoot = path.resolve(__dirname, "..", "..");
const masterDir = path.join(repoRoot, "assets", "brand", "plugin-icons");
const hubDir = path.join(repoRoot, "Hub", "Logos");
const fallbackDir = path.join(repoRoot, "assets", "plugins");
const socialPreview = path.join(
  repoRoot,
  "assets",
  "social",
  "tb_audio_plugins_social_preview.png"
);
const referenceMark = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark-reference.png"
);
const canonicalMark = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark.svg"
);
const referenceMarkSha256 =
  "be0295014ec34eae3076aee26d5727e6ef57474acf95c76f176a5319749041d6";

const ids = [
  "tb_center",
  "tb_compressor",
  "tb_delay",
  "tb_distortion",
  "tb_disperser",
  "tb_eq",
  "tb_colorizer",
  "tb_inverted_flanger",
  "tb_inverted_phaser",
  "tb_jewel_digger",
  "tb_noise_remover",
  "tb_parallel_reverb",
  "tb_scrambler",
  "tb_transient_shaper",
  "tb_step_shifter",
  "tb_tune",
  "tb_volume",
  "tb_vocoder",
  "tb_xyz_panner",
  "tb_limiter",
  "tb_sublow",
];

function sha256(buffer) {
  return crypto.createHash("sha256").update(buffer).digest("hex");
}

async function canonicalMaskIou() {
  const reference = await sharp(referenceMark).greyscale().raw().toBuffer();
  const vector = await sharp(canonicalMark)
    .resize(500, 500)
    .flatten({ background: "#000000" })
    .greyscale()
    .raw()
    .toBuffer();
  let intersection = 0;
  let union = 0;
  for (let index = 0; index < reference.length; index += 1) {
    const referenceIsLight = reference[index] > 127;
    const vectorIsLight = vector[index] > 127;
    if (referenceIsLight && vectorIsLight) intersection += 1;
    if (referenceIsLight || vectorIsLight) union += 1;
  }
  return intersection / union;
}

async function verifyPng(
  filePath,
  expectedWidth,
  expectedHeight,
  minLightRatio = 0.003,
  maxLightRatio = 0.45
) {
  if (!fs.existsSync(filePath)) {
    throw new Error(`Missing asset: ${filePath}`);
  }

  const image = sharp(filePath);
  const metadata = await image.metadata();
  if (
    metadata.format !== "png" ||
    metadata.width !== expectedWidth ||
    metadata.height !== expectedHeight
  ) {
    throw new Error(
      `Unexpected image metadata for ${filePath}: ` +
        `${metadata.format} ${metadata.width}x${metadata.height}`
    );
  }

  const { data, info } = await image.ensureAlpha().raw().toBuffer({
    resolveWithObject: true,
  });
  const cornerOffsets = [
    0,
    (info.width - 1) * info.channels,
    (info.height - 1) * info.width * info.channels,
    (info.width * info.height - 1) * info.channels,
  ];
  if (
    cornerOffsets.some(
      (offset) =>
        data[offset] !== 0 ||
        data[offset + 1] !== 0 ||
        data[offset + 2] !== 0 ||
        data[offset + 3] !== 255
    )
  ) {
    throw new Error(`Canvas corner is not opaque black: ${filePath}`);
  }

  let lightPixels = 0;
  let translucentPixels = 0;
  for (let offset = 0; offset < data.length; offset += info.channels) {
    const luminance =
      data[offset] * 0.2126 +
      data[offset + 1] * 0.7152 +
      data[offset + 2] * 0.0722;
    if (luminance > 32) lightPixels += 1;
    if (data[offset + 3] !== 255) translucentPixels += 1;
  }
  if (translucentPixels !== 0) {
    throw new Error(`Canvas is not fully opaque: ${filePath}`);
  }
  const lightRatio = lightPixels / (expectedWidth * expectedHeight);
  if (lightRatio < minLightRatio || lightRatio > maxLightRatio) {
    throw new Error(
      `Unexpected light-pixel ratio for ${filePath}: ${lightRatio.toFixed(4)}`
    );
  }
}

async function main() {
  await verifyPng(referenceMark, 500, 500, 0.02, 0.08);
  if (sha256(fs.readFileSync(referenceMark)) !== referenceMarkSha256) {
    throw new Error("Canonical reference PNG does not match the approved source");
  }
  if (!fs.existsSync(canonicalMark)) {
    throw new Error(`Missing canonical vector: ${canonicalMark}`);
  }
  const canonicalSvg = fs.readFileSync(canonicalMark, "utf8");
  if (
    !canonicalSvg.includes('viewBox="0 0 500 500"') ||
    /<text\b/i.test(canonicalSvg) ||
    /<image\b/i.test(canonicalSvg)
  ) {
    throw new Error("Canonical vector must be a text-free 500×500 native mark");
  }
  const markIou = await canonicalMaskIou();
  if (markIou < 0.98) {
    throw new Error(
      `Canonical vector is not a precise trace of the approved mark: IoU ${markIou.toFixed(4)}`
    );
  }
  for (const id of ids) {
    const master = path.join(masterDir, `${id}.png`);
    const hub = path.join(hubDir, `${id}.png`);
    const fallback = path.join(fallbackDir, `${id}.png`);
    await verifyPng(master, 1024, 1024);
    await verifyPng(hub, 256, 256);
    await verifyPng(fallback, 256, 256);

    const hubHash = sha256(fs.readFileSync(hub));
    const fallbackHash = sha256(fs.readFileSync(fallback));
    if (hubHash !== fallbackHash) {
      throw new Error(`Hub/fallback mismatch for ${id}`);
    }

    const expectedRuntime = await sharp(master)
      .resize(256, 256, { kernel: sharp.kernel.lanczos3 })
      .png({
        compressionLevel: 9,
        adaptiveFiltering: true,
        palette: true,
        quality: 100,
      })
      .toBuffer();
    if (hubHash !== sha256(expectedRuntime)) {
      throw new Error(`Runtime icon is not derived from the master for ${id}`);
    }
  }

  await verifyPng(socialPreview, 1280, 640, 0.01, 0.5);
  process.stdout.write(
    `PASS: approved reference, canonical vector (IoU ${markIou.toFixed(4)}), ` +
      `${ids.length} masters, ` +
      `${ids.length * 2} runtime copies, and the social preview are valid.\n`
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
