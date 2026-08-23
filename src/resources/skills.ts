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
 * Renders the tiny slice of markdown the skill bodies actually use: setext-free
 * ATX headings, bullets, numbered steps and bold runs. Deliberately not a
 * general markdown parser - this runs in a privileged renderer, so every node
 * is built with createElement and textContent and nothing is ever assigned to
 * innerHTML.
 */
export function renderMarkdown(source: string): DocumentFragment {
  const out = document.createDocumentFragment();
  let list: HTMLElement|null = null;

  const flush = () => {
    if (list) {
      out.append(list);
      list = null;
    }
  };

  for (const raw of source.split('\n')) {
    const line = raw.trimEnd();
    if (!line.trim()) {
      flush();
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

/** Bold and inline code, the only two inline forms the bodies use. */
function inline(text: string): DocumentFragment {
  const frag = document.createDocumentFragment();
  const pattern = /\*\*(.+?)\*\*|`(.+?)`/g;
  let last = 0;
  for (let m = pattern.exec(text); m; m = pattern.exec(text)) {
    if (m.index > last) {
      frag.append(text.slice(last, m.index));
    }
    if (m[1] !== undefined) {
      const strong = document.createElement('strong');
      strong.textContent = m[1];
      frag.append(strong);
    } else {
      const code = document.createElement('code');
      code.textContent = m[2]!;
      frag.append(code);
    }
    last = m.index + m[0].length;
  }
  frag.append(text.slice(last));
  return frag;
}
