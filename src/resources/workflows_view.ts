// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {connectorMark, pathIcon, shape, svgRoot} from './icons.js';

import type {FluxPageHandlerRemote, WorkflowSummary} from
    './flux.mojom-webui.js';
import {WorkflowDialog} from './workflow_dialog.js';

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
  private handler: FluxPageHandlerRemote;
  private dialog: WorkflowDialog;
  private root: HTMLElement|null = null;

  constructor(handler: FluxPageHandlerRemote) {
    this.handler = handler;
    this.dialog = new WorkflowDialog(handler, () => void this.repaint());
  }

  async render(root: HTMLElement) {
    this.root = root;
    await this.repaint();
  }

  /**
   * Puts a message at the top of the screen. Inserted into the live DOM rather
   * than kept as state, so the next repaint clears it - an error about one
   * click should not outlive everything the user does afterwards.
   */
  private reportError(message: string) {
    const screen = this.root?.querySelector('.screen');
    if (!screen) {
      return;
    }
    let banner = screen.querySelector<HTMLElement>('.inline-error');
    if (!banner) {
      banner = document.createElement('div');
      banner.className = 'inline-error';
      banner.setAttribute('role', 'alert');
      screen.insertBefore(banner, screen.children[1] ?? null);
    }
    banner.textContent = message;
  }

  private async repaint() {
    const root = this.root;
    if (!root) {
      return;
    }
    const [all, {workflows}] =
        await Promise.all([loadTemplates(), this.handler.listWorkflows()]);

    root.replaceChildren();
    root.classList.remove('two-column');

    const screen = document.createElement('div');
    screen.className = 'screen';

    const head = document.createElement('div');
    head.className = 'screen-head';
    const h1 = document.createElement('h1');
    h1.textContent = 'Workflows';

    const buttons = document.createElement('div');
    buttons.className = 'head-actions';
    if (workflows.length > 0) {
      // Only once there is a table to put it beside. On the empty screen the
      // examples are already the whole page.
      const examples = document.createElement('a');
      examples.className = 'button';
      examples.href = '#templates/tasks';
      examples.textContent = 'View examples';
      buttons.append(examples);
    }
    const create = document.createElement('button');
    create.className = 'primary';
    create.textContent = 'New workflow';
    create.addEventListener('click', () => this.dialog.open({}));
    buttons.append(create);
    head.append(h1, buttons);
    screen.append(head);

    if (workflows.length === 0) {
      screen.append(this.emptyState(), this.examples(all));
    } else {
      screen.append(this.table(workflows), this.recommended(all));
    }
    root.append(screen);
  }

  private table(workflows: WorkflowSummary[]): HTMLElement {
    const table = document.createElement('div');
    table.className = 'workflow-table';

    const header = document.createElement('div');
    header.className = 'workflow-row workflow-header';
    for (const label of ['Workflow', 'Schedule', 'Last run', 'Status', '']) {
      const cell = document.createElement('span');
      cell.textContent = label;
      header.append(cell);
    }
    table.append(header);

    for (const workflow of workflows) {
      table.append(this.row(workflow));
    }
    return table;
  }

  private row(workflow: WorkflowSummary): HTMLElement {
    const row = document.createElement('div');
    row.className = 'workflow-row';

    const first = document.createElement('div');
    const command = document.createElement('a');
    command.className = 'workflow-command';
    command.href = '#workflows';
    command.textContent = `/${workflow.command}`;
    const description = document.createElement('p');
    description.textContent = workflow.description || workflow.name;
    first.append(command, description);

    const schedule = document.createElement('div');
    if (workflow.cron) {
      const cron = document.createElement('span');
      cron.textContent = workflow.scheduleDisplay || workflow.cron;
      cron.title = workflow.cron;
      const next = document.createElement('p');
      next.textContent = workflow.enabled ?
          nextRunText(workflow) :
          'Paused - it will not fire until resumed';
      schedule.append(cron, next);
    } else {
      const manual = document.createElement('span');
      manual.className = 'muted';
      manual.textContent = 'Only when you run it';
      schedule.append(manual);
    }

    const lastRun = document.createElement('div');
    lastRun.textContent =
        workflow.lastRun ? formatTime(workflow.lastRun) : 'Never run';
    if (workflow.lastFireMissed) {
      const missed = document.createElement('p');
      missed.className = 'muted';
      // Surfaced, because a workflow that quietly skipped a firing looks
      // exactly like one that fired and found nothing to do.
      missed.textContent = 'A scheduled run was missed';
      lastRun.append(missed);
    }

    const status = document.createElement('div');
    status.className = 'workflow-status';
    status.dataset['state'] = workflow.enabled ? 'active' : 'paused';
    status.append(
        pathIcon(workflow.enabled ? 'M4 10.5l4 4 8-9' : 'M7 4v12M13 4v12'));
    status.append(workflow.enabled ? 'Active' : 'Paused');

    row.append(first, schedule, lastRun, status, this.rowMenu(workflow));
    return row;
  }

  private rowMenu(workflow: WorkflowSummary): HTMLElement {
    const wrap = document.createElement('div');
    wrap.className = 'row-menu';

    const button = document.createElement('button');
    button.className = 'ghost-icon';
    button.setAttribute('aria-label', `Actions for /${workflow.command}`);
    button.setAttribute('aria-expanded', 'false');
    button.textContent = '\u22ef';

    const menu = document.createElement('div');
    menu.className = 'row-menu-items';
    menu.hidden = true;
    // The document handler closes any open menu on a click anywhere. Without
    // this, a click on the menu's own padding - between two items - counts as
    // "anywhere" and shuts it. Items close it themselves.
    menu.addEventListener('click', event => event.stopPropagation());

    const item = (label: string, icon: string, onClick: () => void) => {
      const entry = document.createElement('button');
      entry.className = 'row-menu-item';
      entry.append(pathIcon(icon));
      entry.append(label);
      entry.addEventListener('click', () => {
        menu.hidden = true;
        button.setAttribute('aria-expanded', 'false');
        onClick();
      });
      menu.append(entry);
      return entry;
    };

    item('Run now', 'M6 4l10 6-10 6V4z', () => {
      // The result was previously discarded, so a run that could not start
      // looked exactly like one that did - which is to say, like nothing at
      // all. On success the console follows the run; on failure it says why.
      void this.handler.runWorkflowNow(workflow.id).then(({runId, error}) => {
        if (runId) {
          window.location.hash = `#run/${encodeURIComponent(runId)}`;
          return;
        }
        this.reportError(error ?? 'The workflow could not be started.');
      });
    });
    item(workflow.enabled ? 'Pause schedule' : 'Resume schedule',
         workflow.enabled ? 'M7 4v12M13 4v12' : 'M6 4l10 6-10 6V4z', () => {
           this.handler.setWorkflowEnabled(workflow.id, !workflow.enabled);
           void this.repaint();
         });
    item('Edit', 'M3 15.5V17h1.5l9-9L12 6.5l-9 9z', () => {
      // The summary carries no instructions - they can be pages long and the
      // table shows a line. The dialog opens on what the row knows and the
      // user re-states the instructions, rather than the list dragging every
      // workflow's full prompt across the pipe to render five words.
      this.dialog.edit(workflow, '');
    });
    const remove = item('Delete', 'M5 6h10l-1 11H6L5 6zm3-3h4v2H8V3z', () => {
      this.handler.deleteWorkflow(workflow.id);
      void this.repaint();
    });
    remove.classList.add('destructive');

    button.addEventListener('click', event => {
      // Without this the click reaches the document handler below and closes
      // the menu in the same gesture that opened it.
      event.stopPropagation();
      const open = menu.hidden;
      // One menu at a time, and clicking the row's button again closes it.
      closeRowMenus();
      if (!open) {
        return;
      }
      menu.hidden = false;
      button.setAttribute('aria-expanded', 'true');
      placeRowMenu(menu, button);
    });

    wrap.append(button, menu);
    return wrap;
  }

  private recommended(all: Template[]): HTMLElement {
    const section = document.createElement('section');
    const h2 = document.createElement('h2');
    h2.className = 'section-title';
    h2.textContent = 'Recommended next';
    const body = document.createElement('p');
    body.className = 'subtitle';
    body.textContent = 'More scheduled workflows that fit your context.';

    const grid = document.createElement('div');
    grid.className = 'card-grid';
    for (const t of all.filter(t => t.schedule !== null).slice(0, 3)) {
      grid.append(this.card(t));
    }
    section.append(h2, body, grid);
    return section;
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
      const mark = connectorMark(c.id, 'connector-mark');
      mark.dataset['transport'] = c.transport;
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

/** "Next Tue, Sep 1 at 7:00 AM" - the row says when, not a cron expression. */
function nextRunText(workflow: WorkflowSummary): string {
  if (!workflow.nextRun) {
    return 'No next run';
  }
  return `Next ${formatTime(workflow.nextRun)}`;
}

/**
 * Mojo carries a time as microseconds since the Windows epoch (1601), which is
 * not what Date wants. Converting through the documented offset rather than
 * eyeballing it: getting this wrong shows a plausible date that is centuries
 * off, and nobody reads a date carefully enough to catch it.
 */
const WINDOWS_TO_UNIX_EPOCH_MS = 11644473600000;

function formatTime(time: {internalValue: bigint}): string {
  const ms = Number(time.internalValue / BigInt(1000)) -
      WINDOWS_TO_UNIX_EPOCH_MS;
  return new Date(ms).toLocaleString(
      undefined,
      {weekday: 'short', month: 'short', day: 'numeric', hour: 'numeric',
       minute: '2-digit'});
}


/**
 * Places an open row menu against its button, in viewport coordinates.
 *
 * The menu is `position: fixed` rather than absolute inside the row, because
 * two ancestors clip it and no z-index escapes either. `.workflow-table` sets
 * `overflow: hidden` so its rounded corners clip the rows, which sliced the
 * menu off at the bottom edge of the table; and `.content` scrolls, so the
 * last row's menu would have been clipped there even without the table.
 *
 * A fixed element's containing block is the viewport, and an ancestor's
 * overflow does not clip it - but only while no ancestor has a transform,
 * filter or will-change. Any of those becomes the containing block itself and
 * brings the clipping straight back, so if this ever starts being cut off
 * again, that is the first thing to look for.
 */
function placeRowMenu(menu: HTMLElement, button: HTMLElement) {
  const anchor = button.getBoundingClientRect();
  // Measured after unhiding: a hidden element has no box to place from.
  const width = menu.offsetWidth;
  const height = menu.offsetHeight;
  const gap = 6;
  const margin = 8;

  let top = anchor.bottom + gap;
  if (top + height > window.innerHeight - margin) {
    // Flip above the button rather than running off the bottom, which is
    // where every row below the fold would otherwise put it.
    top = Math.max(margin, anchor.top - gap - height);
  }
  const left = Math.min(
      Math.max(margin, anchor.right - width),
      Math.max(margin, window.innerWidth - margin - width));

  menu.style.top = `${Math.round(top)}px`;
  menu.style.left = `${Math.round(left)}px`;
}

function closeRowMenus() {
  for (const menu of
           document.querySelectorAll<HTMLElement>('.row-menu-items')) {
    menu.hidden = true;
  }
  for (const button of
           document.querySelectorAll<HTMLElement>('.row-menu > button')) {
    button.setAttribute('aria-expanded', 'false');
  }
}

// A fixed menu does not travel with its row, so anything that moves the row
// closes it rather than leaving it stranded over a different one. Scroll is
// captured because the scrolling element is `.content`, not the document, and
// a scroll event on a non-root element does not bubble.
document.addEventListener('click', () => closeRowMenus());
document.addEventListener('scroll', () => closeRowMenus(), true);
window.addEventListener('resize', () => closeRowMenus());
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') {
    closeRowMenus();
  }
});
