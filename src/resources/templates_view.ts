// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {CATEGORIES, featured, loadTemplates, matches, trustLabel} from
    './catalog.js';
import type {Template} from './catalog.js';

/**
 * Templates - the task library.
 *
 * Two columns: a nav rail (Tasks / Skills) and the catalog. Everything below
 * the search box is derived from one filtered list, so a query, a category
 * pill and the scheduled-only toggle all compose instead of overriding each
 * other.
 */
export class TemplatesView {
  private all: Template[] = [];
  private query = '';
  private category = 'All';
  private scheduledOnly = false;
  private seeAll: string|null = null;
  private grid!: HTMLElement;

  async render(root: HTMLElement, section: 'tasks'|'skills') {
    this.all = await loadTemplates();

    root.replaceChildren();
    root.classList.add('two-column');

    const rail = document.createElement('nav');
    rail.className = 'sub-nav';
    rail.setAttribute('aria-label', 'Templates');
    const railTitle = document.createElement('h2');
    railTitle.className = 'sub-nav-title';
    railTitle.textContent = 'Templates';
    rail.append(railTitle);
    for (const [id, label] of [['tasks', 'Tasks'], ['skills', 'Skills']]) {
      const a = document.createElement('a');
      a.className = 'sub-nav-item';
      a.textContent = label!;
      a.href = `#templates/${id}`;
      if (id === section) {
        a.setAttribute('aria-current', 'page');
      }
      rail.append(a);
    }

    const main = document.createElement('div');
    main.className = 'screen';
    root.append(rail, main);

    if (section === 'skills') {
      this.renderSkillsPlaceholder(main);
      return;
    }
    this.renderTasks(main);
  }

  private renderSkillsPlaceholder(main: HTMLElement) {
    const h1 = document.createElement('h1');
    h1.textContent = 'Skills';
    const p = document.createElement('p');
    p.className = 'subtitle';
    p.textContent =
        'Know-how the agent applies on its own when a task calls for it. ' +
        'Manage yours under Customize.';
    main.append(h1, p);
  }

  private renderTasks(main: HTMLElement) {
    const h1 = document.createElement('h1');
    h1.textContent = 'Tasks';

    const subtitle = document.createElement('p');
    subtitle.className = 'subtitle';
    subtitle.textContent =
        'Browse what Flux can do. Use a task as a starting point, or save it ' +
        'as a workflow that runs on a schedule.';

    const search = document.createElement('div');
    search.className = 'search';
    search.innerHTML =
        '<svg viewBox="0 0 20 20" aria-hidden="true">' +
        '<circle cx="9" cy="9" r="5.5"/><path d="M13 13l4 4"/></svg>';
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Search tasks, sites, roles...';
    input.addEventListener('input', () => {
      this.query = input.value;
      // A query is a search across everything, so it clears the drill-down
      // rather than searching inside it.
      this.seeAll = null;
      this.paint();
    });
    search.append(input);

    const pills = document.createElement('div');
    pills.className = 'pill-row';
    for (const name of ['All', ...CATEGORIES]) {
      const b = document.createElement('button');
      b.className = 'pill';
      b.textContent = name;
      b.dataset['category'] = name;
      b.addEventListener('click', () => {
        this.category = name;
        this.seeAll = null;
        this.paint();
      });
      pills.append(b);
    }

    const facets = document.createElement('div');
    facets.className = 'facet-row';
    const scheduled = document.createElement('button');
    scheduled.className = 'facet';
    scheduled.id = 'facet-scheduled';
    scheduled.innerHTML =
        '<svg viewBox="0 0 20 20" aria-hidden="true">' +
        '<path d="M4 7h9V4l4 4-4 4V9H6v2H4V7zm12 6H7v3l-4-4 4-4v2h11v3h-2z"/>' +
        '</svg>';
    scheduled.append('Scheduled');
    scheduled.addEventListener('click', () => {
      this.scheduledOnly = !this.scheduledOnly;
      this.paint();
    });
    facets.append(scheduled);

    this.grid = document.createElement('div');
    this.grid.className = 'sections';

    main.append(h1, subtitle, search, pills, facets, this.grid);
    this.paint();
  }

  private filtered(): Template[] {
    return this.all.filter(
        t => matches(t, this.query) &&
            (this.category === 'All' || t.category === this.category) &&
            (!this.scheduledOnly || t.schedule !== null));
  }

  private paint() {
    for (const pill of document.querySelectorAll<HTMLElement>('.pill')) {
      pill.toggleAttribute(
          'data-active', pill.dataset['category'] === this.category);
    }
    const scheduled = document.getElementById('facet-scheduled');
    scheduled?.toggleAttribute('data-active', this.scheduledOnly);

    const rows = this.filtered();
    this.grid.replaceChildren();

    if (rows.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'subtitle';
      empty.textContent = 'Nothing matches that.';
      this.grid.append(empty);
      return;
    }

    // Drilled into one category, or filtered down to one: show the whole thing
    // rather than the first six and a See-all that goes nowhere new.
    if (this.seeAll || this.category !== 'All' || this.query ||
        this.scheduledOnly) {
      const name = this.seeAll ?? this.category;
      this.grid.append(this.section(
          name === 'All' ? 'Results' : name, rows, /*truncate=*/false));
      return;
    }

    this.grid.append(
        this.section('Featured', featured(this.all), /*truncate=*/false));
    for (const category of CATEGORIES) {
      const inCategory = rows.filter(t => t.category === category);
      if (inCategory.length > 0) {
        this.grid.append(this.section(category, inCategory, /*truncate=*/true));
      }
    }
  }

  private section(name: string, rows: Template[], truncate: boolean):
      HTMLElement {
    const section = document.createElement('section');

    const header = document.createElement('div');
    header.className = 'section-header';
    const title = document.createElement('h2');
    title.textContent = name;
    header.append(title);

    // Featured has no count and no See-all in the reference; it is a hand-
    // picked row, not a slice of a larger set.
    if (truncate) {
      const count = document.createElement('span');
      count.className = 'muted';
      count.textContent = String(rows.length);
      header.append(count);

      const all = document.createElement('button');
      all.className = 'see-all';
      all.textContent = 'See all →';
      all.addEventListener('click', () => {
        this.seeAll = name;
        this.paint();
      });
      header.append(all);
    }

    const grid = document.createElement('div');
    grid.className = 'card-grid';
    for (const t of (truncate ? rows.slice(0, 6) : rows)) {
      grid.append(this.card(t));
    }

    section.append(header, grid);
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

    // Connector marks. Brand icons are not shipped, so each is a lettered chip
    // whose tooltip names the service and how it is reached - which the
    // reference's identical icons never told you.
    const marks = document.createElement('div');
    marks.className = 'connector-marks';
    for (const c of t.connectors.slice(0, 3)) {
      const mark = document.createElement('span');
      mark.className = 'connector-mark';
      mark.dataset['transport'] = c.transport;
      mark.textContent = c.id.slice(0, 1);
      mark.title = c.transport === 'api' ?
          `${c.id} - direct API` :
          `${c.id} - driven in the browser`;
      marks.append(mark);
    }
    if (t.connectors.length > 3) {
      const more = document.createElement('span');
      more.className = 'connector-more';
      more.textContent = `+${t.connectors.length - 3}`;
      marks.append(more);
    }
    footer.append(marks);

    // [FLUX] The reference makes you read the prose to find out whether a task
    // sends. It is on the card here, always.
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
