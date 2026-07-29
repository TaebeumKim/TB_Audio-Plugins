const fs = require("fs");
const path = require("path");
const sharp = require("sharp");

const repoRoot = path.resolve(__dirname, "..", "..");
const sourcePath = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark-reference.png"
);
const outputPath = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark.svg"
);
const iso = 128;

function edgePoint(edge, x, y, values) {
  const [topLeft, topRight, bottomRight, bottomLeft] = values;
  const interpolate = (a, b) => {
    if (a === b) return 0.5;
    return Math.max(0, Math.min(1, (iso - a) / (b - a)));
  };

  if (edge === "top") return [x + interpolate(topLeft, topRight), y];
  if (edge === "right") return [x + 1, y + interpolate(topRight, bottomRight)];
  if (edge === "bottom") return [x + interpolate(bottomLeft, bottomRight), y + 1];
  return [x, y + interpolate(topLeft, bottomLeft)];
}

function segmentEdges(index, centerIsInside) {
  const fixed = {
    1: [["left", "top"]],
    2: [["top", "right"]],
    3: [["left", "right"]],
    4: [["right", "bottom"]],
    6: [["top", "bottom"]],
    7: [["left", "bottom"]],
    8: [["bottom", "left"]],
    9: [["top", "bottom"]],
    11: [["right", "bottom"]],
    12: [["left", "right"]],
    13: [["top", "right"]],
    14: [["left", "top"]],
  };
  if (index === 5) {
    return centerIsInside
      ? [["top", "right"], ["bottom", "left"]]
      : [["left", "top"], ["right", "bottom"]];
  }
  if (index === 10) {
    return centerIsInside
      ? [["left", "top"], ["right", "bottom"]]
      : [["top", "right"], ["bottom", "left"]];
  }
  return fixed[index] || [];
}

function pointKey([x, y]) {
  return `${Math.round(x * 10000)},${Math.round(y * 10000)}`;
}

function traceLoops(pixels, width, height) {
  const segments = [];
  const sample = (x, y) =>
    x < 0 || y < 0 || x >= width || y >= height ? 0 : pixels[y * width + x];

  for (let y = -1; y < height; y += 1) {
    for (let x = -1; x < width; x += 1) {
      const values = [
        sample(x, y),
        sample(x + 1, y),
        sample(x + 1, y + 1),
        sample(x, y + 1),
      ];
      const index =
        (values[0] >= iso ? 1 : 0) |
        (values[1] >= iso ? 2 : 0) |
        (values[2] >= iso ? 4 : 0) |
        (values[3] >= iso ? 8 : 0);
      if (index === 0 || index === 15) continue;

      const centerIsInside =
        (values[0] + values[1] + values[2] + values[3]) / 4 >= iso;
      for (const [firstEdge, secondEdge] of segmentEdges(index, centerIsInside)) {
        const first = edgePoint(firstEdge, x + 0.5, y + 0.5, values);
        const second = edgePoint(secondEdge, x + 0.5, y + 0.5, values);
        segments.push([first, second]);
      }
    }
  }

  const adjacency = new Map();
  segments.forEach((segment, segmentIndex) => {
    segment.forEach((point, endpointIndex) => {
      const key = pointKey(point);
      const entries = adjacency.get(key) || [];
      entries.push({ segmentIndex, endpointIndex });
      adjacency.set(key, entries);
    });
  });

  const visited = new Set();
  const loops = [];
  for (let startIndex = 0; startIndex < segments.length; startIndex += 1) {
    if (visited.has(startIndex)) continue;
    const loop = [];
    let segmentIndex = startIndex;
    let endpointIndex = 0;
    const startKey = pointKey(segments[segmentIndex][endpointIndex]);

    while (!visited.has(segmentIndex)) {
      visited.add(segmentIndex);
      const segment = segments[segmentIndex];
      const point = segment[endpointIndex];
      const nextPoint = segment[1 - endpointIndex];
      loop.push(point);
      const nextKey = pointKey(nextPoint);
      if (nextKey === startKey) break;
      const candidates = adjacency.get(nextKey) || [];
      const next = candidates.find((candidate) => !visited.has(candidate.segmentIndex));
      if (!next) {
        throw new Error(`Open contour at ${nextKey}`);
      }
      segmentIndex = next.segmentIndex;
      endpointIndex = next.endpointIndex;
    }

    if (loop.length >= 3) loops.push(loop);
  }
  return loops;
}

function formatNumber(value) {
  return Number(value.toFixed(2)).toString();
}

function loopPath(loop) {
  const [first, ...rest] = loop;
  return `M${formatNumber(first[0])} ${formatNumber(first[1])}${rest
    .map(([x, y]) => `L${formatNumber(x)} ${formatNumber(y)}`)
    .join("")}Z`;
}

async function main() {
  const { data, info } = await sharp(sourcePath)
    .removeAlpha()
    .greyscale()
    .raw()
    .toBuffer({ resolveWithObject: true });
  if (info.width !== 500 || info.height !== 500) {
    throw new Error(`Reference must be 500x500, got ${info.width}x${info.height}`);
  }

  const loops = traceLoops(data, info.width, info.height);
  const pathData = loops.map(loopPath).join("");
  const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="500" height="500" viewBox="0 0 500 500">
  <path d="${pathData}" fill="#FFFFFF" fill-rule="evenodd"/>
</svg>
`;
  fs.writeFileSync(outputPath, svg);

  const referenceMask = await sharp(sourcePath).greyscale().raw().toBuffer();
  const vectorMask = await sharp(Buffer.from(svg))
    .resize(500, 500)
    .flatten({ background: "#000000" })
    .greyscale()
    .raw()
    .toBuffer();
  let intersection = 0;
  let union = 0;
  for (let index = 0; index < referenceMask.length; index += 1) {
    const referenceIsLight = referenceMask[index] > 127;
    const vectorIsLight = vectorMask[index] > 127;
    if (referenceIsLight && vectorIsLight) intersection += 1;
    if (referenceIsLight || vectorIsLight) union += 1;
  }
  const iou = intersection / union;
  console.log(
    `Traced ${loops.length} contours from the approved 500x500 mark (IoU ${iou.toFixed(4)}).`
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
