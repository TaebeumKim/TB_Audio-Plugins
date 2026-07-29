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

const ids = [
  "tb_center",
  "tb_compressor",
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

async function verifyPng(
  filePath,
  expectedWidth,
  expectedHeight,
  requireTransparentCorners = true
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
  const cornerAlpha = [
    data[3],
    data[(info.width - 1) * info.channels + 3],
    data[(info.height - 1) * info.width * info.channels + 3],
    data[(info.width * info.height - 1) * info.channels + 3],
  ];
  if (requireTransparentCorners && cornerAlpha.some((alpha) => alpha !== 0)) {
    throw new Error(`Rounded-square corner is not transparent: ${filePath}`);
  }

  let visiblePixels = 0;
  for (let offset = 3; offset < data.length; offset += info.channels) {
    if (data[offset] > 12) visiblePixels += 1;
  }
  if (visiblePixels < expectedWidth * expectedHeight * 0.22) {
    throw new Error(`Asset is unexpectedly empty: ${filePath}`);
  }
}

async function main() {
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

  await verifyPng(socialPreview, 1280, 640, false);
  process.stdout.write(
    `PASS: ${ids.length} masters, ${ids.length * 2} runtime copies, ` +
      "and the social preview are valid.\n"
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
