// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {loadPackedJson, once} from './resource.js';

/**
 * The template catalog, as packed with the console.
 *
 * It is a static resource rather than a Mojo call because it does not change
 * at runtime and every screen that shows it wants the whole thing: filtering
 * 250 rows in the page is free, and a round trip per keystroke is not.
 */

export type Transport = 'api'|'browser'|'mcp';

export interface TemplateConnector {
  id: string;
  transport: Transport;
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
  // What actually gets sent to the agent. Anything in [square brackets] is a
  // blank for the user to fill in - the detail view lists them, because a
  // template run with "[sheet link]" still in it is a task that fails on its
  // first step.
  //
  // Optional while the 250 are being written. An unauthored template still
  // opens and still runs, on a starting point derived from its title - see
  // promptFor(). That is worse than an authored prompt and much better than an
  // empty box, and it is labelled so nobody mistakes one for the other.
  prompt?: string;
  roles?: string[];
}

/** True when a human wrote this template's prompt. */
export function hasAuthoredPrompt(template: Template): boolean {
  return Boolean(template.prompt);
}

/**
 * The prompt to run, authored or derived.
 *
 * The derived one states the outcome and names the services, which is enough
 * for the agent to attempt the task and enough for the user to see what is
 * missing. It deliberately reads as a first draft rather than as instructions.
 */
export function promptFor(template: Template): string {
  if (template.prompt) {
    return template.prompt;
  }
  const services = template.connectors.map(c => c.id).join(', ');
  return `${template.title}.\n\nWhat I want at the end: ${
      template.outcome}.${
      services ? `\n\nUse: ${services}.` : ''}\n\nAsk me for anything you need that I have not given you here - a sheet link, a date range, who to include - rather than guessing.`;
}

/** The [placeholders] in a prompt, in order, without their brackets. */
export function placeholders(prompt: string): string[] {
  return [...prompt.matchAll(/\[([^\]]+)\]/g)].map(m => m[1]!);
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

export const loadTemplates = once(
    () => loadPackedJson<{templates: Template[]}>('templates.json')
              .then(d => d.templates));

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

/**
 * Tooltip for a connector mark. Three screens draw these marks, and a
 * connector's transport is the one thing about it they all have to explain,
 * so the wording lives here rather than in three ternaries that drifted the
 * moment a third transport appeared.
 */
export function transportLabel(id: string, transport: Transport): string {
  switch (transport) {
    case 'api':
      return `${id} - direct API`;
    case 'mcp':
      return `${id} - over MCP`;
    default:
      return `${id} - driven in the browser`;
  }
}

/** One action the agent can take through a connector, as the console shows it. */
export interface ConnectorAction {
  name: string;
  label: string;
  writeScope: string;
}

/**
 * The actions each connector offers, keyed by connector id.
 *
 * Read out of connector_defs.json - the same packed file the browser process
 * parses - rather than sent over mojo. The list does not change while the
 * browser is running and the console wants all of it, which is the same reason
 * the other three catalogues are static resources.
 */
export const loadConnectorActions = once(async () => {
  const packed = await loadPackedJson<{
    connectors: Array<{
      id: string,
      operations?: Record<string, {label?: string, write_scope?: string}>,
    }>,
  }>('connector_defs.json');
  const byId = new Map<string, ConnectorAction[]>();
  for (const def of packed.connectors) {
    const actions: ConnectorAction[] = [];
    for (const [name, op] of Object.entries(def.operations ?? {})) {
      actions.push({
        name,
        // The label is required by check-connector-defs.py, so a missing one
        // is a build-time failure rather than something to paper over - but
        // falling back to the id beats rendering "undefined" if one slips.
        label: op.label || name,
        writeScope: op.write_scope || 'readonly',
      });
    }
    byId.set(def.id, actions);
  }
  return byId;
});
