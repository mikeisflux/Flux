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
};

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
for (const connector of connectors) {
  const own = overrides.get(connector.id);
  if (own) {
    entries.push([connector.id, own, 'branding/connectors']);
    continue;
  }
  const slug = SLUGS[connector.id];
  const icon = slug ? bySlug.get(slug) : undefined;
  if (icon) {
    entries.push([
      connector.id,
      {
        viewBox: '0 0 24 24',
        shapes: [{tag: 'path', attrs: {fill: `#${icon.hex}`, d: icon.path}}],
      },
      `simple-icons:${slug} (CC0-1.0)`,
    ]);
    continue;
  }

  const extra = ICONIFY[connector.id];
  if (extra) {
    const set = ICONIFY_SETS[extra.set];
    const found = set.icons[extra.name];
    if (!found) {
      throw new Error(`${extra.set}:${extra.name} is not in that set any more`);
    }
    const width = found.width ?? set.width ?? 24;
    const height = found.height ?? set.height ?? 24;
    entries.push([
      connector.id,
      {
        viewBox: `0 0 ${width} ${height}`,
        shapes: shapesFromIconifyBody(found.body),
      },
      `${extra.set}:${extra.name} (${extra.license})`,
    ]);
    continue;
  }

  missing.push(connector.id);
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
// ${entries.length} of ${connectors.length} connectors have a mark. The rest
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
