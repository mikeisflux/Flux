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

/** The always-visible list of runs in the sidebar. */
export class RunList {
  private rows = new Map<string, HTMLElement>();
  private actions = new Map<string, ActionRecord[]>();

  constructor(
      private container: HTMLElement,
      private concurrencyEl: HTMLElement,
      private handler: FluxPageHandlerRemote) {}

  replaceAll(runs: RunProgress[]) {
    this.container.replaceChildren();
    this.rows.clear();
    for (const run of runs) {
      this.update(run);
    }
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
      row.append(
          Object.assign(document.createElement('span'), {className: 'run-dot'}),
          Object.assign(document.createElement('span'), {className: 'run-title'}),
          Object.assign(document.createElement('span'), {className: 'run-cost'}));
      this.rows.set(progress.runId, row);
      this.container.append(row);
    }

    row.dataset['state'] = STATE_NAME[progress.state] ?? 'queued';
    row.querySelector('.run-title')!.textContent =
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
      row.querySelector('.run-title')!.textContent = action.summary;
    }
  }

  finish(runId: string, state: RunState, summary: string|null) {
    const row = this.rows.get(runId);
    if (!row) {
      return;
    }
    row.dataset['state'] = STATE_NAME[state] ?? 'succeeded';
    if (summary) {
      row.querySelector('.run-title')!.textContent = summary;
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
