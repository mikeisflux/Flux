// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {pathIcon} from './icons.js';

import {Provider, WriteScope} from './flux.mojom-webui.js';
import type {FluxPageHandlerRemote} from './flux.mojom-webui.js';

import {loadTemplates} from './catalog.js';
import type {Template} from './catalog.js';

/**
 * How hard to try. The reference exposes this as a bare "Medium" dropdown with
 * no explanation, which is a strange thing to leave unlabelled when it is the
 * only control on the screen that decides what a run costs.
 *
 * Here it is a budget: the ceiling on tokens and on credits, both of which the
 * browser process enforces by failing the run closed rather than billing on.
 */
interface Depth {
  id: string;
  label: string;
  hint: string;
  maxOutputTokens: number;
  creditBudget: bigint;
}

const DEPTHS: Depth[] = [
  {
    id: 'quick',
    label: 'Quick',
    hint: 'One pass, no deep research. Cheapest.',
    maxOutputTokens: 4096,
    creditBudget: 200n,
  },
  {
    id: 'medium',
    label: 'Medium',
    hint: 'The default. Enough room to check its own work.',
    maxOutputTokens: 16384,
    creditBudget: 1000n,
  },
  {
    id: 'thorough',
    label: 'Thorough',
    hint: 'Long-running research. Costs the most; stops at the ceiling.',
    maxOutputTokens: 65536,
    creditBudget: 5000n,
  },
];

/** Prefills for the chips under the composer, each bound to a real template. */
const SUGGESTIONS: Array<{label: string, icon: string, templateId: string}> = [
  {
    label: 'Triage my inbox',
    icon: 'M2 5h16v10H2V5zm0 0l8 6 8-6',
    templateId: 'ops-label-gmail-inbox-morning',
  },
  {
    label: 'Prep my day',
    icon: 'M3 5h14v12H3V5zm0 4h14M7 3v4m6-4v4',
    templateId: 'monitoring-morning-briefing-slack-calendar',
  },
  {
    label: 'Research a topic',
    icon: 'M9 3.5a5.5 5.5 0 1 1 0 11 5.5 5.5 0 0 1 0-11zM13 13l4 4',
    templateId: 'research-research-account-apollo-list',
  },
];

export class NewTaskView {
  private depth: Depth = DEPTHS[1]!;
  private all: Template[] = [];
  private prompt!: HTMLTextAreaElement;
  private templateId: string|null = null;
  private pending: string|null = null;
  private status!: HTMLElement;

  constructor(private handler: FluxPageHandlerRemote) {}

  async render(root: HTMLElement) {
    // A template handed over from the Templates screen. Read here and applied
    // once the composer exists, which is a different method. Via
    // sessionStorage rather than the URL, because a prompt the user has just
    // edited does not belong in a link.
    this.pending = sessionStorage.getItem('flux.pendingTask');
    sessionStorage.removeItem('flux.pendingTask');
    this.all = await loadTemplates();

    root.replaceChildren();
    root.classList.remove('two-column');

    const screen = document.createElement('div');
    screen.className = 'screen composer-screen';

    // The examples take over the whole screen until there is a run to show
    // that the product does anything - which is the one moment they help.
    const {runs} = await this.handler.listRuns();
    if (runs.length === 0 && !sessionStorage.getItem('flux.skip-examples')) {
      screen.append(this.examplesState(screen));
      root.append(screen);
      return;
    }

    screen.append(this.composer());
    root.append(screen);
  }

  // --- First run: try an example -------------------------------------------

  private examplesState(screen: HTMLElement): HTMLElement {
    const wrap = document.createElement('div');
    wrap.className = 'examples';

    const h1 = document.createElement('h1');
    h1.className = 'display';
    h1.textContent = 'Try an example';

    const list = document.createElement('div');
    list.className = 'example-list';

    const paint = () => {
      list.replaceChildren();
      for (const t of this.threeExamples()) {
        list.append(this.exampleCard(t));
      }
    };
    paint();

    const footer = document.createElement('div');
    footer.className = 'examples-footer';

    const shuffle = document.createElement('button');
    shuffle.className = 'link-button';
    shuffle.textContent = '⇄ Shuffle';
    shuffle.addEventListener('click', paint);

    const mine = document.createElement('button');
    mine.className = 'link-button';
    mine.textContent = 'I want to try myself →';
    mine.addEventListener('click', () => {
      // Session-scoped, not a pref: the user is dismissing a first-run hint,
      // not setting a preference, and it comes back in a fresh window if they
      // still have not run anything.
      sessionStorage.setItem('flux.skip-examples', '1');
      screen.replaceChildren(this.composer());
    });

    footer.append(shuffle, document.createTextNode(' · '), mine);
    wrap.append(h1, list, footer);
    return wrap;
  }

  /** Three at random from the featured set, re-rolled by Shuffle. */
  private threeExamples(): Template[] {
    const pool = this.all.filter(t => t.featured !== undefined);
    const picked: Template[] = [];
    const taken = new Set<number>();
    while (picked.length < 3 && taken.size < pool.length) {
      const i = Math.floor(Math.random() * pool.length);
      if (!taken.has(i)) {
        taken.add(i);
        picked.push(pool[i]!);
      }
    }
    return picked;
  }

  private exampleCard(t: Template): HTMLElement {
    const card = document.createElement('button');
    card.className = 'example-card';

    const mark = document.createElement('span');
    mark.className = 'connector-mark';
    mark.textContent = t.connectors[0]?.id.slice(0, 1) ?? '✦';

    const text = document.createElement('span');
    const title = document.createElement('strong');
    title.textContent = t.title;
    const outcome = document.createElement('span');
    outcome.textContent = t.outcome;
    text.append(title, outcome);

    card.append(mark, text);
    card.addEventListener('click', () => void this.start(t.title, t.id));
    return card;
  }

  // --- The composer --------------------------------------------------------

  private composer(): HTMLElement {
    const wrap = document.createElement('div');
    wrap.className = 'composer-wrap';

    const h1 = document.createElement('h1');
    h1.className = 'display';
    h1.textContent = 'What can I do for you?';

    const box = document.createElement('div');
    box.className = 'composer';

    this.prompt = document.createElement('textarea');
    this.prompt.rows = 3;
    this.prompt.placeholder =
        'Describe your task, / for commands, @ for context';
    // Enter submits, Shift+Enter is a newline: this is a task box, and a task
    // is usually one sentence.
    this.prompt.addEventListener('keydown', event => {
      if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        void this.start(this.prompt.value, this.templateId);
      }
    });
    // Typing your own task means you are no longer running the chip's
    // template, only borrowing its wording.
    this.prompt.addEventListener('input', () => {
      this.templateId = null;
    });

    const bar = document.createElement('div');
    bar.className = 'composer-bar';
    bar.append(this.depthPicker());

    const right = document.createElement('div');
    right.className = 'composer-actions';

    // Drawn but not wired: there is no mojom for either yet, and a control
    // that silently does nothing is worse than one that says so.
    for (const [title, path] of [
             ['Watching the run live is not wired up yet',
              'M3 4h14v9H3V4zm4 12h6'],
             ['Attachments are not wired up yet',
              'M13 7l-5 5a2.5 2.5 0 0 0 3.5 3.5l5.5-5.5a4 4 0 0 0-5.7-5.7L5 10.5'],
    ]) {
      const b = document.createElement('button');
      b.className = 'ghost-icon';
      b.disabled = true;
      b.title = title!;
      b.append(pathIcon(path!));
      right.append(b);
    }

    const send = document.createElement('button');
    send.className = 'send';
    send.title = 'Start this task';
    send.setAttribute('aria-label', 'Start this task');
    send.append(pathIcon('M10 16V5m0 0l-4 4m4-4l4 4'));
    send.addEventListener(
        'click', () => void this.start(this.prompt.value, this.templateId));
    right.append(send);

    bar.append(right);
    box.append(this.prompt, bar);

    if (this.pending) {
      try {
        const {prompt, templateId} =
            JSON.parse(this.pending) as {prompt: string, templateId: string};
        this.prompt.value = prompt;
        this.templateId = templateId;
      } catch {
        // A malformed hand-off is not worth failing the screen over: the user
        // still has an empty composer they can type into.
      }
    }

    this.status = document.createElement('p');
    this.status.className = 'composer-status';
    this.status.hidden = true;

    wrap.append(h1, box, this.status, this.chips());
    return wrap;
  }

  private depthPicker(): HTMLElement {
    const select = document.createElement('select');
    select.className = 'depth';
    for (const d of DEPTHS) {
      const option = document.createElement('option');
      option.value = d.id;
      option.textContent = d.label;
      option.selected = d.id === this.depth.id;
      select.append(option);
    }
    select.title = this.depth.hint;
    select.addEventListener('change', () => {
      this.depth = DEPTHS.find(d => d.id === select.value) ?? DEPTHS[1]!;
      select.title = this.depth.hint;
    });
    return select;
  }

  private chips(): HTMLElement {
    const row = document.createElement('div');
    row.className = 'chip-row';

    const byId = new Map(this.all.map(t => [t.id, t]));
    for (const s of SUGGESTIONS) {
      const t = byId.get(s.templateId);
      if (!t) {
        continue;
      }
      const chip = document.createElement('button');
      chip.className = 'chip';
      chip.append(pathIcon(s.icon));
      chip.append(s.label);
      chip.title = t.outcome;
      chip.addEventListener('click', () => {
        this.prompt.value = t.title;
        this.templateId = t.id;
        this.prompt.focus();
      });
      row.append(chip);
    }

    const more = document.createElement('a');
    more.className = 'chip';
    more.href = '#templates/tasks';
    more.textContent = 'More';
    row.append(more);

    return row;
  }

  // --- Starting a run ------------------------------------------------------

  private async start(prompt: string, templateId: string|null) {
    const text = prompt.trim();
    if (!text) {
      return;
    }

    // A task typed by hand gets the narrowest scope there is. Anything wider
    // is a decision the user has to make deliberately, on the run, when the
    // agent asks - not one made for them by a text box.
    const template = templateId ?
        this.all.find(t => t.id === templateId) ?? null :
        null;
    const scope = template ? SCOPES[template.writeScope] : WriteScope.kReadOnly;

    const {runId, error} = await this.handler.startRun({
      prompt: text,
      templateId,
      writeScope: scope,
      model: {
        provider: Provider.kAnthropic,
        model: 'claude-opus-5',
        maxOutputTokens: this.depth.maxOutputTokens,
        allowFailover: true,
      },
      profileId: '',
      creditBudget: this.depth.creditBudget,
    });

    if (error) {
      this.status.hidden = false;
      this.status.textContent = error;
      this.status.classList.add('error');
      return;
    }

    this.prompt.value = '';
    this.templateId = null;
    // Straight to the run. Starting a task and being left on the form, told to
    // "follow it in the sidebar", is the console describing what it just did
    // instead of showing it.
    window.location.hash = `#run/${runId}`;
  }
}

const SCOPES: Record<Template['writeScope'], WriteScope> = {
  readonly: WriteScope.kReadOnly,
  draft: WriteScope.kDraft,
  send: WriteScope.kSend,
  purchase: WriteScope.kPurchase,
};
