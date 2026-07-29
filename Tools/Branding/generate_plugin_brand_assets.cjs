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

function exactMark({
  transform = "",
  fill = WHITE,
  opacity = 1,
  stroke = "",
  strokeWidth = 0,
} = {}) {
  const transformAttribute = transform ? ` transform="${transform}"` : "";
  let markup = canonicalMarkMarkup.replace('fill="#FFFFFF"', `fill="${fill}"`);
  if (stroke && strokeWidth > 0) {
    markup = markup.replace(
      "<path ",
      `<path stroke="${stroke}" stroke-width="${strokeWidth}" ` +
        'stroke-linejoin="round" paint-order="stroke fill" '
    );
  }
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
      opacity: 0.32,
    }),
    exactMark({
      transform: transformAround(320, 250, 0.72),
      opacity: 0.32,
    }),
    exactMark({
      transform: transformAround(250, 250, 0.88),
      stroke: WHITE,
      strokeWidth: 3.5,
    }),
  ].join("");
}

function compressorMark() {
  return `<g fill="none" stroke="${WHITE}" stroke-linecap="round">
      <path d="M105 154 H395" stroke-width="7" stroke-dasharray="14 12" opacity="0.48"/>
      <path d="M105 346 H395" stroke-width="7" stroke-dasharray="14 12" opacity="0.48"/>
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
  const inverted = "translate(0 452) scale(1 -1)";
  return `<defs>
      <clipPath id="phaser-mid"><circle cx="250" cy="226" r="112"/></clipPath>
      <clipPath id="phaser-core"><circle cx="250" cy="226" r="62"/></clipPath>
    </defs>
    ${exactMark({ transform: inverted })}
    <circle cx="250" cy="226" r="112" fill="${BLACK}"/>
    <g clip-path="url(#phaser-mid)" transform="rotate(12 250 226)">
      ${exactMark({ transform: inverted })}
    </g>
    <circle cx="250" cy="226" r="62" fill="${BLACK}"/>
    <g clip-path="url(#phaser-core)" transform="rotate(27 250 226)">
      ${exactMark({ transform: inverted })}
    </g>
    <g fill="none" stroke="${WHITE}" stroke-width="7" stroke-linecap="round" opacity="0.48">
      <path d="M343 132 C385 155 404 195 397 236"/>
      <path d="M397 236 L386 220 M397 236 L407 218"/>
      <path d="M157 320 C119 294 103 255 112 216"/>
      <path d="M112 216 L121 233 M112 216 L102 234"/>
    </g>`;
}

function jewelMark() {
  return `<path d="M278 276 L296 284 L375 141 L357 131 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="7" stroke-linejoin="round"/>
    <path d="M238 95 C286 91 329 102 366 121
      L356 140 C322 118 283 105 242 107 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="7"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M371 123 C401 132 429 147 451 166
      L425 164 C403 158 383 149 359 139 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="7"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M350 119 L381 135 L369 154 L339 138 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="7"
      stroke-linejoin="round"/>
    <path d="M219 56 V73 M219 91 V108 M191 82 H208 M230 82 H247"
      fill="none" stroke="${WHITE}" stroke-width="7" stroke-linecap="round"/>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>
    ${eyeMarkup()}`;
}

function noiseRemoverMark() {
  const headphoneBand =
    "M214 205 C217 148 243 117 275 116 C311 116 331 151 333 207";
  return `${baseMark({ strokeWidth: 7 })}
    <path d="${headphoneBand}" fill="none" stroke="${BLACK}" stroke-width="26"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="${headphoneBand}" fill="none" stroke="${WHITE}" stroke-width="12"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M234 171 C256 167 278 182 285 205 L291 246
      C294 271 278 291 255 293 C232 295 214 275 213 248
      L213 210 C214 191 224 177 234 171 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="11"
      stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M243 195 C256 192 267 203 270 217 L274 245
      C276 258 268 269 257 270 C246 271 238 261 237 247
      L237 214 C237 205 240 198 243 195 Z"
      fill="none" stroke="${WHITE}" stroke-width="7"
      stroke-linecap="round" stroke-linejoin="round"/>`;
}

function parallelReverbMark() {
  const variants = [
    { x: 195, y: 242, color: "#45E3D2" },
    { x: 250, y: 250, color: "#F067B7" },
    { x: 305, y: 258, color: "#F6C85F" },
  ];
  return (
    `<ellipse cx="250" cy="250" rx="175" ry="125"
      fill="url(#reverbBloom)" filter="url(#reverbBlur)" opacity="0.8"/>` +
    variants
      .map(
        ({ x, y, color }) =>
          `<g filter="url(#softBlur)" opacity="0.28">${exactMark({
            transform: transformAround(x - 5, y + 5, 0.68),
            fill: color,
          })}</g>`
      )
      .join("") +
    variants
      .map(({ x, y, color }) =>
        exactMark({
          transform: transformAround(x, y, 0.68),
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
  const fishTransform = transformAround(170, 250, 0.72);
  const flexedArm =
    "M12.4 13.1 " +
    "C14.2 10.6 17.5 10.4 19.8 12.1 " +
    "C22.2 13.9 22.6 17.1 20.7 19.4 " +
    "C18.8 21.8 15.6 22.3 12.6 22.1 " +
    "C8.8 22 5.1 21.2 2.9 19.7 " +
    "C2.3 19.3 2 18.6 2.1 17.8 " +
    "C2.3 13.1 3.3 3 9.7 2.1 " +
    "C11.5 1.9 13 3.2 13 5 " +
    "C13 6.3 12.2 7.2 11 7.2 " +
    "C10 7.2 9.3 6.7 8.9 5.9";
  const shoulder = "M218 263 C238 256 256 261 274 273";
  return `<g transform="translate(250 95) scale(9.5)"
      stroke-linecap="round" stroke-linejoin="round">
      <path d="${flexedArm}" fill="${BLACK}" stroke="${BLACK}" stroke-width="1.8"/>
      <path d="${flexedArm}" fill="none" stroke="${WHITE}" stroke-width="0.82"/>
      <path d="M15.1 14.2 C13.3 12.2 9.6 12.2 7.5 16"
        fill="none" stroke="${WHITE}" stroke-width="0.76"/>
      <path d="M9 6.9 C7.8 8.8 9.1 12.8 7.9 15.1"
        fill="none" stroke="${WHITE}" stroke-width="0.76"/>
    </g>
    <g transform="${fishTransform}" fill="none" stroke-linecap="round"
      stroke-linejoin="round">
      <g stroke="${BLACK}" stroke-width="20">
        <path d="${TAIL_OUTER}"/>
        <path d="${TAIL_INNER}"/>
      </g>
      <g stroke="${WHITE}" stroke-width="8">
        <path d="${TAIL_OUTER}"/>
        <path d="${TAIL_INNER}"/>
      </g>
    </g>
    <path d="${shoulder}" fill="none" stroke="${BLACK}" stroke-width="28"
      stroke-linecap="round"/>
    <path d="${shoulder}" fill="none" stroke="${WHITE}" stroke-width="12"
      stroke-linecap="round"/>
    <g transform="${fishTransform}">
      <path d="${BODY}" fill="${BLACK}" stroke="none"/>
      <g fill="none" stroke="${WHITE}" stroke-width="8"
        stroke-linecap="round" stroke-linejoin="round">
        <path d="${JOIN_OUTER}"/>
        <path d="${JOIN_INNER}"/>
        <path d="${BODY}"/>
      </g>
      ${eyeMarkup()}
    </g>`;
}

function stepMark() {
  return `${exactMark({ transform: transformAround(225, 245, 0.58) })}
    <path d="M92 360 H170 V330 H250 V300 H330 V270 H418"
      fill="none" stroke="${WHITE}" stroke-width="8"
      stroke-linecap="round" stroke-linejoin="round"/>`;
}

function tuneMark() {
  const fishTransform = transformAround(280, 250, 0.78);
  return `${exactMark({ transform: fishTransform })}
    <g transform="${fishTransform}" fill="none" stroke="${WHITE}"
      stroke-width="7" stroke-linecap="round" stroke-linejoin="round">
      <path d="M137 237 C148 249 163 249 174 238"/>
    </g>
    <g fill="${BLACK}" stroke="${WHITE}" stroke-width="8"
      stroke-linecap="round" stroke-linejoin="round">
      <path d="M77 174 C61 179 54 195 59 212 L68 234
        C74 250 91 257 105 249 C119 241 121 224 113 209
        L102 187 C97 177 87 172 77 174 Z"/>
      <path d="M96 248 L123 301 L105 310 L79 257 Z"/>
    </g>
    <g fill="none" stroke="${WHITE}" stroke-width="5"
      stroke-linecap="round" opacity="0.9">
      <path d="M68 197 L103 187 M65 209 L109 198 M69 222 L113 211"/>
      <path d="M116 211 C125 205 131 198 134 190"/>
      <path d="M125 228 C136 222 143 214 147 204"/>
    </g>`;
}

function volumeMark() {
  return `<path d="M250 88 V175 M250 280 V410"
      fill="none" stroke="${WHITE}" stroke-width="8" stroke-linecap="round"/>
    <path d="${BODY}" fill="${BLACK}"/>
    <path d="${BODY}" fill="none" stroke="${WHITE}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M219 217 H281 V259 H219 Z"
      fill="${BLACK}" stroke="${WHITE}" stroke-width="7" stroke-linejoin="round"/>
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
  const gameBody =
    "M104 228 C140 184 186 160 230 158 C277 156 317 174 339 205 " +
    "C351 222 351 247 341 267 C308 292 249 302 190 292 " +
    "C153 286 120 270 96 248 L120 238 Z";
  const gameTail =
    "M244 160 C263 107 306 92 341 111 C377 131 388 188 394 239 " +
    "C402 298 417 345 419 365 C421 382 413 395 402 392 " +
    "C382 387 366 350 357 313 L333 219 C327 194 311 177 289 171";
  return `<defs>
      <clipPath id="xyz-body-clip"><path d="${gameBody}"/></clipPath>
    </defs>
    <ellipse cx="254" cy="365" rx="145" ry="26"
      fill="url(#xyzGround)" filter="url(#xyzGroundBlur)"/>
    <g transform="rotate(-7 250 250)">
      <path d="${gameTail}" transform="translate(16 14)"
        fill="none" stroke="#120A2D" stroke-width="36"
        stroke-linecap="round" stroke-linejoin="round"/>
      <path d="${gameTail}" fill="none" stroke="url(#xyzTail)"
        stroke-width="27" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="${gameTail}" transform="translate(-3 -4)"
        fill="none" stroke="#C7FFFF" stroke-width="6" opacity="0.72"
        stroke-linecap="round" stroke-linejoin="round"/>

      <path d="${gameBody}" transform="translate(16 14)"
        fill="url(#xyzBodyDepth)" stroke="#100923" stroke-width="8"
        stroke-linejoin="round"/>
      <path d="M341 267 C308 292 249 302 190 292
        C153 286 120 270 96 248 L112 262
        C138 282 172 297 209 305 C267 315 326 303 357 281 Z"
        fill="url(#xyzBodyDepth)" opacity="0.92"/>
      <path d="${gameBody}" fill="url(#xyzBody)"
        stroke="#170B2C" stroke-width="8" stroke-linejoin="round"/>
      <ellipse cx="201" cy="185" rx="96" ry="48"
        fill="url(#xyzSpecular)" clip-path="url(#xyz-body-clip)"
        filter="url(#xyzSpecularBlur)"/>
      <path d="M111 226 C149 185 188 168 229 166
        C267 163 297 173 320 191"
        fill="none" stroke="#D8FFFF" stroke-width="8" opacity="0.72"
        stroke-linecap="round"/>
      <path d="M113 253 C159 282 221 292 279 282
        C304 278 324 270 340 258"
        fill="none" stroke="#7D2CC9" stroke-width="10" opacity="0.58"
        stroke-linecap="round"/>
      <ellipse cx="282" cy="236" rx="58" ry="38"
        fill="#EE4EFF" opacity="0.16" filter="url(#xyzSpecularBlur)"
        clip-path="url(#xyz-body-clip)"/>

      <circle cx="162" cy="216" r="16" fill="#091126" opacity="0.7"/>
      <circle cx="198" cy="221" r="23" fill="#091126" opacity="0.72"/>
      <circle cx="157" cy="211" r="14" fill="url(#xyzEye)"
        stroke="#D8FFFF" stroke-width="3"/>
      <circle cx="192" cy="215" r="21" fill="url(#xyzEye)"
        stroke="#D8FFFF" stroke-width="3"/>
      <circle cx="161" cy="214" r="5.5" fill="#081024"/>
      <circle cx="187" cy="220" r="8.5" fill="#081024"/>
      <circle cx="154" cy="205" r="3.2" fill="${WHITE}"/>
      <circle cx="185" cy="207" r="4.5" fill="${WHITE}"/>
    </g>`;
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
  const bassHead =
    "M104 228 C137 190 181 169 224 166 L302 174 " +
    "C343 178 373 195 385 219 C396 242 386 266 361 283 " +
    "C331 303 286 305 242 295 C188 284 143 267 104 244 L123 238 Z";
  return `<path d="${bassHead}" fill="${BLACK}"/>
    <g fill="none" stroke="${WHITE}" stroke-linecap="round" stroke-linejoin="round">
      <path d="${bassHead}" stroke-width="7"/>
      <path d="M207 194 V270" stroke-width="7"/>
      <path d="M211 211 L252 204 M211 224 L290 199
        M211 237 L329 205 M211 250 L361 219"
        stroke-width="5"/>
      <circle cx="252" cy="204" r="7" fill="${BLACK}" stroke-width="5"/>
      <circle cx="290" cy="199" r="7" fill="${BLACK}" stroke-width="5"/>
      <circle cx="329" cy="205" r="7" fill="${BLACK}" stroke-width="5"/>
      <circle cx="361" cy="219" r="7" fill="${BLACK}" stroke-width="5"/>
      <path d="M252 197 L250 159 M290 192 L291 153
        M329 198 L337 163 M361 212 L375 181" stroke-width="7"/>
    </g>
    <g fill="${BLACK}" stroke="${WHITE}" stroke-width="6" stroke-linejoin="round">
      <rect x="235" y="136" width="30" height="22" rx="5"/>
      <rect x="276" y="130" width="30" height="22" rx="5"/>
      <rect x="323" y="141" width="30" height="22" rx="5" transform="rotate(8 338 152)"/>
      <rect x="362" y="159" width="30" height="22" rx="5" transform="rotate(16 377 170)"/>
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
      <radialGradient id="reverbBloom" cx="50%" cy="50%" r="50%">
        <stop offset="0" stop-color="#F067B7" stop-opacity="0.42"/>
        <stop offset="0.48" stop-color="#45E3D2" stop-opacity="0.2"/>
        <stop offset="0.76" stop-color="#F6C85F" stop-opacity="0.11"/>
        <stop offset="1" stop-color="#000000" stop-opacity="0"/>
      </radialGradient>
      <linearGradient id="xyzBody" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#9AFFFF"/>
        <stop offset="0.32" stop-color="#57B8FF"/>
        <stop offset="0.62" stop-color="#7654F6"/>
        <stop offset="0.84" stop-color="#EC4DDB"/>
        <stop offset="1" stop-color="#4B187B"/>
      </linearGradient>
      <linearGradient id="xyzBodyDepth" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#25316F"/>
        <stop offset="0.52" stop-color="#27144E"/>
        <stop offset="1" stop-color="#120922"/>
      </linearGradient>
      <linearGradient id="xyzTail" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stop-color="#9AFFFF"/>
        <stop offset="0.45" stop-color="#4DA4FF"/>
        <stop offset="0.72" stop-color="#7751F4"/>
        <stop offset="1" stop-color="#ED4DCC"/>
      </linearGradient>
      <radialGradient id="xyzEye" cx="30%" cy="25%" r="75%">
        <stop offset="0" stop-color="#FFFFFF"/>
        <stop offset="0.62" stop-color="#E4F7FF"/>
        <stop offset="1" stop-color="#7586B0"/>
      </radialGradient>
      <radialGradient id="xyzSpecular" cx="50%" cy="50%" r="50%">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.9"/>
        <stop offset="0.38" stop-color="#A8FFFF" stop-opacity="0.5"/>
        <stop offset="1" stop-color="#8CFFFF" stop-opacity="0"/>
      </radialGradient>
      <radialGradient id="xyzGround" cx="50%" cy="50%" r="50%">
        <stop offset="0" stop-color="#5C3BDE" stop-opacity="0.48"/>
        <stop offset="0.58" stop-color="#3C1A8D" stop-opacity="0.24"/>
        <stop offset="1" stop-color="#000000" stop-opacity="0"/>
      </radialGradient>
      <filter id="softBlur" x="-20%" y="-20%" width="140%" height="140%">
        <feGaussianBlur stdDeviation="5"/>
      </filter>
      <filter id="reverbBlur" x="-30%" y="-40%" width="160%" height="180%">
        <feGaussianBlur stdDeviation="18"/>
      </filter>
      <filter id="xyzGroundBlur" x="-30%" y="-100%" width="160%" height="300%">
        <feGaussianBlur stdDeviation="12"/>
      </filter>
      <filter id="xyzSpecularBlur" x="-30%" y="-30%" width="160%" height="160%">
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
