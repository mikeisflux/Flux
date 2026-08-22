// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

/**
 * The template catalog, as packed with the console.
 *
 * It is a static resource rather than a Mojo call because it does not change
 * at runtime and every screen that shows it wants the whole thing: filtering
 * 250 rows in the page is free, and a round trip per keystroke is not.
 */

export interface TemplateConnector {
  id: string;
  transport: 'api'|'browser';
}

export interface TemplateSchedule {
  cron: string;
  display: string;
}

export interface Template {
  id: string;
  title: string;
  outcome: string;
  category: string;
  connectors: TemplateConnector[];
  schedule: TemplateSchedule|null;
  writeScope: 'readonly'|'draft'|'send'|'purchase';
  featured?: number;
}

/**
 * Section order is fixed by the reference and is not alphabetical, so it is
 * declared rather than derived.
 */
export const CATEGORIES: string[] = [
  'Sales',
  'Recruiting',
  'Marketing',
  'Data',
  'Research',
  'Ops',
  'Engineering',
  'Docs',
  'Personal',
  'Monitoring',
];

let cached: Promise<Template[]>|null = null;

export function loadTemplates(): Promise<Template[]> {
  if (!cached) {
    cached = fetch('templates.json')
                 .then(r => r.json())
                 .then((d: {templates: Template[]}) => d.templates);
  }
  return cached;
}

export function featured(all: Template[]): Template[] {
  return all.filter(t => t.featured !== undefined)
      .sort((a, b) => a.featured! - b.featured!);
}

/**
 * Matches title, outcome, category and connector id, because the reference's
 * placeholder advertises searching by site and role, not just task text.
 */
export function matches(t: Template, query: string): boolean {
  if (!query) {
    return true;
  }
  const q = query.toLowerCase();
  return t.title.toLowerCase().includes(q) ||
      t.outcome.toLowerCase().includes(q) ||
      t.category.toLowerCase().includes(q) ||
      t.connectors.some(c => c.id.toLowerCase().includes(q));
}

/**
 * What the task is allowed to do, stated on the card.
 *
 * The reference gives you no way to tell "drafts a reply" from "sends a reply"
 * without reading the description prose, which is exactly the distinction a
 * user needs before running something unattended.
 */
export function trustLabel(scope: Template['writeScope']): string {
  switch (scope) {
    case 'readonly':
      return 'Reads only';
    case 'draft':
      return 'Writes drafts';
    case 'send':
      return 'Sends';
    case 'purchase':
      return 'Spends money';
  }
}
