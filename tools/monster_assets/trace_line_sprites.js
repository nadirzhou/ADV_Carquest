import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { basename, join, resolve } from 'node:path';

const require = createRequire(import.meta.url);
const potrace = require('potrace');

const args = parseArgs(process.argv.slice(2));
const inputDir = resolve(args.inputDir || args.inputdir || '../../main/apps/app_signal_quest/assets/monster_line_trace');
const outDir = resolve(args.outDir || args.outdir || '../../main/apps/app_signal_quest/assets/monster_line_svg');
mkdirSync(outDir, { recursive: true });

const files = (args.files || '').split(';').filter(Boolean);
for (const file of files) {
  const input = resolve(file);
  const slug = basename(input).replace(/\.[^.]+$/, '');
  const svg = await trace(input);
  const d = extractPath(svg);
  const out = join(outDir, `${slug}.svg`);
  writeFileSync(out, [
    '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">',
    `  <path fill="currentColor" fill-rule="evenodd" clip-rule="evenodd" d="${d}"/>`,
    '</svg>',
    ''
  ].join('\n'));
  console.log(`Wrote ${out}`);
}

function trace(file) {
  return new Promise((resolveTrace, reject) => {
    potrace.trace(file, {
      threshold: 128,
      turdSize: Number(args.turdSize || 0),
      optCurve: true,
      optTolerance: Number(args.optTolerance || 0.12),
      turnPolicy: potrace.Potrace.TURNPOLICY_MINORITY,
      color: '#000000',
      background: 'transparent'
    }, (err, svg) => {
      if (err) reject(err);
      else resolveTrace(svg);
    });
  });
}

function extractPath(svg) {
  const match = svg.match(/<path\b[^>]*\sd="([^"]+)"/);
  return match ? match[1] : '';
}

function parseArgs(values) {
  const parsed = {};
  for (let i = 0; i < values.length; i++) {
    const value = values[i];
    if (!value.startsWith('--')) continue;
    const key = value.slice(2);
    const next = values[i + 1];
    if (next && !next.startsWith('--')) {
      parsed[key] = next;
      i++;
    } else {
      parsed[key] = true;
    }
  }
  return parsed;
}
