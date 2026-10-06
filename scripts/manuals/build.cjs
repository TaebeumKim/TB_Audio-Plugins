#!/usr/bin/env node
// Builds the TB plug-in user manuals from content/<id>.<lang>.json into Documents/<LANG>/.
// Usage: node build.cjs [plugin-id ...] [--lang en,ko] [--out <dir>]
'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..', '..');
const CONTENT_DIR = path.join(__dirname, 'content');
const UI_DIR = path.join(__dirname, 'ui');
const ICON_DIR = path.join(ROOT, 'assets', 'brand', 'plugin-icons');

const LANGS = {
  en: {
    folder: 'ENG',
    suffix: 'EN',
    htmlLang: 'en',
    manual: 'User Manual',
    version: 'Version',
    contents: 'Contents',
    page: 'Page',
    supportTitle: 'Support and license',
    support: [
      'Report problems or request features at **github.com/TaebeumKim/TB_Audio-Plugins/issues**. Include the plug-in version, your DAW, and your operating system.',
      'TB Audio plug-ins are free for personal and commercial work under the TB Audio Plug-ins Freeware License. If you release or monetize work made with them, add a credit such as **Made with TB Audio Plug-ins by Team Impulse Impact**. Selling or bundling the plug-ins is not allowed.',
      'If the plug-ins help your music, you can support development at **ko-fi.com/teamimpulseimpact**.',
    ],
  },
  ko: {
    folder: 'KOR',
    suffix: 'KO',
    htmlLang: 'ko',
    manual: '사용자 설명서',
    version: '버전',
    contents: '목차',
    page: '쪽',
    supportTitle: '지원 및 라이선스',
    support: [
      '문제 신고나 기능 요청은 **github.com/TaebeumKim/TB_Audio-Plugins/issues** 에 남겨 주세요. 플러그인 버전, 사용하는 DAW, 운영체제를 함께 적어 주시면 빠르게 확인할 수 있습니다.',
      'TB Audio 플러그인은 TB Audio Plug-ins Freeware License에 따라 개인 작업과 상업 작업 모두 무료로 사용할 수 있습니다. 플러그인으로 만든 작업물을 발매하거나 수익화할 때는 **Made with TB Audio Plug-ins by Team Impulse Impact** 와 같은 크레딧을 표기해야 하며, 플러그인을 판매하거나 유료 번들에 포함하는 것은 허용되지 않습니다.',
      '플러그인이 작업에 도움이 되었다면 **ko-fi.com/teamimpulseimpact** 에서 개발을 응원해 주세요.',
    ],
  },
};

function parseArgs(argv) {
  const args = { ids: [], langs: Object.keys(LANGS), out: null };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--lang') args.langs = argv[++i].split(',');
    else if (a === '--out') args.out = path.resolve(argv[++i]);
    else args.ids.push(a);
  }
  return args;
}

function fontFaces() {
  const dir = path.join(path.dirname(require.resolve('pretendard/package.json')), 'dist', 'web', 'static', 'woff2');
  const weights = { Regular: 400, Medium: 500, SemiBold: 600, Bold: 700, ExtraBold: 800 };
  return Object.entries(weights)
    .map(([name, weight]) => `@font-face{font-family:'Pretendard';font-weight:${weight};font-style:normal;src:url('${pathToFileURL(path.join(dir, `Pretendard-${name}.woff2`)).href}') format('woff2');}`)
    .join('\n');
}

function esc(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// **bold**, `PANEL LABEL`, {{n}} callout reference
function inline(s) {
  return esc(s)
    .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
    .replace(/`(.+?)`/g, '<span class="ui">$1</span>')
    .replace(/\{\{(\d+)\}\}/g, '<span class="ref">$1</span>');
}

function table(t) {
  const widths = t.widths ? `<colgroup>${t.widths.map((w) => `<col style="width:${w}%">`).join('')}</colgroup>` : '';
  const head = `<thead><tr>${t.cols.map((c) => `<th>${inline(c)}</th>`).join('')}</tr></thead>`;
  const body = t.rows
    .map((r) => (Array.isArray(r)
      ? `<tr>${r.map((c, i) => `<td${i === 0 ? ' class="first"' : ''}>${inline(c)}</td>`).join('')}</tr>`
      : `<tr class="group"><td colspan="${t.cols.length}">${inline(r.group)}</td></tr>`))
    .join('');
  return `<table class="${t.compact ? 'compact' : ''}">${widths}${head}<tbody>${body}</tbody></table>`;
}

function uiFigure(f, ctx) {
  const img = pathToFileURL(path.join(UI_DIR, f.image)).href;
  const marks = (f.callouts || [])
    .map((c) => `<span class="mark" style="left:${c.x}%;top:${c.y}%">${c.n}</span>`)
    .join('');
  const legend = f.legend
    ? `<ol class="legend">${f.legend.map(([n, title, text]) => `<li><span class="ref">${n}</span><div><strong>${inline(title)}</strong>${text ? ` — ${inline(text)}` : ''}</div></li>`).join('')}</ol>`
    : '';
  const caption = f.caption ? `<div class="caption">${inline(f.caption)}</div>` : '';
  return `<figure class="uifig"><div class="shot"><img src="${img}">${marks}</div>${caption}</figure>${legend}`;
}

function flow(f) {
  const steps = f.steps.map((s) => `<div class="node">${inline(s)}</div>`).join('<div class="arrow">→</div>');
  return `<div class="flow">${steps}</div>${f.caption ? `<div class="caption">${inline(f.caption)}</div>` : ''}`;
}

function recipe(r) {
  const settings = r.settings
    ? `<table class="kv">${r.settings.map(([k, v]) => `<tr><th>${inline(k)}</th><td>${inline(v)}</td></tr>`).join('')}</table>`
    : '';
  const steps = r.steps ? `<ol class="steps small">${r.steps.map((s) => `<li>${inline(s)}</li>`).join('')}</ol>` : '';
  return `<div class="recipe"><div class="recipe-title">${inline(r.title)}</div>${r.use ? `<p class="recipe-use">${inline(r.use)}</p>` : ''}${settings}${steps}</div>`;
}

function box(kind, v) {
  const texts = Array.isArray(v.text) ? v.text : v.text ? [v.text] : [];
  const list = v.list ? `<ul>${v.list.map((i) => `<li>${inline(i)}</li>`).join('')}</ul>` : '';
  return `<div class="box ${kind}">${v.title ? `<div class="box-title">${inline(v.title)}</div>` : ''}${texts.map((t) => `<p>${inline(t)}</p>`).join('')}${list}</div>`;
}

function block(b, ctx) {
  const [k] = Object.keys(b);
  const v = b[k];
  switch (k) {
    case 'h2': return `<h2 id="s${++ctx.section}"><span class="num">${String(ctx.section).padStart(2, '0')}</span>${inline(v)}</h2>`;
    case 'h3': return `<h3>${inline(v)}</h3>`;
    case 'p': return `<p>${inline(v)}</p>`;
    case 'lead': return `<p class="lead">${inline(v)}</p>`;
    case 'ul': return `<ul>${v.map((i) => `<li>${inline(i)}</li>`).join('')}</ul>`;
    case 'ol': return `<ol class="steps">${v.map((i) => `<li>${inline(i)}</li>`).join('')}</ol>`;
    case 'note': case 'tip': case 'warn': case 'new': return box(k, v);
    case 'table': return table(v);
    case 'ui': return uiFigure(v, ctx);
    case 'flow': return flow(v);
    case 'recipe': return recipe(v);
    case 'recipes': return `<div class="recipes">${v.map(recipe).join('')}</div>`;
    case 'pagebreak': return '<div class="pagebreak"></div>';
    default: throw new Error(`Unknown block type: ${k}`);
  }
}

function cover(doc, L, plugin) {
  const icon = pathToFileURL(path.join(ICON_DIR, `${doc.icon || plugin.id}.png`)).href;
  const shot = doc.coverImage !== false && doc.ui
    ? `<div class="cover-shot"><img src="${pathToFileURL(path.join(UI_DIR, doc.coverImage || doc.ui)).href}"></div>`
    : '';
  const facts = (doc.facts || []).map((f) => `<span>${inline(f)}</span>`).join('');
  return `<section class="cover">
  <div class="cover-top"><img class="cover-icon" src="${icon}"><div class="cover-brand">TB AUDIO PLUG-INS<br><span>Team Impulse Impact</span></div></div>
  <div class="cover-title">
    <div class="kicker">${esc(L.manual)}</div>
    <h1>${esc(plugin.name)}</h1>
    <div class="subtitle">${inline(doc.subtitle)}</div>
    <p class="tagline">${inline(doc.tagline)}</p>
    <div class="facts">${facts}</div>
  </div>
  ${shot}
  <div class="cover-foot">${esc(L.version)} ${esc(doc.version)} · ${esc(doc.date)}</div>
</section>`;
}

function toc(doc, L) {
  const items = doc.body.filter((b) => b.h2).map((b, i) => `<li><span class="num">${String(i + 1).padStart(2, '0')}</span>${inline(b.h2)}</li>`).join('');
  return `<nav class="toc"><div class="toc-title">${esc(L.contents)}</div><ol>${items}<li><span class="num">${String(doc.body.filter((b) => b.h2).length + 1).padStart(2, '0')}</span>${esc(L.supportTitle)}</li></ol></nav>`;
}

function html(doc, L, plugin) {
  const ctx = { section: 0 };
  const body = doc.body.map((b) => block(b, ctx)).join('\n');
  const support = block({ h2: L.supportTitle }, ctx) + L.support.map((s) => `<p>${inline(s)}</p>`).join('');
  const css = fs.readFileSync(path.join(__dirname, 'manual.css'), 'utf8');
  const footer = `${plugin.name} ${doc.version} · ${L.manual}`;
  return `<!doctype html><html lang="${L.htmlLang}"><head><meta charset="utf-8">
<style>${fontFaces()}
${css}
@page { @bottom-left { content: "${footer.replace(/"/g, '\\"')}"; } }
</style></head><body>
${cover(doc, L, plugin)}
<main>${doc.toc === false ? '' : toc(doc, L)}${body}${support}</main>
</body></html>`;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const catalog = JSON.parse(fs.readFileSync(path.join(ROOT, 'catalog.json'), 'utf8'));
  const plugins = Object.fromEntries(catalog.plugins.map((p) => [p.id, p]));
  const files = fs.readdirSync(CONTENT_DIR).filter((f) => f.endsWith('.json'));
  const jobs = files
    .map((f) => { const m = f.match(/^(.+)\.([a-z]{2})\.json$/); return m && { id: m[1], lang: m[2], file: f }; })
    .filter((j) => j && args.langs.includes(j.lang) && (args.ids.length === 0 || args.ids.includes(j.id)));
  if (jobs.length === 0) throw new Error('No matching content files.');

  const browser = await chromium.launch();
  const page = await browser.newPage();
  const tmp = path.join(__dirname, '.render.html');
  try {
    for (const job of jobs) {
      const doc = JSON.parse(fs.readFileSync(path.join(CONTENT_DIR, job.file), 'utf8'));
      const plugin = plugins[job.id];
      if (!plugin) throw new Error(`${job.id} is not in catalog.json`);
      const L = LANGS[job.lang];
      fs.writeFileSync(tmp, html(doc, L, plugin));
      await page.goto(pathToFileURL(tmp).href, { waitUntil: 'load' });
      await page.evaluate(() => document.fonts.ready);
      const outDir = args.out || path.join(ROOT, 'Documents', L.folder);
      const out = path.join(outDir, `${doc.file}_Detailed_User_Manual_${L.suffix}.pdf`);
      await page.pdf({ path: out, printBackground: true, preferCSSPageSize: true });
      console.log(`${job.id} [${job.lang}] -> ${path.relative(ROOT, out)}`);
    }
  } finally {
    await browser.close();
    fs.rmSync(tmp, { force: true });
  }
}

main().catch((err) => { console.error(err); process.exit(1); });
