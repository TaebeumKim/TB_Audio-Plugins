const fs = require("fs");
const path = require("path");
const sharp = require("sharp");

const repoRoot = path.resolve(__dirname, "..", "..");
const fishPath = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-fish.svg"
);
const masterDir = path.join(repoRoot, "assets", "brand", "plugin-icons");
const hubDir = path.join(repoRoot, "Hub", "Logos");
const fallbackDir = path.join(repoRoot, "assets", "plugins");
const socialDir = path.join(repoRoot, "assets", "social");
const qaDir = path.resolve(repoRoot, "..", "output", "github-logo-qa");

const products = [
  {
    id: "tb_center",
    name: "TB CENTER",
    primary: "#9FB3C8",
    secondary: "#5D7690",
    metaphor: "center",
  },
  {
    id: "tb_compressor",
    name: "TB COMPRESSOR",
    primary: "#E18476",
    secondary: "#72B9D4",
    metaphor: "compressor",
  },
  {
    id: "tb_distortion",
    name: "TB DISTORTION",
    primary: "#D6E04A",
    secondary: "#7086FF",
    metaphor: "distortion",
  },
  {
    id: "tb_disperser",
    name: "TB DISPERSER",
    primary: "#D83A45",
    secondary: "#8FA6B8",
    metaphor: "disperser",
  },
  {
    id: "tb_eq",
    name: "TB EQ",
    primary: "#E0A05C",
    secondary: "#72D99C",
    tertiary: "#61C9E8",
    metaphor: "eq",
  },
  {
    id: "tb_colorizer",
    name: "TB COLORIZER",
    primary: "#D7A6FF",
    secondary: "#FF9BCB",
    tertiary: "#78C7FF",
    metaphor: "colorizer",
  },
  {
    id: "tb_inverted_flanger",
    name: "TB INVERTED FLANGER",
    primary: "#D8924F",
    secondary: "#7FCBB0",
    metaphor: "invertedFlanger",
  },
  {
    id: "tb_inverted_phaser",
    name: "TB INVERTED PHASER",
    primary: "#E07D73",
    secondary: "#C0DDCA",
    metaphor: "invertedPhaser",
  },
  {
    id: "tb_jewel_digger",
    name: "TB JEWEL DIGGER & FINDER",
    primary: "#79C9C3",
    secondary: "#D99063",
    metaphor: "jewelDigger",
  },
  {
    id: "tb_noise_remover",
    name: "TB NOISE REMOVER",
    primary: "#54D3FF",
    secondary: "#FF6F61",
    metaphor: "noiseRemover",
  },
  {
    id: "tb_parallel_reverb",
    name: "TB PARALLEL REVERB",
    primary: "#62B8A8",
    secondary: "#D5A15E",
    tertiary: "#8D7BC2",
    metaphor: "parallelReverb",
  },
  {
    id: "tb_scrambler",
    name: "TB SCRAMBLER",
    primary: "#F2B632",
    secondary: "#87929C",
    metaphor: "scrambler",
  },
  {
    id: "tb_transient_shaper",
    name: "TB SPECTRAL TRANSIENT SHAPER",
    primary: "#E7A05F",
    secondary: "#8FC9AD",
    tertiary: "#E07D73",
    metaphor: "transientShaper",
  },
  {
    id: "tb_step_shifter",
    name: "TB STEP SHIFTER",
    primary: "#E07D73",
    secondary: "#C0DDCA",
    metaphor: "stepShifter",
  },
  {
    id: "tb_tune",
    name: "TB TUNE",
    primary: "#62D6E6",
    secondary: "#F0C34E",
    tertiary: "#B88BE6",
    metaphor: "tune",
  },
  {
    id: "tb_volume",
    name: "TB VOLUME",
    primary: "#7FCBB0",
    secondary: "#E0A05C",
    metaphor: "volume",
  },
  {
    id: "tb_vocoder",
    name: "TB VOCODER",
    primary: "#F0005A",
    secondary: "#0047FF",
    metaphor: "vocoder",
  },
  {
    id: "tb_xyz_panner",
    name: "TB XYZ PANNER",
    primary: "#ED9251",
    secondary: "#78C4BD",
    metaphor: "xyzPanner",
  },
  {
    id: "tb_limiter",
    name: "TB LIMITER",
    primary: "#F1E8D2",
    secondary: "#91A2B5",
    tertiary: "#E36D86",
    metaphor: "limiter",
  },
  {
    id: "tb_sublow",
    name: "TB SUBLOW",
    primary: "#D6B86C",
    secondary: "#A36A8B",
    metaphor: "subLow",
  },
];

function escapeXml(value) {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}

function hexToRgb(hex) {
  const value = hex.replace("#", "");
  return [
    parseInt(value.slice(0, 2), 16) / 255,
    parseInt(value.slice(2, 4), 16) / 255,
    parseInt(value.slice(4, 6), 16) / 255,
  ];
}

function tintFilter(id, colour) {
  const [r, g, b] = hexToRgb(colour);
  return `<filter id="${id}" color-interpolation-filters="sRGB">
    <feColorMatrix type="matrix" values="
      0 0 0 0 ${r}
      0 0 0 0 ${g}
      0 0 0 0 ${b}
      0 0 0 1 0"/>
  </filter>`;
}

function fishImage(
  fishUri,
  x,
  y,
  width,
  height,
  filter,
  opacity = 1,
  transform = "",
  preserveAspectRatio = "xMidYMid meet"
) {
  const transformAttribute = transform ? ` transform="${transform}"` : "";
  return `<image href="${fishUri}" x="${x}" y="${y}" width="${width}" height="${height}"
    preserveAspectRatio="${preserveAspectRatio}" opacity="${opacity}" filter="url(#${filter})"${transformAttribute}/>`;
}

function wavePath(y, amplitude, cycles, colour, opacity = 0.9, width = 10) {
  const x0 = 170;
  const x1 = 854;
  const step = (x1 - x0) / cycles;
  let d = `M ${x0} ${y}`;
  for (let index = 0; index < cycles; index += 1) {
    const start = x0 + index * step;
    d += ` C ${start + step * 0.25} ${y - amplitude}, ${start + step * 0.75} ${y + amplitude}, ${start + step} ${y}`;
  }
  return `<path d="${d}" fill="none" stroke="${colour}" stroke-opacity="${opacity}"
    stroke-width="${width}" stroke-linecap="round"/>`;
}

function metaphorSvg(product, fishUri, pixelFishUri, coarseFishUri) {
  const p = product.primary;
  const s = product.secondary;
  const t = product.tertiary || "#F4F7FA";
  const fish = (x, y, width, height, filter = "primary", opacity = 1, transform = "") =>
    fishImage(fishUri, x, y, width, height, filter, opacity, transform);
  const stretchedFish = (x, y, width, height, filter = "primary", opacity = 1) =>
    fishImage(fishUri, x, y, width, height, filter, opacity, "", "none");

  switch (product.metaphor) {
    case "center":
      return `
        ${fish(92, 350, 300, 300, "secondary", 0.2)}
        ${fish(362, 322, 300, 300, "white", 0.98)}
        ${fish(632, 350, 300, 300, "secondary", 0.2)}
      `;
    case "compressor":
      return `
        <path d="M170 266 H854" stroke="${s}" stroke-opacity=".28" stroke-width="11"
          stroke-dasharray="28 26" stroke-linecap="round"/>
        <path d="M170 754 H854" stroke="${s}" stroke-opacity=".28" stroke-width="11"
          stroke-dasharray="28 26" stroke-linecap="round"/>
        <path d="M142 510 H882" stroke="${p}" stroke-width="15" stroke-linecap="round"/>
        ${stretchedFish(232, 334, 560, 376, "white", 0.98)}
      `;
    case "distortion":
      return `
        <image href="${pixelFishUri}" x="210" y="210" width="604" height="604"
          preserveAspectRatio="xMidYMid meet" image-rendering="pixelated"/>
        <g fill="${s}" opacity=".9">
          <rect x="282" y="334" width="42" height="42"/><rect x="664" y="386" width="38" height="38"/>
          <rect x="452" y="586" width="44" height="44"/><rect x="584" y="270" width="34" height="34"/>
        </g>
      `;
    case "disperser":
      return `
        ${fish(252, 304, 500, 500, "secondary", 0.28)}
        ${fish(300, 276, 500, 500, "white", 0.98)}
        ${wavePath(474, 22, 5, p, 0.88, 10)}
        ${wavePath(548, 15, 6, s, 0.72, 8)}
      `;
    case "eq":
      return `
        ${fish(92, 366, 260, 260, "primary", 0.98)}
        ${fish(382, 366, 260, 260, "primary", 0.98)}
        ${fish(672, 366, 260, 260, "primary", 0.98)}
      `;
    case "colorizer":
      return `
        <defs>
          <linearGradient id="rainbowFish" x1="0" y1="0" x2="1" y2="1">
            <stop offset="0" stop-color="#FF4F64"/>
            <stop offset=".23" stop-color="#FFF176"/>
            <stop offset=".45" stop-color="#4BE38A"/>
            <stop offset=".68" stop-color="#78C7FF"/>
            <stop offset="1" stop-color="#B44CFF"/>
          </linearGradient>
          <mask id="rainbowMask" maskUnits="userSpaceOnUse" x="205" y="205" width="614" height="614">
            <image href="${fishUri}" x="205" y="205" width="614" height="614"/>
          </mask>
        </defs>
        <rect x="205" y="205" width="614" height="614" fill="url(#rainbowFish)" mask="url(#rainbowMask)"/>
      `;
    case "invertedFlanger":
      return `
        ${fish(262, 252, 500, 500, "secondary", 0.22, "translate(-34 18)")}
        ${fish(262, 252, 500, 500, "white", 0.96, "translate(0 1024) scale(1 -1)")}
        ${wavePath(335, 22, 5, p, 0.92, 10)}
        ${wavePath(690, 18, 6, s, 0.85, 9)}
      `;
    case "invertedPhaser":
      return `
        <defs>
          <clipPath id="twirlOuter"><ellipse cx="512" cy="512" rx="252" ry="218"/></clipPath>
          <clipPath id="twirlMid"><ellipse cx="512" cy="512" rx="170" ry="145"/></clipPath>
          <clipPath id="twirlInner"><ellipse cx="512" cy="512" rx="92" ry="78"/></clipPath>
        </defs>
        ${fish(252, 252, 520, 520, "secondary", 0.38, "translate(0 1024) scale(1 -1)")}
        <g clip-path="url(#twirlOuter)">${fish(252, 252, 520, 520, "primary", 0.48, "rotate(8 512 512) translate(0 1024) scale(1 -1)")}</g>
        <g clip-path="url(#twirlMid)">${fish(252, 252, 520, 520, "white", 0.7, "rotate(23 512 512) translate(0 1024) scale(1 -1)")}</g>
        <g clip-path="url(#twirlInner)">${fish(252, 252, 520, 520, "primary", 0.95, "rotate(42 512 512) translate(0 1024) scale(1 -1)")}</g>
        <path d="M760 512 C760 320 420 290 354 476 C304 618 588 700 650 550
          C692 448 486 400 442 498 C412 570 548 606 582 530"
          fill="none" stroke="${p}" stroke-width="13" stroke-linecap="round" opacity=".95"/>
      `;
    case "jewelDigger":
      return `
        ${fish(292, 292, 460, 460, "white", 0.96)}
        <path d="M690 276 L356 758" stroke="#15191F" stroke-width="38" stroke-linecap="round"/>
        <path d="M690 276 L356 758" stroke="${s}" stroke-width="22" stroke-linecap="round"/>
        <path d="M484 260 Q680 118 864 286" fill="none" stroke="#15191F" stroke-width="42" stroke-linecap="round"/>
        <path d="M484 260 Q680 118 864 286" fill="none" stroke="${p}" stroke-width="23" stroke-linecap="round"/>
        <circle cx="682" cy="280" r="25" fill="${s}" stroke="#15191F" stroke-width="12"/>
      `;
    case "noiseRemover":
      return `
        ${fish(276, 292, 472, 472, "white", 0.98)}
        <path d="M294 548 C294 226 730 226 730 548" fill="none" stroke="#0A0D11"
          stroke-width="48" stroke-linecap="round"/>
        <path d="M294 548 C294 226 730 226 730 548" fill="none" stroke="${p}"
          stroke-width="24" stroke-linecap="round"/>
        <rect x="248" y="522" width="102" height="222" rx="32" fill="#0A0D11"/>
        <rect x="264" y="538" width="70" height="190" rx="22" fill="${p}"/>
        <rect x="674" y="522" width="102" height="222" rx="32" fill="#0A0D11"/>
        <rect x="690" y="538" width="70" height="190" rx="22" fill="${p}"/>
      `;
    case "parallelReverb":
      return `
        ${fish(82, 382, 260, 260, "primary", 0.2, "translate(22 10)")}
        ${fish(82, 382, 260, 260, "primary", 0.98)}
        ${fish(382, 382, 260, 260, "secondary", 0.2, "translate(22 10)")}
        ${fish(382, 382, 260, 260, "secondary", 0.98)}
        ${fish(682, 382, 260, 260, "tertiary", 0.2, "translate(22 10)")}
        ${fish(682, 382, 260, 260, "tertiary", 0.98)}
      `;
    case "scrambler":
      return `
        ${fish(218, 218, 588, 588, "white", 0.16)}
        <image href="${coarseFishUri}" x="198" y="198" width="628" height="628"
          preserveAspectRatio="xMidYMid meet" image-rendering="pixelated" opacity=".34"/>
        <g opacity=".98">
          <rect x="244" y="244" width="126" height="126" fill="${p}"/>
          <rect x="382" y="232" width="126" height="138" fill="#202833"/>
          <rect x="520" y="252" width="132" height="118" fill="${s}"/>
          <rect x="664" y="236" width="116" height="134" fill="${p}"/>
          <rect x="232" y="382" width="138" height="126" fill="#303944"/>
          <rect x="382" y="382" width="126" height="126" fill="${s}"/>
          <rect x="520" y="382" width="132" height="126" fill="${p}"/>
          <rect x="664" y="382" width="116" height="126" fill="#161C23"/>
          <rect x="248" y="520" width="122" height="132" fill="${s}"/>
          <rect x="382" y="520" width="126" height="132" fill="${p}"/>
          <rect x="520" y="520" width="132" height="132" fill="#303944"/>
          <rect x="664" y="520" width="120" height="132" fill="${s}"/>
          <rect x="236" y="664" width="134" height="116" fill="${p}"/>
          <rect x="382" y="664" width="126" height="116" fill="#161C23"/>
          <rect x="520" y="664" width="132" height="116" fill="${s}"/>
          <rect x="664" y="664" width="116" height="116" fill="${p}"/>
        </g>
      `;
    case "transientShaper":
      return `
        ${fish(212, 294, 520, 520, "white", 0.98)}
        <path d="M600 666 C640 666 674 646 694 612
          C714 578 746 568 778 586
          C790 544 784 506 792 470
          L830 442 L874 482 L838 524
          C846 616 822 700 748 724
          C682 744 626 716 600 666 Z"
          fill="${p}" stroke="#0A0D11" stroke-width="26" stroke-linejoin="round"/>
        <path d="M688 612 C706 558 770 542 804 586" fill="none" stroke="${t}"
          stroke-width="16" stroke-linecap="round"/>
        <rect x="786" y="412" width="86" height="70" rx="22" fill="${p}"
          stroke="#0A0D11" stroke-width="24" transform="rotate(30 829 447)"/>
        <path d="M804 426 L860 458 M794 444 L850 476" stroke="${t}"
          stroke-width="10" stroke-linecap="round"/>
      `;
    case "stepShifter":
      return `
        <path d="M142 812 H332 V684 H478 V556 H624 V428 H882"
          fill="none" stroke="#0A0D11" stroke-width="54" stroke-linejoin="round"/>
        <path d="M142 812 H332 V684 H478 V556 H624 V428 H882"
          fill="none" stroke="${s}" stroke-width="28" stroke-linejoin="round"/>
        ${fish(506, 140, 392, 392, "primary", 0.98)}
      `;
    case "tune":
      return `
        ${fish(522, 334, 372, 372, "white", 0.98)}
        <rect x="208" y="254" width="116" height="168" rx="54" fill="#0A0D11"/>
        <rect x="226" y="270" width="80" height="136" rx="38" fill="${p}"/>
        <path d="M268 410 L360 642" stroke="#0A0D11" stroke-width="52" stroke-linecap="round"/>
        <path d="M268 410 L360 642" stroke="${t}" stroke-width="28" stroke-linecap="round"/>
        <path d="M304 516 C350 488 394 512 402 556 C410 604 354 632 318 602 Z"
          fill="${s}" stroke="#0A0D11" stroke-width="16"/>
        <path d="M448 316 V500 C448 536 408 558 378 538 C350 520 360 478 396 470
          C410 466 424 468 436 474 V344 L532 318 V438"
          fill="none" stroke="${s}" stroke-width="22" stroke-linecap="round" stroke-linejoin="round"/>
      `;
    case "volume":
      return `
        ${fish(434, 312, 420, 420, "white", 0.98)}
        <rect x="204" y="202" width="38" height="620" rx="19" fill="#0A0D11" stroke="${s}" stroke-width="12"/>
        <g stroke="${s}" stroke-width="9" opacity=".82">
          <path d="M160 238 H194"/><path d="M160 354 H194"/><path d="M160 470 H194"/>
          <path d="M160 586 H194"/><path d="M160 702 H194"/><path d="M160 806 H194"/>
        </g>
        <rect x="148" y="454" width="150" height="88" rx="26" fill="#0A0D11" stroke="${p}" stroke-width="16"/>
        <path d="M182 498 H264" stroke="${p}" stroke-width="10" stroke-linecap="round"/>
      `;
    case "vocoder":
      return `
        ${fish(252, 272, 520, 520, "white", 0.98)}
        <rect x="312" y="410" width="190" height="142" rx="58" fill="#07112A" stroke="${s}" stroke-width="22"/>
        <rect x="516" y="410" width="190" height="142" rx="58" fill="#07112A" stroke="${s}" stroke-width="22"/>
        <path d="M502 458 C510 442 520 442 528 458" fill="none" stroke="${s}" stroke-width="20"/>
        <path d="M306 446 L252 422 M712 446 L770 414" stroke="${s}" stroke-width="18" stroke-linecap="round"/>
        <path d="M346 448 H468 M550 448 H672" stroke="#2C62FF" stroke-width="16" opacity=".72"/>
      `;
    case "xyzPanner":
      return `
        ${fish(292, 292, 500, 500, "secondary", 0.28, "translate(42 36)")}
        ${fish(248, 248, 500, 500, "primary", 0.98)}
        <path d="M316 362 L514 268 L692 356 L516 450 Z" fill="${s}" opacity=".34"/>
        <path d="M516 450 L692 356 L700 604 L526 746 Z" fill="#213D42" opacity=".58"/>
        <path d="M316 362 L516 450 L526 746 L330 618 Z" fill="${p}" opacity=".2"/>
        <path d="M340 330 L518 266 L612 316" fill="none" stroke="#FFFFFF"
          stroke-opacity=".68" stroke-width="16" stroke-linecap="round"/>
      `;
    case "limiter":
      return `
        <path d="M166 274 H858" stroke="${t}" stroke-width="18" stroke-linecap="round"/>
        ${stretchedFish(232, 384, 560, 244, "white", 0.98)}
        <g stroke="#0A0D11" stroke-width="38" stroke-linecap="round">
          <path d="M344 442 L408 514 M408 442 L344 514"/>
          <path d="M438 442 L502 514 M502 442 L438 514"/>
        </g>
        <g stroke="${t}" stroke-width="18" stroke-linecap="round">
          <path d="M344 442 L408 514 M408 442 L344 514"/>
          <path d="M438 442 L502 514 M502 442 L438 514"/>
        </g>
      `;
    case "subLow":
      return `
        ${fish(486, 270, 368, 368, "white", 0.96)}
        <path d="M334 684 L706 350" stroke="#0A0D11" stroke-width="76" stroke-linecap="round"/>
        <path d="M334 684 L706 350" stroke="${p}" stroke-width="48" stroke-linecap="round"/>
        <path d="M142 760 C126 688 176 632 244 648
          C258 590 316 566 366 600
          L412 636 C438 656 436 688 408 706
          L370 730 C342 750 332 784 348 824
          C278 860 168 844 142 760 Z"
          fill="${p}" stroke="#0A0D11" stroke-width="26"/>
        <path d="M214 744 C214 694 258 662 306 678
          C348 692 360 746 328 778 C292 814 222 792 214 744 Z"
          fill="${s}" stroke="#0A0D11" stroke-width="18"/>
        <path d="M666 382 L770 266 L838 314 L730 426 Z" fill="${p}"
          stroke="#0A0D11" stroke-width="24"/>
        <path d="M240 730 L798 286 M250 742 L808 298 M260 754 L818 310 M270 766 L828 322"
          stroke="#F7E6A2" stroke-width="6" opacity=".9"/>
        <circle cx="766" cy="274" r="16" fill="${s}"/><circle cx="800" cy="290" r="16" fill="${s}"/>
        <circle cx="826" cy="316" r="16" fill="${s}"/><circle cx="790" cy="334" r="16" fill="${s}"/>
      `;
    default:
      throw new Error(`Unknown metaphor: ${product.metaphor}`);
  }
}

function iconSvg(product, fishUri, pixelFishUri, coarseFishUri) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
    <defs>
      <linearGradient id="outerBg" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#303740"/>
        <stop offset=".45" stop-color="#151A20"/>
        <stop offset="1" stop-color="#080B0F"/>
      </linearGradient>
      <radialGradient id="accentGlow" cx=".5" cy=".42" r=".7">
        <stop offset="0" stop-color="${product.primary}" stop-opacity=".16"/>
        <stop offset=".55" stop-color="${product.primary}" stop-opacity=".035"/>
        <stop offset="1" stop-color="${product.primary}" stop-opacity="0"/>
      </radialGradient>
      <filter id="softShadow" x="-20%" y="-20%" width="140%" height="140%">
        <feGaussianBlur in="SourceAlpha" stdDeviation="18"/>
        <feOffset dy="20"/>
        <feColorMatrix values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 .72 0"/>
        <feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>
      </filter>
      ${tintFilter("white", "#F4F7FA")}
      ${tintFilter("primary", product.primary)}
      ${tintFilter("secondary", product.secondary)}
      ${tintFilter("tertiary", product.tertiary || "#F4F7FA")}
    </defs>
    <rect x="30" y="30" width="964" height="964" rx="252" fill="url(#outerBg)"
      stroke="#4A535E" stroke-width="22" filter="url(#softShadow)"/>
    <rect x="88" y="88" width="848" height="848" rx="208" fill="#10151B"
      stroke="#303943" stroke-width="12"/>
    <rect x="88" y="88" width="848" height="848" rx="208" fill="url(#accentGlow)"/>
    <path d="M184 156 H840" stroke="#FFFFFF" stroke-opacity=".06" stroke-width="10" stroke-linecap="round"/>
    ${metaphorSvg(product, fishUri, pixelFishUri, coarseFishUri)}
  </svg>`;
}

function labelSvg(product, size = 240, fontSize = 12) {
  return Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="38">
    <rect width="100%" height="100%" fill="#0B0F14"/>
    <text x="${size / 2}" y="25" fill="#D9E0E7" font-family="Arial,Segoe UI,sans-serif"
      font-size="${fontSize}" font-weight="700" text-anchor="middle">${escapeXml(product.name)}</text>
  </svg>`);
}

async function makePixelFish(fishPathValue, colour, coarseSize) {
  return sharp(fishPathValue)
    .resize(coarseSize, coarseSize, { fit: "contain", kernel: "nearest" })
    .tint(colour)
    .resize(640, 640, { kernel: "nearest" })
    .png({ compressionLevel: 9, palette: true })
    .toBuffer();
}

async function renderProduct(product, fishUri) {
  const pixelFish = await makePixelFish(fishPath, product.primary, 22);
  const coarseFish = await makePixelFish(fishPath, product.primary, 10);
  const pixelFishUri = `data:image/png;base64,${pixelFish.toString("base64")}`;
  const coarseFishUri = `data:image/png;base64,${coarseFish.toString("base64")}`;
  const svg = Buffer.from(iconSvg(product, fishUri, pixelFishUri, coarseFishUri));
  const masterPath = path.join(masterDir, `${product.id}.png`);
  const hubPath = path.join(hubDir, `${product.id}.png`);
  const fallbackPath = path.join(fallbackDir, `${product.id}.png`);

  await sharp(svg)
    .png({ compressionLevel: 9, adaptiveFiltering: true })
    .toFile(masterPath);

  const runtimeIcon = await sharp(masterPath)
    .resize(256, 256, { kernel: sharp.kernel.lanczos3 })
    .png({ compressionLevel: 9, adaptiveFiltering: true, palette: true, quality: 100 })
    .toBuffer();

  fs.writeFileSync(hubPath, runtimeIcon);
  fs.writeFileSync(fallbackPath, runtimeIcon);
}

async function buildContactSheet(iconSize, filename) {
  const columns = 5;
  const rows = 4;
  const cellWidth = iconSize >= 180 ? 240 : 190;
  const cellHeight = iconSize >= 180 ? 278 : 132;
  const width = columns * cellWidth;
  const height = rows * cellHeight;
  const composites = [];

  for (let index = 0; index < products.length; index += 1) {
    const product = products[index];
    const column = index % columns;
    const row = Math.floor(index / columns);
    const left = column * cellWidth + Math.floor((cellWidth - iconSize) / 2);
    const top = row * cellHeight + 10;
    const icon = await sharp(path.join(masterDir, `${product.id}.png`))
      .resize(iconSize, iconSize)
      .png()
      .toBuffer();
    composites.push({ input: icon, left, top });
    composites.push({
      input: labelSvg(product, cellWidth, iconSize >= 180 ? 12 : 10),
      left: column * cellWidth,
      top: row * cellHeight + iconSize + 16,
    });
  }

  await sharp({
    create: {
      width,
      height,
      channels: 4,
      background: "#0B0F14",
    },
  })
    .composite(composites)
    .png({ compressionLevel: 9 })
    .toFile(path.join(qaDir, filename));
}

async function buildSocialPreview() {
  const width = 1280;
  const height = 640;
  const backdrop = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}">
    <defs>
      <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#252C34"/>
        <stop offset=".5" stop-color="#11161C"/>
        <stop offset="1" stop-color="#070A0E"/>
      </linearGradient>
      <radialGradient id="glow" cx=".22" cy=".36" r=".7">
        <stop offset="0" stop-color="#E07D73" stop-opacity=".2"/>
        <stop offset=".55" stop-color="#78C4BD" stop-opacity=".08"/>
        <stop offset="1" stop-color="#78C4BD" stop-opacity="0"/>
      </radialGradient>
    </defs>
    <rect width="${width}" height="${height}" fill="url(#bg)"/>
    <rect width="${width}" height="${height}" fill="url(#glow)"/>
    <rect x="28" y="28" width="1224" height="584" rx="34" fill="none" stroke="#46505B" stroke-width="2"/>
    <text x="74" y="210" fill="#F4F7FA" font-family="Arial,Segoe UI,sans-serif"
      font-size="72" font-weight="800" letter-spacing="2">TB AUDIO</text>
    <text x="74" y="286" fill="#F4F7FA" font-family="Arial,Segoe UI,sans-serif"
      font-size="72" font-weight="800" letter-spacing="2">PLUG-INS</text>
    <text x="78" y="346" fill="#AEB8C3" font-family="Arial,Segoe UI,sans-serif"
      font-size="25" font-weight="700" letter-spacing="3">TEAM IMPULSE IMPACT</text>
    <path d="M78 386 H510" stroke="#E07D73" stroke-width="5" stroke-linecap="round"/>
    <text x="78" y="440" fill="#C9D1D9" font-family="Arial,Segoe UI,sans-serif"
      font-size="24">20 effect-specific fish marks</text>
    <text x="78" y="476" fill="#7FCBB0" font-family="Arial,Segoe UI,sans-serif"
      font-size="20">One family. Distinct sonic identities.</text>
  </svg>`);

  const composites = [{ input: backdrop, left: 0, top: 0 }];
  const startX = 628;
  const startY = 62;
  const cellX = 114;
  const cellY = 132;
  const iconSize = 104;

  for (let index = 0; index < products.length; index += 1) {
    const column = index % 5;
    const row = Math.floor(index / 5);
    const icon = await sharp(path.join(masterDir, `${products[index].id}.png`))
      .resize(iconSize, iconSize)
      .png()
      .toBuffer();
    composites.push({
      input: icon,
      left: startX + column * cellX,
      top: startY + row * cellY,
    });
  }

  await sharp({
    create: {
      width,
      height,
      channels: 4,
      background: "#0B0F14",
    },
  })
    .composite(composites)
    .png({ compressionLevel: 9, adaptiveFiltering: true })
    .toFile(path.join(socialDir, "tb_audio_plugins_social_preview.png"));
}

async function main() {
  for (const directory of [masterDir, hubDir, fallbackDir, socialDir, qaDir]) {
    fs.mkdirSync(directory, { recursive: true });
  }

  const fishData = fs.readFileSync(fishPath);
  const fishUri = `data:image/svg+xml;base64,${fishData.toString("base64")}`;

  for (const product of products) {
    await renderProduct(product, fishUri);
  }

  await buildSocialPreview();
  await buildContactSheet(200, "plugin-icons-200px-contact-sheet.png");
  await buildContactSheet(64, "plugin-icons-64px-contact-sheet.png");

  process.stdout.write(
    `Generated ${products.length} masters, ${products.length * 2} runtime copies, ` +
      `1 social preview, and 2 QA sheets.\n`
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
