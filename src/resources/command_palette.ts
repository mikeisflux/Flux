// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {loadTemplates} from './catalog.js';
import type {Template} from './catalog.js';
import {loadConnectors} from './connectors_view.js';
import type {Connector} from './connectors_view.js';
import {loadSkills} from './skills.js';
import type {Skill} from './skills.js';

/**
 * The command panel behind Ctrl+K and the titlebar's magnifier.
 *
 * Everything the product can do shares one namespace, so the panel searches
 * all of it at once: screens, 250 templates, 134 skills, 38 connectors.
 *
 * [FLUX] The reference has skills, workflows and templates in one flat
 * namespace with three different naming conventions between them - which makes
 * a typing interface unusable and forces fuzzy search. Entries here are
 * prefixed by kind, so `skill:` narrows to skills and `/` still finds a
 * command by name.
 */
interface Command {
  kind: 'screen'|'task'|'skill'|'connector';
  label: string;
  detail: string;
  handle: string;
  href: string;
}

const SCREENS: Command[] = [
  {kind: 'screen', label: 'New task', detail: 'Describe a task to run',
   handle: 'new-task', href: '#new-task'},
  {kind: 'screen', label: 'Templates', detail: '250 tasks to start from',
   handle: 'templates', href: '#templates/tasks'},
  {kind: 'screen', label: 'Workflows', detail: 'Saved, reusable tasks',
   handle: 'workflows', href: '#workflows'},
  {kind: 'screen', label: 'Connectors', detail: 'Direct lines into apps',
   handle: 'connectors', href: '#connectors'},
  {kind: 'screen', label: 'Instructions',
   detail: 'What Flux knows on every task', handle: 'instructions',
   href: '#customize/instructions'},
  {kind: 'screen', label: 'Skills', detail: 'Know-how the agent applies',
   handle: 'skills', href: '#customize/skills'},
  {kind: 'screen', label: 'Settings', detail: 'Profiles and preferences',
   handle: 'settings', href: '#settings'},
];

export class CommandPalette {
  private commands: Command[] = [];
  private results!: HTMLElement;
  private rows: Command[] = [];
  private cursor = 0;

  async render(root: HTMLElement) {
    const [templates, skills, connectors] = await Promise.all(
        [loadTemplates(), loadSkills(), loadConnectors()]);
    this.commands = [
      ...SCREENS,
      ...templates.map(toTaskCommand),
      ...skills.map(toSkillCommand),
      ...connectors.map(toConnectorCommand),
    ];

    root.replaceChildren();
    root.classList.remove('two-column', 'four-column');

    const screen = document.createElement('div');
    screen.className = 'screen palette';

    const search = document.createElement('div');
    search.className = 'search';
    search.innerHTML =
        '<svg viewBox="0 0 20 20" aria-hidden="true">' +
        '<circle cx="9" cy="9" r="5.5"/><path d="M13 13l4 4"/></svg>';
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder =
        'Search everything - a task, a skill, a screen. Try skill:';
    input.addEventListener('input', () => this.paint(input.value));
    input.addEventListener('keydown', event => this.onKey(event));
    search.append(input);

    this.results = document.createElement('div');
    this.results.className = 'palette-results';

    screen.append(search, this.results);
    root.append(screen);
    this.paint('');
    input.focus();
  }

  private onKey(event: KeyboardEvent) {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault();
      const step = event.key === 'ArrowDown' ? 1 : -1;
      this.cursor = Math.max(
          0, Math.min(this.rows.length - 1, this.cursor + step));
      this.highlight();
      return;
    }
    if (event.key === 'Enter') {
      event.preventDefault();
      const row = this.rows[this.cursor];
      if (row) {
        window.location.hash = row.href;
      }
    }
  }

  private paint(query: string) {
    // `skill:` and the rest narrow by kind; anything else is a plain match.
    let kind: Command['kind']|null = null;
    let text = query.trim().toLowerCase();
    const prefix = /^(screen|task|skill|connector):\s*/.exec(text);
    if (prefix) {
      kind = prefix[1] as Command['kind'];
      text = text.slice(prefix[0].length);
    }

    this.rows = this.commands
                    .filter(c => (!kind || c.kind === kind) &&
                                (!text || c.label.toLowerCase().includes(text) ||
                                 c.handle.includes(text) ||
                                 c.detail.toLowerCase().includes(text)))
                    .slice(0, 60);
    this.cursor = 0;

    this.results.replaceChildren();
    if (this.rows.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'subtitle';
      empty.textContent = 'Nothing matches that.';
      this.results.append(empty);
      return;
    }

    for (const [i, c] of this.rows.entries()) {
      const row = document.createElement('a');
      row.className = 'palette-row';
      row.href = c.href;
      row.dataset['index'] = String(i);

      const kindTag = document.createElement('span');
      kindTag.className = 'palette-kind';
      kindTag.textContent = c.kind;

      const label = document.createElement('span');
      label.className = 'palette-label';
      label.textContent = c.label;

      const detail = document.createElement('span');
      detail.className = 'palette-detail';
      detail.textContent = c.detail;

      row.append(kindTag, label, detail);
      this.results.append(row);
    }
    this.highlight();
  }

  private highlight() {
    for (const row of
             this.results.querySelectorAll<HTMLElement>('.palette-row')) {
      row.toggleAttribute(
          'data-cursor', Number(row.dataset['index']) === this.cursor);
    }
  }
}

function toTaskCommand(t: Template): Command {
  return {
    kind: 'task',
    label: t.title,
    detail: t.outcome,
    handle: t.id,
    href: '#templates/tasks',
  };
}

function toSkillCommand(s: Skill): Command {
  return {
    kind: 'skill',
    label: s.name,
    detail: s.description,
    handle: s.command,
    href: '#customize/skills',
  };
}

function toConnectorCommand(c: Connector): Command {
  return {
    kind: 'connector',
    label: c.name,
    detail: c.description,
    handle: c.id,
    href: '#connectors',
  };
}
