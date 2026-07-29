const fs = require("fs");
const path = require("path");
const sharp = require("sharp");

const repoRoot = path.resolve(__dirname, "..", "..");
const masterDir = path.join(repoRoot, "assets", "brand", "plugin-icons");
const hubDir = path.join(repoRoot, "Hub", "Logos");
const fallbackDir = path.join(repoRoot, "assets", "plugins");
const socialDir = path.join(repoRoot, "assets", "social");
const qaDir = path.resolve(repoRoot, "..", "output", "github-logo-qa");
const referenceMarkPath = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark-reference.png"
);
const canonicalMarkPath = path.join(
  repoRoot,
  "assets",
  "brand",
  "source",
  "team-impulse-mark.svg"
);
const canonicalMarkMarkup = fs
  .readFileSync(canonicalMarkPath, "utf8")
  .match(/<svg[^>]*>([\s\S]*?)<\/svg>/i)[1]
  .trim();

const WHITE = "#FFFFFF";
const BLACK = "#000000";
const BODY =
  "M113 228 C149 188 194 166 220 166 C253 160 283 168 305 181 " +
  "C321 190 329 210 330 267 C250 291 170 284 113 244 L129 238 Z";
const TAIL_OUTER =
  "M236 166 C249 121 278 104 306 107 C349 111 367 151 376 205 " +
  "C384 259 395 322 404 354 C409 377 402 391 393 389 " +
  "C375 386 360 352 351 317 L326 218 C321 197 310 183 292 176";
const TAIL_INNER =
  "M250 168 C263 134 283 118 306 120 C339 123 352 158 360 211 " +
  "C368 263 380 325 389 355 C392 368 389 375 385 373 " +
  "C374 370 363 340 356 314 L333 215 C327 192 312 178 293 173";
const JOIN_OUTER =
  "M274 272 C269 248 265 218 274 201 C283 183 299 182 312 194 " +
  "C325 207 329 239 331 265";
const JOIN_INNER =
  "M290 277 C286 252 282 227 288 211 C294 197 305 195 314 205 " +
  "C323 216 326 241 327 260";

function transformAround(cx = 250, cy = 250, sx = 1, sy = sx, rotation = 0) {
  return `translate(${cx} ${cy}) rotate(${rotation}) scale(${sx} ${sy}) translate(-250 -250)`;
}

function exactMark({ transform = "", fill = WHITE, opacity = 1 } = {}) {
  const transformAttribute = transform ? ` transform="${transform}"` : "";
  const markup = canonicalMarkMarkup.replace('fill="#FFFFFF"', `fill="${fill}"`);
  return `<g${transformAttribute} opacity="${opacity}">${markup}</g>`;
}

function eyeMarkup(mode = "dots", fill = WHITE) {
  if (mode === "none") return "";
  if (mode === "x") {
    return `<g fill="none" stroke="${fill}" stroke-width="7" stroke-linecap="round">
      <path d="M151 204 L165 218 M165 204 L151 218"/>
      <path d="M180 204 L200 226 M200 204 L180 226"/>
    </g>`;
  }
  if (mode === "squares") {
    return `<g fill="${fill}">
      <rect x="149" y="202" width="18" height="18"/>
      <rect x="178" y="202" width="25" height="25"/>
    </g>`;
  }
  return `<g fill="${fill}">
    <circle cx="158" cy="211" r="8.5"/>
    <circle cx="190" cy="215" r="12.5"/>
  </g>`;
}

function baseMark({
  transform = "",
  stroke = WHITE,
  opacity = 1,
  strokeWidth = 5,
  eyes = "dots",
  maskBody = true,
  details = true,
} = {}) {
  const transformAttribute = transform ? ` transform="${transform}"` : "";
  const mask = maskBody ? `<path d="${BODY}" fill="${BLACK}"/>` : "";
  const detailPaths = details
    ? `<path d="${TAIL_INNER}"/><path d="${JOIN_OUTER}"/><path d="${JOIN_INNER}"/>`
    : "";
  return `<g${transformAttribute} opacity="${opacity}">
    <g fill="none" stroke="${stroke}" stroke-width="${strokeWidth}" stroke-linecap="round" stroke-linejoin="round">
      <path d="${TAIL_OUTER}"/>
      ${detailPaths}
    </g>
    ${mask}
    <path d="${BODY}" fill="none" stroke="${stroke}" stroke-width="${strokeWidth}" stroke-linecap="round" stroke-linejoin="round"/>
    ${eyeMarkup(eyes, stroke)}
  </g>`;
}

function centerMark() {
  return [
    exactMark({
      transform: transformAround(180, 250, 0.72),
      opacity: 0.2,
    }),
    exactMark({
      transform: transformAround(320, 250, 0.72),
      opacity: 0.2,
    }),
    exactMark({ transform: transformAround(250, 250, 0.86) }),
  ].join("");
}

function compressorMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-linecap="round">
      <path d="M105 154 H395" stroke-width="6" stroke-dasharray="14 12" opacity="0.4"/>
      <path d="M105 346 H395" stroke-width="6" stroke-dasharray="14 12" opacity="0.4"/>
      <path d="M105 239 H395" stroke-width="6"/>
    </g>
    ${exactMark({ transform: transformAround(250, 250, 1, 0.72) })}`;
}

function distortionMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="11" stroke-linecap="square" stroke-linejoin="miter">
      <path d="M110 228 H126 V214 H142 V200 H164 V188 H188 V178 H220 V170 H258 V174 H286 V184 H310 V198 H326 V216 H334 V266 H306 V276 H270 V284 H220 V282 H180 V274 H150 V262 H126 V250 H110 L128 239 Z"/>
      <path d="M236 168 V146 H248 V126 H270 V112 H306 V116 H332 V132 H348 V160 H360 V210 H370 V260 H382 V310 H394 V354 H402 V380 H390 V368 H378 V342 H366 V308 H354 V270 H344 V230 H334 V210 H324"/>
      <path d="M278 272 V244 H274 V218 H282 V202 H296 V192 H312 V204 H322 V232 H328 V262"/>
    </g>
    ${eyeMarkup("squares")}`;
}

function disperserMark() {
  const phaseBody =
    "M113 228 C149 188 194 166 220 166 C253 160 283 168 305 181 " +
    "C321 190 329 210 330 267 C307 273 290 267 270 274 " +
    "C249 281 232 274 211 281 C188 288 163 275 143 266 " +
    "C130 260 120 252 113 244 L129 238 Z";
  const phaseTail =
    "M236 166 C249 121 278 104 306 107 C349 111 367 151 376 205 " +
    "C383 248 391 280 386 313 C382 340 411 365 393 389 " +
    "C376 385 362 352 351 317 C341 286 348 254 337 224 " +
    "C331 205 315 181 292 176";
  const phaseJoin =
    "M274 272 C268 248 264 221 274 201 C282 185 298 182 312 194 " +
    "C327 208 324 235 331 265";
  return `<g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
      <path d="${phaseTail}"/>
    </g>
    <path d="${phaseBody}" fill="${BLACK}"/>
    <path d="${phaseBody}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
    <g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round"><path d="${phaseJoin}"/></g>
    ${eyeMarkup()}
    <g opacity="0.46" transform="translate(15 5)">
      ${exactMark()}
    </g>`;
}

function eqMark() {
  return [105, 250, 395]
    .map((x) =>
      exactMark({
        transform: transformAround(x, 250, 0.42),
      })
    )
    .join("");
}

function colorizerMark() {
  return exactMark({ fill: "url(#colorFlow)" });
}

function flangerMark() {
  const aircraft =
    "M85 216 L105 226 L85 236 L140 250 L238 246 " +
    "L320 338 L348 330 L290 241 L376 236 L414 254 L428 248 " +
    "L414 228 L428 208 L414 202 L376 220 L290 215 " +
    "L348 126 L320 118 L238 210 L140 206 Z";
  return `<path d="${aircraft}" transform="translate(0 10)" fill="none"
      stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"
      opacity="0.2"/>
    <path d="${aircraft}" fill="${BLACK}" stroke="${WHITE}" stroke-width="6"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M105 226 H392 M238 210 L290 215 M238 246 L290 241"
      fill="none" stroke="${WHITE}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
    <circle cx="158" cy="222" r="8.5" fill="${WHITE}"/>
    <circle cx="190" cy="224" r="12.5" fill="${WHITE}"/>`;
}

function phaserMark() {
  const swirlOuter =
    "M236 166 C259 116 326 106 354 150 C379 190 363 249 324 270 " +
    "C288 289 244 271 237 236 C230 207 252 182 279 182 " +
    "C302 182 318 199 316 219 C314 237 299 249 284 245 " +
    "C271 242 265 230 270 219";
  const swirlInner =
    "M250 168 C271 132 319 126 341 158 C359 186 348 228 320 247 " +
    "C294 263 262 253 255 230 C250 212 263 198 280 198 " +
    "C293 198 302 207 301 219 C300 227 294 232 287 231";
  return `<g transform="translate(0 452) scale(1 -1) rotate(-7 250 250)">
      <g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
        <path d="${swirlOuter}"/><path d="${swirlInner}"/>
      </g>
      <path d="${BODY}" fill="${BLACK}"/>
      <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="${JOIN_OUTER}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round"/>
      ${eyeMarkup()}
    </g>`;
}

function jewelMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round">
      <path d="M274 272 C280 226 300 180 342 137 L369 106"/>
      <path d="M292 277 C298 238 316 198 354 153 L382 121"/>
    </g>
    <path d="M245 108 L271 88 C306 78 340 86 369 104
      C389 92 411 94 432 105 L445 119 L441 135
      C420 123 401 123 382 136 L369 124
      C338 101 307 100 274 125 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    ${eyeMarkup()}`;
}

function noiseRemoverMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
      <path d="M236 166 C249 121 278 104 307 108 C340 111 360 133 363 160
        C366 183 354 204 331 214 C311 222 286 223 265 217"/>
      <path d="M250 168 C263 134 283 118 306 121 C331 124 347 140 348 160
        C349 177 340 190 323 199 C307 207 286 209 269 205"/>
    </g>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M259 193 C273 193 282 205 282 221 L279 244 C277 258 267 267 255 263
      C244 259 238 248 239 233 L241 214 C243 201 250 194 259 193 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M256 208 C263 207 267 214 266 223 L264 241 C263 247 259 250 255 248
      C251 246 250 240 251 233 L252 216 C252 211 254 209 256 208 Z"
      fill="none" stroke="${WHITE}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
    ${eyeMarkup()}`;
}

function parallelReverbMark() {
  const variants = [
    { x: 215, y: 242, color: "#45E3D2" },
    { x: 250, y: 250, color: "#F067B7" },
    { x: 285, y: 258, color: "#F6C85F" },
  ];
  return (
    variants
      .map(
        ({ x, y, color }) =>
          `<g filter="url(#softBlur)" opacity="0.28">${exactMark({
            transform: transformAround(x - 5, y + 5, 0.78),
            fill: color,
          })}</g>`
      )
      .join("") +
    variants
      .map(({ x, y, color }) =>
        exactMark({
          transform: transformAround(x, y, 0.78),
          fill: color,
        })
      )
      .join("")
  );
}

function scramblerMark() {
  const blocks = [
    [115, 220, 38, 18],
    [145, 192, 44, 20],
    [182, 174, 48, 20],
    [225, 164, 50, 18],
    [270, 176, 42, 20],
    [306, 194, 30, 28],
    [123, 246, 44, 20],
    [160, 265, 50, 20],
    [205, 276, 52, 18],
    [252, 270, 44, 18],
    [300, 252, 35, 20],
    [244, 126, 24, 38],
    [274, 108, 42, 20],
    [316, 122, 32, 38],
    [346, 157, 24, 52],
    [360, 210, 24, 55],
    [371, 270, 24, 58],
    [382, 332, 24, 46],
  ];
  return `<g fill="${WHITE}">${blocks
    .map(([x, y, width, height]) => `<rect x="${x}" y="${y}" width="${width}" height="${height}"/>`)
    .join("")}
    <rect x="151" y="202" width="16" height="16" fill="${BLACK}"/>
    <rect x="181" y="202" width="24" height="24" fill="${BLACK}"/>
  </g>`;
}

function transientMark() {
  const combinedBodyArm =
    "M113 228 C149 188 194 166 220 166 C253 160 277 167 295 178 " +
    "C311 145 342 116 374 128 C400 138 410 167 395 197 " +
    "C388 211 378 223 366 232 C385 227 398 204 404 178 L414 145 " +
    "L410 134 L421 120 L438 122 L449 135 L447 149 L454 160 " +
    "L445 174 L431 169 L421 198 C413 225 411 251 394 272 " +
    "C375 295 351 299 329 282 L318 248 " +
    "C276 285 184 286 113 244 L129 238 Z";
  const armInner =
    "M333 174 C348 153 373 154 385 174 " +
    "M367 232 C385 234 403 220 413 201";
  return `<path d="${combinedBodyArm}" fill="${BLACK}" stroke="${WHITE}" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="${armInner}" fill="none" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    ${eyeMarkup()}`;
}

function stepMark() {
  return `${exactMark({ transform: transformAround(225, 245, 0.58) })}
    <path d="M92 360 H170 V330 H250 V300 H330 V270 H418"
      fill="none" stroke="${WHITE}" stroke-width="8"
      stroke-linecap="round" stroke-linejoin="round"/>`;
}

function tuneMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="5.5" stroke-linecap="round" stroke-linejoin="round">
      <path d="M330 267 C294 312 235 329 183 306 C143 289 105 284 70 282"/>
      <path d="M306 249 C278 282 232 299 195 286 C160 275 120 269 84 268"/>
      <path d="M70 282 L98 234 M84 268 L109 224"/>
      <path d="M95 235 C82 228 78 216 84 206 C90 196 103 197 114 205
        C124 213 127 225 120 234 C114 243 104 242 95 235 Z"/>
      <path d="M86 210 C97 212 108 220 114 231 M82 219 C93 221 103 228 109 238"/>
      <path d="M73 270 C81 270 87 274 90 280 M78 261 C86 262 92 267 95 273"/>
    </g>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
    <g fill="none" stroke="${WHITE}" stroke-width="4.5" stroke-linecap="round" stroke-linejoin="round">
      <path d="M119 212 C123 201 131 197 138 201 C146 205 145 217 138 223"/>
      <path d="M129 199 V181 C129 176 133 174 138 176 L151 181"/>
    </g>
    ${eyeMarkup()}`;
}

function volumeMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="6" stroke-linecap="round">
      <path d="M286 86 V166 M286 282 V414"/>
      <path d="M306 86 V177 M306 277 V414"/>
    </g>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="5.5" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M278 214 H314 V254 H278 Z" fill="${BLACK}" stroke="${WHITE}" stroke-width="5" stroke-linejoin="round"/>
    ${eyeMarkup()}`;
}

function vocoderMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
      <path d="${TAIL_OUTER}"/><path d="${TAIL_INNER}"/>
    </g>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M140 200 C151 190 168 190 180 198 L176 217
      C165 224 151 222 144 214 Z" fill="${WHITE}"/>
    <path d="M175 199 C190 188 218 188 235 198 L231 216
      C217 225 194 224 180 217 Z" fill="${WHITE}"/>
    <path d="${JOIN_OUTER}" fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round"/>
  `;
}

function xyzMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linejoin="miter">
      <path d="M236 166 L250 130 L278 108 L307 110 L337 128 L358 165
        L374 220 L384 278 L397 340 L405 365 L402 386 L393 389
        L377 363 L362 322 L350 278 L337 225 L324 195 L292 176"/>
      <path d="M250 168 L264 140 L286 120 L307 122 L329 139 L345 171
        L360 223 L371 280 L384 340 L390 358 L387 373 L381 362
        L367 318 L355 275 L343 221 L330 192 L293 173"/>
    </g>
    <polygon points="113,228 149,192 194,174 220,166 260,166 295,179 318,198 330,221 330,267 292,277 247,284 199,281 158,269 113,244 129,238"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="5" stroke-linejoin="miter"/>
    <g stroke="#909090" stroke-width="2.5" stroke-linejoin="miter">
      <polygon points="113,228 194,174 220,225 129,238" fill="#353535"/>
      <polygon points="194,174 260,166 247,226 220,225" fill="#5A5A5A"/>
      <polygon points="260,166 318,198 247,226" fill="#2E2E2E"/>
      <polygon points="129,238 220,225 199,281 158,269" fill="#484848"/>
      <polygon points="220,225 247,226 292,277 199,281" fill="#666666"/>
      <polygon points="247,226 318,198 330,267 292,277" fill="#3C3C3C"/>
    </g>
    <path d="M274 272 L268 230 L274 202 L288 185 L305 185 L317 199 L326 236 L331 265
      M290 277 L284 238 L289 211 L300 195 L313 204 L322 238 L327 260"
      fill="none" stroke="${WHITE}" stroke-width="5" stroke-linejoin="miter"/>
    <polygon points="149,205 158,201 167,209 163,219 152,220 146,212" fill="${WHITE}"/>
    <polygon points="178,204 190,199 202,207 203,220 193,228 180,223 175,213" fill="${WHITE}"/>`;
}

function limiterMark() {
  const compressed = transformAround(250, 252, 1, 0.57);
  return `<path d="M102 171 H398" fill="none" stroke="${WHITE}" stroke-width="7" stroke-linecap="round"/>
    <g transform="${compressed}">
      ${exactMark()}
      <ellipse cx="158" cy="211" rx="13" ry="13" fill="${BLACK}"/>
      <ellipse cx="190" cy="215" rx="17" ry="17" fill="${BLACK}"/>
      ${eyeMarkup("x")}
    </g>`;
}

function subLowMark() {
  const bassBody =
    "M105 228 C137 189 180 168 224 168 C257 168 283 180 295 198 " +
    "C304 212 300 225 287 235 C305 247 307 263 292 278 " +
    "C251 294 171 286 105 244 L123 238 Z";
  return `<path d="${bassBody}" fill="${BLACK}"/>
    <g fill="none" stroke="${WHITE}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
      <path d="${bassBody}"/>
      <path d="M278 194 L410 105 L438 130 L296 228 Z" fill="${BLACK}" stroke-width="6"/>
      <path d="M286 202 L424 116 M289 209 L429 121 M292 216 L433 126 M295 223 L437 131" stroke-width="3.8"/>
      <path d="M410 105 L430 90 L458 113 L438 130 Z" fill="${BLACK}" stroke-width="6"/>
      <path d="M428 96 L435 85 M439 103 L449 94 M447 116 L459 120 M435 128 L444 140"/>
      <path d="M274 232 C258 226 244 230 237 242 C230 254 237 267 250 271"/>
      <path d="M225 214 L251 210 L257 244 L231 248 Z M258 206 L278 204 L284 236 L264 240 Z" stroke-width="4"/>
    </g>
    ${eyeMarkup()}`;
}

const products = [
  { id: "tb_center", label: "TB CENTER", render: centerMark },
  { id: "tb_compressor", label: "TB COMPRESSOR", render: compressorMark },
  { id: "tb_distortion", label: "TB DISTORTION", render: distortionMark },
  { id: "tb_disperser", label: "TB DISPERSER", render: disperserMark },
  { id: "tb_eq", label: "TB EQ", render: eqMark },
  { id: "tb_colorizer", label: "TB COLORIZER", render: colorizerMark },
  { id: "tb_inverted_flanger", label: "TB INVERTED FLANGER", render: flangerMark },
  { id: "tb_inverted_phaser", label: "TB INVERTED PHASER", render: phaserMark },
  { id: "tb_jewel_digger", label: "TB JEWEL DIGGER", render: jewelMark },
  { id: "tb_noise_remover", label: "TB NOISE REMOVER", render: noiseRemoverMark },
  { id: "tb_parallel_reverb", label: "TB PARALLEL REVERB", render: parallelReverbMark },
  { id: "tb_scrambler", label: "TB SCRAMBLER", render: scramblerMark },
  { id: "tb_transient_shaper", label: "TB TRANSIENT SHAPER", render: transientMark },
  { id: "tb_step_shifter", label: "TB STEP SHIFTER", render: stepMark },
  { id: "tb_tune", label: "TB TUNE", render: tuneMark },
  { id: "tb_volume", label: "TB VOLUME", render: volumeMark },
  { id: "tb_vocoder", label: "TB VOCODER", render: vocoderMark },
  { id: "tb_xyz_panner", label: "TB XYZ PANNER", render: xyzMark },
  { id: "tb_limiter", label: "TB LIMITER", render: limiterMark },
  { id: "tb_sublow", label: "TB SUBLOW", render: subLowMark },
];

function iconSvg(product) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 500 500">
    <defs>
      <linearGradient id="colorFlow" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#40D9FF"/>
        <stop offset="0.34" stop-color="#56F39A"/>
        <stop offset="0.68" stop-color="#EF67D5"/>
        <stop offset="1" stop-color="#FF9D4D"/>
      </linearGradient>
      <filter id="softBlur" x="-20%" y="-20%" width="140%" height="140%">
        <feGaussianBlur stdDeviation="5"/>
      </filter>
    </defs>
    <rect width="500" height="500" fill="${BLACK}"/>
    <g transform="translate(0 22)">
      ${product.render()}
    </g>
  </svg>`;
  if (/<text\b/i.test(svg)) {
    throw new Error(`Icon renderer must not contain text: ${product.id}`);
  }
  return Buffer.from(svg);
}

function labelSvg(product, width, fontSize) {
  const escaped = product.label
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
  return Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="30">
    <text x="50%" y="${fontSize + 2}" text-anchor="middle"
      font-family="Arial, sans-serif" font-size="${fontSize}" font-weight="700"
      letter-spacing="0.4" fill="#D6D6D6">${escaped}</text>
  </svg>`);
}

async function renderProduct(product) {
  const masterPath = path.join(masterDir, `${product.id}.png`);
  const hubPath = path.join(hubDir, `${product.id}.png`);
  const fallbackPath = path.join(fallbackDir, `${product.id}.png`);

  await sharp(iconSvg(product))
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
  const cellWidth = iconSize >= 180 ? 240 : 170;
  const cellHeight = iconSize >= 180 ? 250 : 108;
  const width = columns * cellWidth;
  const height = rows * cellHeight;
  const composites = [];

  for (let index = 0; index < products.length; index += 1) {
    const product = products[index];
    const column = index % columns;
    const row = Math.floor(index / columns);
    const left = column * cellWidth + Math.floor((cellWidth - iconSize) / 2);
    const top = row * cellHeight + 8;
    const icon = await sharp(path.join(masterDir, `${product.id}.png`))
      .resize(iconSize, iconSize, { kernel: sharp.kernel.lanczos3 })
      .png()
      .toBuffer();
    composites.push({ input: icon, left, top });
    composites.push({
      input: labelSvg(product, cellWidth, iconSize >= 180 ? 12 : 9),
      left: column * cellWidth,
      top: row * cellHeight + iconSize + 12,
    });
  }

  await sharp({
    create: {
      width,
      height,
      channels: 4,
      background: "#111111",
    },
  })
    .composite(composites)
    .png({ compressionLevel: 9 })
    .toFile(path.join(qaDir, filename));
}

async function buildSocialPreview() {
  const width = 1280;
  const height = 640;
  const composites = [];
  const iconSize = 112;
  const startX = 650;
  const startY = 70;
  const gapX = 118;
  const gapY = 122;

  for (let index = 0; index < products.length; index += 1) {
    const row = Math.floor(index / 5);
    const column = index % 5;
    const icon = await sharp(path.join(masterDir, `${products[index].id}.png`))
      .resize(iconSize, iconSize, { kernel: sharp.kernel.lanczos3 })
      .png()
      .toBuffer();
    composites.push({
      input: icon,
      left: startX + column * gapX,
      top: startY + row * gapY,
    });
  }

  const title = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="580" height="460">
    <text x="0" y="110" font-family="Arial, sans-serif" font-size="70" font-weight="800" letter-spacing="3" fill="#FFFFFF">TB AUDIO</text>
    <text x="0" y="184" font-family="Arial, sans-serif" font-size="70" font-weight="800" letter-spacing="3" fill="#FFFFFF">PLUG-INS</text>
    <path d="M0 224 H360" stroke="#FFFFFF" stroke-width="5"/>
    <text x="0" y="275" font-family="Arial, sans-serif" font-size="23" font-weight="600" letter-spacing="4" fill="#CFCFCF">TEAM IMPULSE IMPACT</text>
    <text x="0" y="335" font-family="Arial, sans-serif" font-size="20" fill="#A6A6A6">One mark. Twenty integrated redraws.</text>
  </svg>`);
  composites.unshift({ input: title, left: 70, top: 80 });

  await sharp({
    create: {
      width,
      height,
      channels: 4,
      background: BLACK,
    },
  })
    .composite(composites)
    .png({ compressionLevel: 9, adaptiveFiltering: true })
    .toFile(path.join(socialDir, "tb_audio_plugins_social_preview.png"));
}

async function buildReferenceComparison() {
  const reference = await sharp(referenceMarkPath).png().toBuffer();
  const vector = await sharp({
    create: {
      width: 500,
      height: 500,
      channels: 4,
      background: BLACK,
    },
  })
    .composite([{ input: canonicalMarkPath }])
    .png()
    .toBuffer();

  await sharp({
    create: {
      width: 1000,
      height: 500,
      channels: 4,
      background: BLACK,
    },
  })
    .composite([
      { input: reference, left: 0, top: 0 },
      { input: vector, left: 500, top: 0 },
    ])
    .png({ compressionLevel: 9 })
    .toFile(path.join(qaDir, "team-impulse-mark-reference-vs-vector.png"));
}

async function main() {
  for (const directory of [masterDir, hubDir, fallbackDir, socialDir, qaDir]) {
    fs.mkdirSync(directory, { recursive: true });
  }

  for (const product of products) {
    await renderProduct(product);
  }

  await buildSocialPreview();
  await buildContactSheet(200, "plugin-icons-200px-contact-sheet.png");
  await buildContactSheet(64, "plugin-icons-64px-contact-sheet.png");
  await buildReferenceComparison();

  process.stdout.write(
    `Generated ${products.length} masters, ${products.length * 2} runtime copies, ` +
      "1 social preview, and 3 QA sheets.\n"
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
