// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {shape, svgRoot} from './icons.js';

import {loadTemplates, transportLabel, trustLabel} from './catalog.js';
import type {Template} from './catalog.js';

/**
 * Workflows - saved, reusable tasks.
 *
 * The six examples are not a separate list: they are catalog templates that
 * ship with a schedule, which is the point the empty-state copy is making.
 * They are looked up by title so the two can never drift.
 */
const EXAMPLE_TITLES: string[] = [
  'Turn last month\'s email receipts into an expense report',
  'Log prices for products I\'m watching every day',
  'Summarize competitor product updates every week',
  'Read a company\'s strategy from its job postings monthly',
  'Queue my usual lunch order every workday',
  'Watch a booked-out restaurant for a table',
];

export class WorkflowsView {
  async render(root: HTMLElement) {
    const all = await loadTemplates();

    root.replaceChildren();
    root.classList.remove('two-column');

    const screen = document.createElement('div');
    screen.className = 'screen';

    const head = document.createElement('div');
    head.className = 'screen-head';
    const h1 = document.createElement('h1');
    h1.textContent = 'Workflows';
    const create = document.createElement('button');
    create.className = 'primary';
    create.textContent = 'New workflow';
    head.append(h1, create);

    screen.append(head, this.emptyState(), this.examples(all));
    root.append(screen);
  }

  private emptyState(): HTMLElement {
    const empty = document.createElement('div');
    empty.className = 'empty-state';
    const icon = svgRoot('0 0 32 32', 'empty-icon');
    shape(icon, 'rect', {x: 3, y: 11, width: 13, height: 12, rx: 3});
    shape(icon, 'rect', {x: 16, y: 7, width: 13, height: 12, rx: 3});
    empty.append(icon);

    const h2 = document.createElement('h2');
    h2.textContent = 'No workflows yet';

    const body = document.createElement('p');
    body.append('A workflow is a task you save once and reuse — run it ' +
                'on a schedule, or trigger it anytime with ');
    const code = document.createElement('code');
    code.textContent = '/command';
    body.append(code, '.');

    empty.append(h2, body);
    return empty;
  }

  private examples(all: Template[]): HTMLElement {
    const section = document.createElement('section');
    const h2 = document.createElement('h2');
    h2.className = 'section-title';
    h2.textContent = 'Start from an example';

    const grid = document.createElement('div');
    grid.className = 'card-grid';
    const byTitle = new Map(all.map(t => [t.title, t]));
    for (const title of EXAMPLE_TITLES) {
      const t = byTitle.get(title);
      if (t) {
        grid.append(this.card(t));
      }
    }

    const footer = document.createElement('div');
    footer.className = 'centered-footer';
    const browse = document.createElement('a');
    browse.className = 'outlined';
    // The Scheduled facet exists on the Tasks screen precisely so this link
    // has somewhere to land.
    browse.href = '#templates/tasks';
    browse.textContent = 'Browse all examples';
    footer.append(browse);

    section.append(h2, grid, footer);
    return section;
  }

  private card(t: Template): HTMLElement {
    const card = document.createElement('article');
    card.className = 'template-card';

    const title = document.createElement('h3');
    title.textContent = t.title;
    const outcome = document.createElement('p');
    outcome.textContent = t.outcome;

    const footer = document.createElement('div');
    footer.className = 'card-footer';

    const marks = document.createElement('div');
    marks.className = 'connector-marks';
    for (const c of t.connectors) {
      const mark = document.createElement('span');
      mark.className = 'connector-mark';
      mark.dataset['transport'] = c.transport;
      mark.textContent = c.id.slice(0, 1);
      mark.title = transportLabel(c.id, c.transport);
      marks.append(mark);
    }
    footer.append(marks);

    const trust = document.createElement('span');
    trust.className = 'trust';
    trust.dataset['scope'] = t.writeScope;
    trust.textContent = trustLabel(t.writeScope);
    footer.append(trust);

    if (t.schedule) {
      const chip = document.createElement('span');
      chip.className = 'schedule-chip';
      chip.textContent = t.schedule.display;
      chip.title = t.schedule.cron;
      footer.append(chip);
    }

    card.append(title, outcome, footer);
    return card;
  }
}
