// Generate src/resources/connector_icons.ts from Simple Icons.
//
// The connector marks are the only saturated pixels in the console - that is
// the whole reason the rest of the UI has no accent, so the eye lands on what
// a task touches. A grey monogram does not do that job.
//
// Simple Icons rather than each vendor's brand kit: its SVG path data is
// CC0, so it can live in this repo. The marks are still the trademarks of
// their owners, and are used here to identify the service each connector
// talks to - which is what they are for.
//
// Not every service is in it. Several were removed from Simple Icons on
// request from the trademark holder, and tracing a replacement by hand would
// be doing exactly the thing they asked not to be done. Those connectors fall
// back to a monogram tile, and a user who has the rights to a mark can drop
// one into branding/connectors/<id>.svg and it wins over both.
//
// Output is committed. The build machine needs no npm - same rule as
// branding/icons.
//
//   node tools/build-connector-icons.mjs

import * as si from 'simple-icons';
import logosSet from '@iconify-json/logos/icons.json' with {type: 'json'};
import mdiSet from '@iconify-json/mdi/icons.json' with {type: 'json'};
import {readFileSync, writeFileSync, existsSync, readdirSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {dirname, join} from 'node:path';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');

// connector id -> Simple Icons slug. Only where the mark actually exists;
// everything absent from here renders as a monogram on purpose.
const SLUGS = {
  google: 'google',
  slack: 'slack',
  imessage: 'imessage',
  quickbooks: 'quickbooks',
  notion: 'notion',
  linear: 'linear',
  stripe: 'stripe',
  atlassian: 'atlassian',
  asana: 'asana',
  sentry: 'sentry',
  cloudflare: 'cloudflare',
  zapier: 'zapier',
  webflow: 'webflow',
  github: 'github',
  hubspot: 'hubspot',
  calendly: 'calendly',
  airtable: 'airtable',
  salesforce: 'salesforce',
  brex: 'brex',
  cal_com: 'caldotcom',
  coda: 'coda',
  posthog: 'posthog',
  sanity: 'sanity',
  google_analytics: 'googleanalytics',
  instagram: 'instagram',
  facebook_pages: 'facebook',
  basecamp: 'basecamp',
};

// Marks Simple Icons does not have, from sets that are also redistributable:
// Gil Barbara's SVG Logos (CC0-1.0) and Material Design Icons (Apache-2.0).
//
// Nothing is mapped on a name match alone. logos:apollostack is Apollo GRAPHQL
// and this connector is Apollo.io, a sales tool; mdi:grain is a sheaf of
// wheat, not Grain the meeting recorder. A confidently wrong logo is worse
// than an honest monogram, so those stay monograms.
const ICONIFY = {
  outlook: {set: 'mdi', name: 'microsoft-outlook', license: 'Apache-2.0'},
  monday: {set: 'logos', name: 'monday-icon', license: 'CC0-1.0'},
  mondaycom: {set: 'logos', name: 'monday-icon', license: 'CC0-1.0'},
  chrome: {set: 'logos', name: 'chrome', license: 'CC0-1.0'},
  chromedevtools: {set: 'logos', name: 'chrome', license: 'CC0-1.0'},
};

// Names in templates.json and skills.json are display labels - "Docs",
// "Hacker News", "AT&T" - not catalogue ids, so normalising is not enough.
// Every entry here is a judgement that the mark is the RIGHT one, not merely
// a near-enough string:
//
//   Docs / Sheets      Google's, from the company they appear beside.
//   Hacker News        HN is a Y Combinator property and flies the orange Y.
//   Chrome DevTools    part of Chrome; Chrome's mark is accurate, not a stand-in.
//   Analytics          Google Analytics - the card is a dashboards sweep.
//
// Deliberately absent: Apollo (Apollo.io, and the only Apollo mark available
// is Apollo GraphQL's), Grain, Gong, Outreach, Ahrefs, Ashby, OpenTable, Resy,
// Kayak, Zocdoc, TodayTix, Vivid Seats, and every government registry. A logo
// that belongs to a different company is worse than the letter it replaces.
const ALIASES = {
  docs: 'googledocs',
  sheets: 'googlesheets',
  hackernews: 'ycombinator',
  acrobat: 'adobeacrobatreader',
  att: 'atandt',
  analytics: 'googleanalytics',
  googleanalytics: 'googleanalytics',
  facebookpages: 'facebook',
  superhumanmail: 'superhuman',
  calcom: 'caldotcom',
};

/** Lowercase alphanumerics only, so "Google Drive" and "google_drive" agree. */
function slugify(name) {
  return name.toLowerCase().replace(/[^a-z0-9]/g, '');
}

const ICONIFY_SETS = {logos: logosSet, mdi: mdiSet};

/**
 * Iconify stores an icon as a fragment of SVG markup. It is turned into a list
 * of shapes here rather than shipped as a string, because the console cannot
 * assign markup - Trusted Types blocks innerHTML - and builds every node with
 * createElementNS instead.
 */
function shapesFromIconifyBody(body) {
  const shapes = [];
  // A single wrapping <g> carries a transform that its children need. Lift it
  // onto each child rather than modelling groups.
  let inherited = {};
  const group = body.match(/^\s*<g\s([^>]*)>([\s\S]*)<\/g>\s*$/);
  let markup = body;
  if (group) {
    inherited = attributesOf(group[1]);
    markup = group[2];
  }
  for (const match of markup.matchAll(
           /<(path|circle|rect|ellipse|polygon|polyline)\s([^>]*?)\/?>/g)) {
    shapes.push({
      tag: match[1],
      attrs: {...inherited, ...attributesOf(match[2])},
    });
  }
  return shapes;
}

function attributesOf(text) {
  const attrs = {};
  for (const m of text.matchAll(/([a-zA-Z-]+)="([^"]*)"/g)) {
    attrs[m[1]] = m[2];
  }
  return attrs;
}

const bySlug = new Map();
for (const key of Object.keys(si)) {
  const icon = si[key];
  if (icon && icon.slug) bySlug.set(icon.slug, icon);
}

const connectors =
    JSON.parse(readFileSync(join(ROOT, 'src/resources/connectors.json'), 'utf8'))
        .connectors;

// The catalogue's 40 ids are not the only things the console draws a mark for.
// Template and skill cards name the services they touch by DISPLAY LABEL -
// "Docs", "Hacker News", "AT&T" - so a table keyed on catalogue ids missed
// every card chip in the product, which is most of them. Both vocabularies are
// resolved to the same slug and share one table.
const referenced = new Map();
for (const connector of connectors) {
  referenced.set(slugify(connector.id), connector.id);
}
for (const file of ['templates.json', 'skills.json']) {
  const data =
      JSON.parse(readFileSync(join(ROOT, 'src/resources', file), 'utf8'));
  for (const row of data.templates ?? data.skills ?? []) {
    for (const used of row.connectors ?? row.worksWith ?? []) {
      if (used && used.id) referenced.set(slugify(used.id), used.id);
    }
  }
}

// A mark the user supplied themselves wins: they own the rights decision.
const OVERRIDE_DIR = join(ROOT, 'branding/connectors');
const overrides = new Map();
if (existsSync(OVERRIDE_DIR)) {
  for (const file of readdirSync(OVERRIDE_DIR)) {
    if (!file.endsWith('.svg')) continue;
    const svg = readFileSync(join(OVERRIDE_DIR, file), 'utf8');
    const paths = [...svg.matchAll(/\bd="([^"]+)"/g)].map(m => m[1]);
    if (!paths.length) {
      console.error(`    SKIP  ${file} has no <path d="...">`);
      continue;
    }
    const viewBox = (svg.match(/viewBox="([^"]+)"/) || [, '0 0 24 24'])[1];
    const hex = (svg.match(/fill="#([0-9a-fA-F]{6})"/) || [, '000000'])[1];
    overrides.set(file.replace(/\.svg$/, ''), {
      viewBox,
      shapes: paths.map(d => ({tag: 'path', attrs: {fill: `#${hex}`, d}})),
    });
  }
}

const entries = [];
const missing = [];
for (const [slugKey, label] of [...referenced].sort()) {
  const connector = {id: label};
  const own = overrides.get(label) ?? overrides.get(slugKey);
  if (own) {
    entries.push([slugKey, own, 'branding/connectors']);
    continue;
  }
  const slug = SLUGS[label] ?? ALIASES[slugKey] ?? slugKey;
  const icon = slug ? bySlug.get(slug) : undefined;
  if (icon) {
    entries.push([
      slugKey,
      {
        viewBox: '0 0 24 24',
        shapes: [{tag: 'path', attrs: {fill: `#${icon.hex}`, d: icon.path}}],
      },
      `simple-icons:${slug} (CC0-1.0)`,
    ]);
    continue;
  }

  const extra = ICONIFY[slugKey] ?? ICONIFY[slug];
  if (extra) {
    const set = ICONIFY_SETS[extra.set];
    const found = set.icons[extra.name];
    if (!found) {
      throw new Error(`${extra.set}:${extra.name} is not in that set any more`);
    }
    const width = found.width ?? set.width ?? 24;
    const height = found.height ?? set.height ?? 24;
    entries.push([
      slugKey,
      {
        viewBox: `0 0 ${width} ${height}`,
        shapes: shapesFromIconifyBody(found.body),
      },
      `${extra.set}:${extra.name} (${extra.license})`,
    ]);
    continue;
  }

  missing.push(label);
}

const body = entries
    .map(([id, icon, source]) => {
      const shapes = icon.shapes
          .map(s => `      {tag: ${JSON.stringify(s.tag)}, attrs: ${
                        JSON.stringify(s.attrs)}},`)
          .join('\n');
      return `  // ${source}\n  ${JSON.stringify(id)}: {\n` +
          `    viewBox: ${JSON.stringify(icon.viewBox)},\n` +
          `    shapes: [\n${shapes}\n    ],\n  },`;
    })
    .join('\n');

const out = `// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.
//
// GENERATED by tools/build-connector-icons.mjs - do not edit by hand.
//
// Brand marks for the connector catalogue. Path data from Simple Icons, which
// is CC0; the marks themselves remain the trademarks of their owners and are
// used to identify the service each connector talks to.
//
// ${entries.length} of ${referenced.size} services referenced by the console
// have a mark - the connector catalogue plus every service a template or
// skill card names. The rest
// render as a monogram, because their mark is not in a source this repo can
// redistribute. To give one a real logo, put an SVG at
// branding/connectors/<id>.svg and re-run the generator.
//
// Missing: ${missing.length ? missing.join(', ') : 'none'}

export interface BrandShape {
  tag: string;
  attrs: Record<string, string>;
}

export interface BrandMark {
  viewBox: string;
  shapes: BrandShape[];
}

export const BRAND_MARKS: Record<string, BrandMark> = {
${body}
};
`;

writeFileSync(join(ROOT, 'src/resources/connector_icons.ts'), out);
console.log(`    ${entries.length} marks, ${missing.length} monogram fallbacks`);
if (missing.length) console.log(`    no mark: ${missing.join(', ')}`);
