// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {connectorMark, pathIcon, searchIcon} from './icons.js';

import {CATEGORIES, featured, loadTemplates, matches, transportLabel, trustLabel} from
    './catalog.js';
import type {Template} from './catalog.js';

import {loadSkills} from './skills.js';

import {TemplateDialog} from './template_dialog.js';
import {WorkflowDialog} from './workflow_dialog.js';
import {WriteScope} from './flux.mojom-webui.js';
import type {FluxPageHandlerRemote, UserTemplate} from './flux.mojom-webui.js';
import type {Skill} from './skills.js';

/**
 * Templates - the task library.
 *
 * Two columns: a nav rail (Tasks / Skills) and the catalog. Everything below
 * the search box is derived from one filtered list, so a query, a category
 * pill and the scheduled-only toggle all compose instead of overriding each
 * other.
 */
export class TemplatesView {
  private detail: TemplateDialog;
  private workflowDialog: WorkflowDialog;
  private all: Template[] = [];
  private mine: Template[] = [];
  private query = '';
  private category = 'All';
  private scheduledOnly = false;
  private seeAll: string|null = null;
  private grid!: HTMLElement;

  // The Skills tab keeps its own query, category and drill-down. They are the
  // same controls over a different catalogue, and carrying a task search into
  // the skill list - or the reverse - reads as the filter having been ignored.
  private skills: Skill[] = [];
  private skillQuery = '';
  private skillCategory = 'All';
  private skillSeeAll: string|null = null;
  private skillGrid!: HTMLElement;

  constructor(private handler: FluxPageHandlerRemote) {
    this.workflowDialog = new WorkflowDialog(handler, () => {
      window.location.hash = '#workflows';
    });
    this.detail = new TemplateDialog(
        (template, prompt) => this.useTemplate(template, prompt),
        (template, prompt) => this.saveAsWorkflow(template, prompt));
  }

  /**
   * Hands the prompt to New task rather than starting it here. The user has
   * just edited it, and a template that runs the instant it is chosen gives
   * them nowhere to check the blanks they filled in.
   */
  private useTemplate(template: Template, prompt: string) {
    sessionStorage.setItem(
        'flux.pendingTask',
        JSON.stringify({prompt, templateId: template.id}));
    window.location.hash = '#new-task';
  }

  private saveAsWorkflow(template: Template, prompt: string) {
    this.workflowDialog.open({
      command: template.id,
      name: template.title,
      description: template.outcome,
      instructions: prompt,
      cron: template.schedule?.cron ?? '',
      scheduleDisplay: template.schedule?.display ?? '',
      templateId: template.id,
      writeScope: SCOPE_BY_NAME[template.writeScope],
    });
  }

  /** Same category, minus itself. Enough to be useful, cheap to compute. */
  private relatedTo(template: Template): Template[] {
    return this.all
        .filter(t => t.category === template.category && t.id !== template.id)
        .slice(0, 3);
  }

  async render(root: HTMLElement, section: 'tasks'|'skills') {
    this.all = await loadTemplates();
    await this.loadMine();

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
      this.skills = await loadSkills();
      this.renderSkills(main);
      return;
    }
    this.renderTasks(main);
  }

  private renderSkills(main: HTMLElement) {
    const h1 = document.createElement('h1');
    h1.textContent = 'Skills';

    const subtitle = document.createElement('p');
    subtitle.className = 'subtitle';
    subtitle.textContent =
        'Know-how the agent applies on its own when a task calls for it. ' +
        'Adopt one to make it yours, and manage what you have adopted under ' +
        'Customize.';

    const search = document.createElement('div');
    search.className = 'search';
    search.append(searchIcon());
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Search skills, sites, roles...';
    input.value = this.skillQuery;
    input.addEventListener('input', () => {
      this.skillQuery = input.value;
      this.skillSeeAll = null;
      this.paintSkills();
    });
    search.append(input);

    const pills = document.createElement('div');
    pills.className = 'pill-row';
    for (const name of ['All', ...CATEGORIES]) {
      const b = document.createElement('button');
      b.className = 'pill skill-pill';
      b.textContent = name;
      b.dataset['category'] = name;
      b.addEventListener('click', () => {
        this.skillCategory = name;
        this.skillSeeAll = null;
        this.paintSkills();
      });
      pills.append(b);
    }

    this.skillGrid = document.createElement('div');
    this.skillGrid.className = 'sections';

    main.append(h1, subtitle, search, pills, this.skillGrid);
    this.paintSkills();
  }

  /**
   * Title, description, category, role and connector id - the same fields the
   * task search covers, because "find me the LinkedIn one" and "find me the
   * recruiting one" are both things a person types into a box labelled
   * skills, sites, roles.
   */
  private skillMatches(skill: Skill): boolean {
    if (!this.skillQuery) {
      return true;
    }
    const q = this.skillQuery.toLowerCase();
    return skill.name.toLowerCase().includes(q) ||
        skill.description.toLowerCase().includes(q) ||
        skill.command.toLowerCase().includes(q) ||
        skill.categories.some(c => c.toLowerCase().includes(q)) ||
        skill.roles.some(r => r.toLowerCase().includes(q)) ||
        skill.worksWith.some(c => c.id.toLowerCase().includes(q));
  }

  private filteredSkills(): Skill[] {
    return this.skills.filter(
        s => this.skillMatches(s) &&
            (this.skillCategory === 'All' ||
             s.categories.includes(this.skillCategory)));
  }

  private paintSkills() {
    for (const pill of document.querySelectorAll<HTMLElement>('.skill-pill')) {
      const active = pill.dataset['category'] === this.skillCategory;
      // data-active is a CSS hook and announces nothing. Which filter is on is
      // the only thing that distinguishes these buttons from each other, so it
      // has to be set in both places or the two states disagree.
      pill.toggleAttribute('data-active', active);
      pill.setAttribute('aria-pressed', String(active));
    }

    const rows = this.filteredSkills();
    this.skillGrid.replaceChildren();

    if (rows.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'subtitle';
      empty.textContent = 'Nothing matches that.';
      this.skillGrid.append(empty);
      return;
    }

    if (this.skillSeeAll || this.skillCategory !== 'All' || this.skillQuery) {
      const name = this.skillSeeAll ?? this.skillCategory;
      this.skillGrid.append(this.skillSection(
          name === 'All' ? 'Results' : name, rows, /*truncate=*/false));
      return;
    }

    // A skill can sit in more than one category, so it can appear under more
    // than one heading. That is the point of the categories, not a bug: a
    // person browsing Recruiting should see the skill that is also Ops.
    for (const category of CATEGORIES) {
      const inCategory = rows.filter(s => s.categories.includes(category));
      if (inCategory.length > 0) {
        this.skillGrid.append(
            this.skillSection(category, inCategory, /*truncate=*/true));
      }
    }
  }

  private skillSection(name: string, rows: Skill[], truncate: boolean):
      HTMLElement {
    const section = document.createElement('section');

    const header = document.createElement('div');
    header.className = 'section-header';
    const title = document.createElement('h2');
    title.textContent = name;
    header.append(title);

    const count = document.createElement('span');
    count.className = 'muted';
    count.textContent = String(rows.length);
    header.append(count);

    if (truncate && rows.length > 6) {
      const all = document.createElement('button');
      all.className = 'see-all';
      all.textContent = 'See all \u2192';
      all.addEventListener('click', () => {
        this.skillSeeAll = name;
        this.paintSkills();
      });
      header.append(all);
    }

    const grid = document.createElement('div');
    grid.className = 'card-grid';
    for (const skill of (truncate ? rows.slice(0, 6) : rows)) {
      grid.append(this.skillCard(skill));
    }

    section.append(header, grid);
    return section;
  }

  private skillCard(skill: Skill): HTMLElement {
    const card = document.createElement('article');
    card.className = 'template-card';

    const title = document.createElement('h3');
    title.textContent = skill.name;

    const description = document.createElement('p');
    description.textContent = skill.description;

    const footer = document.createElement('div');
    footer.className = 'card-footer';

    const marks = document.createElement('div');
    marks.className = 'connector-marks';
    for (const connector of skill.worksWith.slice(0, 3)) {
      const mark = connectorMark(connector.id, 'connector-mark');
      mark.dataset['transport'] = connector.transport;
      mark.title = transportLabel(connector.id, connector.transport);
      marks.append(mark);
    }
    if (skill.worksWith.length > 3) {
      const more = document.createElement('span');
      more.className = 'connector-more';
      more.textContent = `+${skill.worksWith.length - 3}`;
      marks.append(more);
    }
    footer.append(marks);

    // Same as a task card: whether it can send is on the card, not buried in
    // the body text.
    const trust = document.createElement('span');
    trust.className = 'trust';
    trust.dataset['scope'] = skill.writeScope;
    trust.textContent = trustLabel(skill.writeScope);
    footer.append(trust);

    // The command is how the skill is invoked by hand, and it is the only
    // thing on the card that is not prose.
    const command = document.createElement('span');
    command.className = 'schedule-chip';
    command.textContent = skill.command;
    command.title = skill.whenToUse;
    footer.append(command);

    card.append(title, description, footer);
    return card;
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
    search.append(searchIcon());
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Search tasks, sites, roles...';
    // Restored from state, not left blank. This view outlives its DOM - the
    // screen is rebuilt on every visit while the instance keeps the filters -
    // so a fresh empty box over a surviving query is a screen that shows two
    // of two hundred and fifty results and gives no reason why.
    input.value = this.query;
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
    scheduled.append(
        pathIcon('M4 7h9V4l4 4-4 4V9H6v2H4V7zm12 6H7v3l-4-4 4-4v2h11v3h-2z'));
    scheduled.append('Scheduled');
    scheduled.setAttribute('aria-pressed', 'false');
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

  /**
   * Templates the user or the assistant made, as cards beside the shipped 250.
   *
   * They come over mojo rather than out of the packed catalogue because they
   * are the only mutable half - templates.json is a resource baked into the
   * binary, so there has never been anywhere for a new one to go.
   */
  private async loadMine() {
    const {templates} = await this.handler.listUserTemplates();
    this.mine = templates.map((t: UserTemplate) => ({
      id: t.id,
      title: t.title,
      outcome: t.outcome,
      category: t.category,
      connectors: [],
      schedule: null,
      writeScope: SCOPE_NAME[t.writeScope] ?? 'readonly',
      prompt: t.prompt,
    } as Template));
  }

  private paint() {
    for (const pill of document.querySelectorAll<HTMLElement>('.pill')) {
      const active = pill.dataset['category'] === this.category;
      pill.toggleAttribute('data-active', active);
      pill.setAttribute('aria-pressed', String(active));
    }
    const scheduled = document.getElementById('facet-scheduled');
    scheduled?.toggleAttribute('data-active', this.scheduledOnly);
    scheduled?.setAttribute('aria-pressed', String(this.scheduledOnly));

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

    // Yours first. A template someone made for themselves outranks one that
    // came in the box, and burying it under six categories of shipped ones is
    // how a feature stops being used.
    if (this.mine.length > 0) {
      this.grid.append(
          this.section('Yours', this.mine, /*truncate=*/false));
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
    card.tabIndex = 0;
    card.setAttribute('role', 'button');
    const open = () => this.detail.open(t, this.relatedTo(t));
    card.addEventListener('click', open);
    card.addEventListener('keydown', event => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        open();
      }
    });

    const title = document.createElement('h3');
    title.textContent = t.title;

    const outcome = document.createElement('p');
    outcome.textContent = t.outcome;

    const footer = document.createElement('div');
    footer.className = 'card-footer';

    // Connector marks. The brand mark where there is one, a monogram where
    // there is not - and a tooltip on both naming the service and how it is
    // reached, which the reference's identical icons never told you.
    const marks = document.createElement('div');
    marks.className = 'connector-marks';
    for (const c of t.connectors.slice(0, 3)) {
      const mark = connectorMark(c.id, 'connector-mark');
      mark.dataset['transport'] = c.transport;
      mark.title = transportLabel(c.id, c.transport);
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

/** And back the other way, for templates that arrive over mojo. */
const SCOPE_NAME: Record<number, Template['writeScope']> = {
  [WriteScope.kReadOnly]: 'readonly',
  [WriteScope.kDraft]: 'draft',
  [WriteScope.kSend]: 'send',
  [WriteScope.kPurchase]: 'purchase',
};

/** The console names scopes as strings; the mojom names them as an enum. */
const SCOPE_BY_NAME: Record<Template['writeScope'], WriteScope> = {
  readonly: WriteScope.kReadOnly,
  draft: WriteScope.kDraft,
  send: WriteScope.kSend,
  purchase: WriteScope.kPurchase,
};
