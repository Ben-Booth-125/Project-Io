#!/usr/bin/env node
// cvd_palette_check.js — the colour-deficiency check on the lapse polity palette
// (BL-1087 R5; NATION_GENERATION.md § Pass 5: "the palette the slots index must
// hold under a colour deficiency, on the same reasoning that chose the
// twelve-slot nation table").
//
//   node tools/session/cvd_palette_check.js [--families 4..8] [--json]
//
// WHAT IT DOES. Reproduces `ui::palette::polity_slot_colour` (presentation.cpp)
// in JavaScript for F = 4..8 hue-family wedges, simulates protanopia,
// deuteranopia and tritanopia on every slot (Machado, Oliveira & Fernandes 2009,
// severity 1.0, in linear sRGB), converts to CIELAB, and reports the SMALLEST
// pairwise CIE76 distance in three classes, for normal vision and under each
// deficiency:
//   - NON-KIN: offset-0 colours of two different wedges (two unrelated realms);
//   - KIN:     two offsets of ONE wedge (two adjacent kin, which the greedy walk
//              gives offsets 0 and 1 first, then 2...);
//   - NATION:  the twelve-slot nation table, the reference the reasoning cites.
// A pair under ~10 dE is hard to tell apart; under ~5 it is one colour. The
// check is RECORDED, not asserted: the numbers go into the item's design text.
//
// Mirror of presentation.cpp by hand: keep the two in step when either moves.

'use strict';

const args = process.argv.slice(2);
const json = args.includes('--json');
let famArg = null;
const fi = args.indexOf('--families');
if (fi >= 0 && args[fi + 1]) famArg = args[fi + 1];
const families = famArg ? famArg.split(',').map(Number) : [4, 5, 6, 7, 8];

const OFFSETS = 6;
// Overridable from the environment for a candidate set (PAL_NUDGE, PAL_VALUE:
// six comma-separated numbers; PAL_BASE: "even,odd"; PAL_SAT: "even,odd"), so
// a re-tuning can be measured before it is mirrored into presentation.cpp.
const envList = (name, fallback) => (process.env[name] ? process.env[name].split(',').map(Number) : fallback);
const NUDGE = envList('PAL_NUDGE', [0.0, +0.35, -0.35, +0.2, -0.2, 0.0]);
const VALUE = envList('PAL_VALUE', [1.00, 0.60, 0.82, 0.70, 0.92, 0.52]);
const BASE  = envList('PAL_BASE',  [0.98, 0.76]);
const SAT   = envList('PAL_SAT',   [0.80, 0.55]);

function hsv2rgb(h, s, v) {
  h = h - Math.floor(h);
  const i = Math.floor(h * 6), f = h * 6 - i;
  const p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
  switch (i % 6) {
    case 0: return [v, t, p];
    case 1: return [q, v, p];
    case 2: return [p, v, t];
    case 3: return [p, q, v];
    case 4: return [t, p, v];
    default: return [v, p, q];
  }
}

// presentation.cpp: polity_slot_colour(slot, family_count, rung = 0)
function politySlotColour(slot, F) {
  const wedge = Math.floor(slot / OFFSETS) % F;
  const offset = slot % OFFSETS;
  const wedgeW = 1 / F;
  const centre = wedgeW * wedge;
  let hue = centre + NUDGE[offset] * (wedgeW * 0.5);
  hue = hue - Math.floor(hue);
  const baseV = (wedge % 2 === 0) ? BASE[0] : BASE[1];
  const s = (wedge % 2 === 0) ? SAT[0] : SAT[1], v = baseV * VALUE[offset];
  return hsv2rgb(hue, s, v).map(c => Math.round(c * 255 + 0.5) / 255);
}

// presentation.cpp: the twelve-slot nation table (Okabe-Ito + four lightness variants)
const NATION = [
  [213, 94, 0], [230, 159, 0], [240, 228, 66], [0, 158, 115], [86, 180, 233], [0, 114, 178],
  [204, 121, 167], [150, 150, 150], [140, 60, 0], [150, 210, 180], [150, 180, 220], [150, 80, 110],
].map(c => c.map(x => x / 255));

// Machado et al. 2009, severity 1.0
const CVD = {
  protan: [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
  deutan: [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.011820, 0.042940, 0.968881]],
  tritan: [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.303900]],
};

const lin = c => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
const clamp01 = x => Math.max(0, Math.min(1, x));

function simulate(rgb, kind) {
  if (kind === 'normal') return rgb;
  const l = rgb.map(lin);
  const M = CVD[kind];
  const out = [0, 1, 2].map(i => clamp01(M[i][0] * l[0] + M[i][1] * l[1] + M[i][2] * l[2]));
  // back to sRGB gamma for a uniform path into Lab below
  return out.map(c => (c <= 0.0031308 ? 12.92 * c : 1.055 * Math.pow(c, 1 / 2.4) - 0.055));
}

function rgb2lab(rgb) {
  const [r, g, b] = rgb.map(lin);
  let x = (r * 0.4124 + g * 0.3576 + b * 0.1805) / 0.95047;
  let y = (r * 0.2126 + g * 0.7152 + b * 0.0722) / 1.00000;
  let z = (r * 0.0193 + g * 0.1192 + b * 0.9505) / 1.08883;
  const f = t => (t > 0.008856 ? Math.cbrt(t) : 7.787 * t + 16 / 116);
  x = f(x); y = f(y); z = f(z);
  return [116 * y - 16, 500 * (x - y), 200 * (y - z)];
}

const dE = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);

function minPair(colours, kind) {
  const labs = colours.map(c => rgb2lab(simulate(c, kind)));
  let best = Infinity, pair = null;
  for (let i = 0; i < labs.length; ++i)
    for (let j = i + 1; j < labs.length; ++j) {
      const d = dE(labs[i], labs[j]);
      if (d < best) { best = d; pair = [i, j]; }
    }
  return { min: best, pair };
}

const kinds = ['normal', 'protan', 'deutan', 'tritan'];
const report = { families: {}, nation: {} };

for (const k of kinds) {
  const r = minPair(NATION, k);
  report.nation[k] = { min_dE: +r.min.toFixed(1), pair: r.pair };
}

for (const F of families) {
  const nonKin = [];
  for (let w = 0; w < F; ++w) nonKin.push(politySlotColour(w * OFFSETS, F));
  // the first three offsets of each wedge are what adjacent kin actually get
  const row = {};
  for (const k of kinds) {
    const nk = minPair(nonKin, k);
    let kinMin = Infinity, kinPair = null, kinWedge = -1, kin3Min = Infinity;
    for (let w = 0; w < F; ++w) {
      const kin = [];
      for (let o = 0; o < OFFSETS; ++o) kin.push(politySlotColour(w * OFFSETS + o, F));
      const r = minPair(kin, k);
      if (r.min < kinMin) { kinMin = r.min; kinPair = r.pair; kinWedge = w; }
      // The first THREE offsets are what adjacent kin actually receive (the
      // greedy walk hands out the lowest free one); the sixth is a rare spill.
      const r3 = minPair(kin.slice(0, 3), k);
      if (r3.min < kin3Min) kin3Min = r3.min;
    }
    // and across ALL slots: the worst pair anywhere on the wheel
    const all = [];
    for (let s = 0; s < F * OFFSETS; ++s) all.push(politySlotColour(s, F));
    const allMin = minPair(all, k);
    row[k] = {
      non_kin_min_dE: +nk.min.toFixed(1), non_kin_pair: nk.pair,
      kin_min_dE: +kinMin.toFixed(1), kin_pair: kinPair, kin_wedge: kinWedge,
      kin3_min_dE: +kin3Min.toFixed(1),
      any_min_dE: +allMin.min.toFixed(1), any_pair: allMin.pair,
    };
  }
  report.families[F] = row;
}

if (json) { console.log(JSON.stringify(report, null, 1)); process.exit(0); }

console.log('CVD check on the lapse polity palette (presentation.cpp polity_slot_colour), CIE76 dE, smallest pair');
console.log('reference: the twelve-slot nation table');
for (const k of kinds) console.log(`  ${k.padEnd(7)} min dE ${String(report.nation[k].min_dE).padStart(5)}  (slots ${report.nation[k].pair.join(' vs ')})`);
for (const F of families) {
  console.log(`families = ${F}  (${F * OFFSETS} slots)`);
  for (const k of kinds) {
    const r = report.families[F][k];
    console.log(`  ${k.padEnd(7)} non-kin wedge centres min dE ${String(r.non_kin_min_dE).padStart(5)} (wedges ${r.non_kin_pair.join(' vs ')})`
      + ` | kin first-three min dE ${String(r.kin3_min_dE).padStart(5)}, all six ${String(r.kin_min_dE).padStart(5)} (wedge ${r.kin_wedge}, offsets ${r.kin_pair.join(' vs ')})`
      + ` | any two slots min dE ${String(r.any_min_dE).padStart(5)} (slots ${r.any_pair.join(' vs ')})`);
  }
}
