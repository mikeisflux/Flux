// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {RunState} from './flux.mojom-webui.js';
import type {
  ActionRecord,
  FluxPageHandlerRemote,
  RunProgress,
} from './flux.mojom-webui.js';

const STATE_NAME: Record<number, string> = {
  [RunState.kQueued]: 'queued',
  [RunState.kRunning]: 'running',
  [RunState.kAwaitingApproval]: 'awaitingApproval',
  [RunState.kAwaitingInput]: 'awaitingInput',
  [RunState.kPaused]: 'paused',
  [RunState.kSucceeded]: 'succeeded',
  [RunState.kFailed]: 'failed',
  [RunState.kCancelled]: 'cancelled',
};

/**
 * "Today", "Yesterday", or the date.
 *
 * mojo_base.mojom.Time arrives as microseconds since the WINDOWS epoch, which
 * is 1601 - not the Unix epoch. Subtracting the offset is not optional; without
 * it every run is dated in the seventeenth century.
 */
const WINDOWS_TO_UNIX_EPOCH_MS = 11644473600000;

function dayLabel(time: {internalValue: bigint}): string {
  const ms = Number(time.internalValue / 1000n) - WINDOWS_TO_UNIX_EPOCH_MS;
  if (!Number.isFinite(ms) || ms <= 0) {
    return 'Earlier';
  }
  const when = new Date(ms);
  const midnight = new Date();
  midnight.setHours(0, 0, 0, 0);
  const dayMs = 24 * 60 * 60 * 1000;
  if (when.getTime() >= midnight.getTime()) {
    return 'Today';
  }
  if (when.getTime() >= midnight.getTime() - dayMs) {
    return 'Yesterday';
  }
  return when.toLocaleDateString(
      undefined, {month: 'short', day: 'numeric'});
}

/** The always-visible list of runs in the sidebar. */
export class RunList {
  private rows = new Map<string, HTMLElement>();
  private headings = new Map<string, HTMLElement>();
  private actions = new Map<string, ActionRecord[]>();

  constructor(
      private container: HTMLElement,
      private concurrencyEl: HTMLElement,
      private handler: FluxPageHandlerRemote) {}

  replaceAll(runs: RunProgress[]) {
    this.container.replaceChildren();
    this.rows.clear();
    this.headings.clear();
    // Newest first, so a run started this minute is at the top of Today rather
    // than at the bottom of whatever order the browser's map produced.
    const ordered = [...runs].sort(
        (a, b) => Number(b.startedAt.internalValue - a.startedAt.internalValue));
    for (const run of ordered) {
      this.update(run);
    }
  }

  /**
   * Files a row under a day heading, creating it if this is the first run of
   * that day.
   *
   * Grouped rather than one flat column, because a list that mixes this
   * morning with last Tuesday reads as a single undifferentiated pile and the
   * only thing anyone wants from it is "what did I run today".
   */
  private place(row: HTMLElement, progress: RunProgress) {
    const label = dayLabel(progress.startedAt);
    let heading = this.headings.get(label);
    if (!heading) {
      heading = document.createElement('div');
      heading.className = 'run-day';
      heading.textContent = label;
      this.headings.set(label, heading);
      this.container.append(heading);
    }
    // Directly after its heading: within a day the newest run belongs on top,
    // and replaceAll feeds them newest first.
    heading.after(row);
  }

  update(progress: RunProgress) {
    let row = this.rows.get(progress.runId);
    if (!row) {
      row = document.createElement('div');
      row.className = 'run';
      row.tabIndex = 0;
      row.setAttribute('role', 'link');
      // The sidebar is a different document from the tab, so it navigates the
      // window through the browser process rather than setting a hash the tab
      // would never see. Same path the nav rows take.
      const open = () => this.handler.showScreen(`run/${progress.runId}`);
      row.addEventListener('click', open);
      row.addEventListener('keydown', event => {
        if (event.key === 'Enter' || event.key === ' ') {
          event.preventDefault();
          open();
        }
      });
      const body = document.createElement('span');
      body.className = 'run-body';
      body.append(
          Object.assign(document.createElement('span'), {className: 'run-title'}),
          Object.assign(document.createElement('span'), {className: 'run-step'}));
      row.append(
          Object.assign(document.createElement('span'), {className: 'run-dot'}),
          body,
          Object.assign(document.createElement('span'), {className: 'run-cost'}));
      this.rows.set(progress.runId, row);
      this.place(row, progress);
    }

    row.dataset['state'] = STATE_NAME[progress.state] ?? 'queued';
    // The title is fixed for the life of the run; the step underneath is what
    // moves. It used to be the other way round - the row's label was
    // currentStep, so it changed every turn and a run could not be found by
    // name in a list of six.
    row.querySelector('.run-title')!.textContent =
        progress.title || 'Untitled task';
    row.querySelector('.run-step')!.textContent =
        progress.currentStep || STATE_NAME[progress.state] || '';

    // Cost is shown live rather than discovered on a bill afterwards.
    const cost = row.querySelector('.run-cost')!;
    cost.textContent = progress.creditsSpent > 0
        ? formatCredits(progress.creditsSpent)
        : '';

    row.title = `${progress.actionsTaken} actions · ` +
        `${progress.inputTokens + progress.outputTokens} tokens`;
  }

  appendAction(runId: string, action: ActionRecord) {
    const list = this.actions.get(runId) ?? [];
    list.push(action);
    this.actions.set(runId, list);

    const row = this.rows.get(runId);
    if (row) {
      row.querySelector('.run-step')!.textContent = action.summary;
    }
  }

  finish(runId: string, state: RunState, summary: string|null) {
    const row = this.rows.get(runId);
    if (!row) {
      return;
    }
    row.dataset['state'] = STATE_NAME[state] ?? 'succeeded';
    if (summary) {
      row.querySelector('.run-step')!.textContent = summary;
    }
    void this.refreshConcurrency();
  }

  noteLearned(fact: string, sourceRunId: string) {
    const row = this.rows.get(sourceRunId);
    if (row) {
      row.title += `\nLearned: ${fact}`;
    }
  }

  /**
   * Renders the resolved concurrency, not the abstract setting. "Auto" tells a
   * user nothing when they are trying to work out why a task has not started;
   * "4 running · 2 queued" tells them exactly.
   */
  async refreshConcurrency() {
    const {limit, active, queued} = await this.handler.getConcurrencyLimit();
    this.concurrencyEl.textContent = queued > 0
        ? `${active}/${limit} · ${queued} queued`
        : `${active}/${limit}`;
  }
}

function formatCredits(credits: bigint|number): string {
  const n = Number(credits) / 1000;
  return n < 0.01 ? '<0.01' : n.toFixed(2);
}
