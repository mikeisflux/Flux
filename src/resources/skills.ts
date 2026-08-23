// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {loadPackedJson, once} from './resource.js';

/**
 * The shipped skill library, packed from data/skills/*.md.
 *
 * Docs 06 settles the schema: freeform markdown with exactly one required
 * section, `When to use`, which is the retrieval trigger that makes "applied
 * automatically when relevant" work. Everything after it is authored to fit
 * the job rather than poured into a fixed template.
 */

export interface SkillConnector {
  id: string;
  transport: 'api'|'browser'|'mcp';
}

export interface Skill {
  command: string;
  name: string;
  description: string;
  categories: string[];
  roles: string[];
  worksWith: SkillConnector[];
  writeScope: 'readonly'|'draft'|'send'|'purchase';
  bodyStatus: 'authored'|'skeleton';
  whenToUse: string;
  body: string;
}

export const loadSkills = once(
    () => loadPackedJson<{skills: Skill[]}>('skills.json')
              .then(d => d.skills));

/**
 * Siblings, computed rather than curated - the reference's Related list is the
 * same three skills on every Data skill, which is what a similarity query
 * looks like, not a hand-authored link set.
 */
export function related(all: Skill[], skill: Skill, limit: number): Skill[] {
  const score = (other: Skill) => {
    if (other.command === skill.command) {
      return -1;
    }
    const shared = other.categories.filter(c => skill.categories.includes(c));
    const roles = other.roles.filter(r => skill.roles.includes(r));
    return shared.length * 2 + roles.length;
  };
  return all.filter(s => score(s) > 0)
      .sort((a, b) => score(b) - score(a))
      .slice(0, limit);
}

/**
 * Renders the markdown an agent actually produces.
 *
 * It started as the tiny slice the skill bodies use - headings, bullets, bold,
 * inline code - and that was wrong the moment the same function was pointed at
 * a run's answer. The 250 template prompts ask for tables ("a side-by-side of
 * every competitor's plans"), for verbatim quotes ("quote them exactly"), and
 * above all for links: "write it to a new Google Sheet and give me the link"
 * appears dozens of times, and `[Prospects](https://...)` was rendering as
 * literal square brackets.
 *
 * Still deliberately not a general markdown parser. This runs in a privileged
 * renderer, so every node is built with createElement and textContent and
 * nothing is ever assigned to innerHTML - which is also why link hrefs are
 * scheme-checked rather than trusted.
 */
export function renderMarkdown(source: string): DocumentFragment {
  const out = document.createDocumentFragment();
  let list: HTMLElement|null = null;
  let quote: HTMLElement|null = null;

  const flush = () => {
    if (list) {
      out.append(list);
      list = null;
    }
    if (quote) {
      out.append(quote);
      quote = null;
    }
  };

  const lines = source.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]!.trimEnd();

    // Fenced code. Consumed whole here rather than line by line, so that a
    // table or a heading inside a code block stays code.
    const fence = /^\s*```(\w*)\s*$/.exec(line);
    if (fence) {
      flush();
      const body: string[] = [];
      i++;
      while (i < lines.length && !/^\s*```\s*$/.test(lines[i]!)) {
        body.push(lines[i]!);
        i++;
      }
      const pre = document.createElement('pre');
      const code = document.createElement('code');
      if (fence[1]) {
        code.dataset['lang'] = fence[1];
      }
      code.textContent = body.join('\n');
      pre.append(code);
      out.append(pre);
      continue;
    }

    if (!line.trim()) {
      flush();
      continue;
    }

    // A table: a header row, a delimiter row of dashes, then body rows. The
    // delimiter is what distinguishes it from a line that merely contains a
    // pipe, so both are required before anything is treated as a table.
    if (line.includes('|') && i + 1 < lines.length &&
        /^\s*\|?[\s:|-]*-[\s:|-]*\|?\s*$/.test(lines[i + 1]!) &&
        lines[i + 1]!.includes('-')) {
      flush();
      const table = document.createElement('table');
      const thead = document.createElement('thead');
      thead.append(tableRow(line, 'th'));
      table.append(thead);
      const tbody = document.createElement('tbody');
      i += 2;
      while (i < lines.length && lines[i]!.includes('|') &&
             lines[i]!.trim()) {
        tbody.append(tableRow(lines[i]!, 'td'));
        i++;
      }
      i--;
      table.append(tbody);
      // Wrapped, because a twelve-column comparison table is wider than the
      // run view and the page must not scroll sideways because of it.
      const scroller = document.createElement('div');
      scroller.className = 'table-scroll';
      scroller.append(table);
      out.append(scroller);
      continue;
    }

    const heading = /^(#{1,4})\s+(.*)$/.exec(line);
    if (heading) {
      flush();
      const h = document.createElement(`h${heading[1]!.length + 1}`);
      h.append(inline(heading[2]!));
      out.append(h);
      continue;
    }

    if (/^\s*(?:---+|\*\*\*+|___+)\s*$/.test(line)) {
      flush();
      out.append(document.createElement('hr'));
      continue;
    }

    const quoted = /^\s*>\s?(.*)$/.exec(line);
    if (quoted) {
      if (list) {
        out.append(list);
        list = null;
      }
      if (!quote) {
        quote = document.createElement('blockquote');
      }
      const p = document.createElement('p');
      p.append(inline(quoted[1]!));
      quote.append(p);
      continue;
    }

    const bullet = /^\s*[-*]\s+(.*)$/.exec(line);
    const numbered = /^\s*\d+\.\s+(.*)$/.exec(line);
    if (bullet || numbered) {
      const wanted = bullet ? 'UL' : 'OL';
      if (!list || list.tagName !== wanted) {
        flush();
        list = document.createElement(bullet ? 'ul' : 'ol');
      }
      const li = document.createElement('li');
      li.append(inline((bullet ?? numbered)![1]!));
      list.append(li);
      continue;
    }

    flush();
    const p = document.createElement('p');
    p.append(inline(line));
    out.append(p);
  }

  flush();
  return out;
}

/** One row of a pipe table, split on unescaped pipes. */
function tableRow(line: string, cell: 'th'|'td'): HTMLTableRowElement {
  const row = document.createElement('tr');
  const trimmed = line.trim().replace(/^\|/, '').replace(/\|$/, '');
  for (const text of trimmed.split('|')) {
    const td = document.createElement(cell);
    td.append(inline(text.trim()));
    row.append(td);
  }
  return row;
}

/**
 * Only http, https and mailto are allowed through as links.
 *
 * The console is a privileged WebUI: a `javascript:` or `data:` href written
 * by a model - or by a page the model was reading when it copied a link out -
 * would run with chrome://flux's authority. Anything else renders as its own
 * text, so the user still sees the URL and nothing is silently dropped.
 */
function safeHref(url: string): string|null {
  try {
    const parsed = new URL(url, 'https://invalid.example');
    return ['http:', 'https:', 'mailto:'].includes(parsed.protocol)
        ? parsed.href
        : null;
  } catch {
    return null;
  }
}

/** Links, bold, and inline code. */
function inline(text: string): DocumentFragment {
  const frag = document.createDocumentFragment();
  const pattern = /\[([^\]]+)\]\(([^)\s]+)\)|\*\*(.+?)\*\*|`(.+?)`/g;
  let last = 0;
  for (let m = pattern.exec(text); m; m = pattern.exec(text)) {
    if (m.index > last) {
      frag.append(text.slice(last, m.index));
    }
    if (m[1] !== undefined) {
      const href = safeHref(m[2]!);
      if (href) {
        const a = document.createElement('a');
        a.href = href;
        a.textContent = m[1];
        a.target = '_blank';
        a.rel = 'noopener noreferrer';
        frag.append(a);
      } else {
        frag.append(`${m[1]} (${m[2]})`);
      }
    } else if (m[3] !== undefined) {
      const strong = document.createElement('strong');
      strong.textContent = m[3];
      frag.append(strong);
    } else {
      const code = document.createElement('code');
      code.textContent = m[4]!;
      frag.append(code);
    }
    last = m.index + m[0].length;
  }
  frag.append(text.slice(last));
  return frag;
}
