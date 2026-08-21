// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {
  FluxPageHandlerFactory,
  FluxPageHandlerRemote,
  FluxPageHandlerObserverReceiver,
  type ActionRecord,
  type ApprovalRequest,
  type RunProgress,
  RunState,
} from './flux.mojom-webui.js';

import {RunList} from './runs.js';
import {ApprovalQueue} from './approvals.js';
import {SettingsView} from './settings.js';

/**
 * Owns the Mojo connection and routes browser-process events to the views.
 *
 * All state lives in the browser process. The console holds nothing that
 * matters, so reloading chrome://flux mid-run is harmless - a deliberate
 * difference from a console that would lose a run's history on refresh.
 */
class FluxApp {
  private handler: FluxPageHandlerRemote;
  private runs: RunList;
  private approvals: ApprovalQueue;
  private settings: SettingsView;

  constructor() {
    this.handler = new FluxPageHandlerRemote();
    const observer = new FluxPageHandlerObserverReceiver(this);

    FluxPageHandlerFactory.getRemote().createPageHandler(
        observer.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());

    this.runs = new RunList(
        document.getElementById('run-list')!,
        document.getElementById('concurrency')!,
        this.handler);

    this.approvals = new ApprovalQueue(
        document.getElementById('approval-dialog') as HTMLDialogElement,
        document.getElementById('approvals-nav')!,
        document.getElementById('approvals-badge')!,
        this.handler);

    this.settings = new SettingsView(this.handler);

    this.bindNav();
    void this.refresh();
  }

  private bindNav() {
    for (const el of document.querySelectorAll<HTMLElement>('.nav-item')) {
      el.addEventListener('click', () => {
        for (const other of document.querySelectorAll('.nav-item')) {
          other.removeAttribute('aria-current');
        }
        el.setAttribute('aria-current', 'page');
        this.render(el.dataset['view'] ?? 'new-task');
      });
    }
  }

  private async refresh() {
    const {runs} = await this.handler.listRuns();
    this.runs.replaceAll(runs);
    await this.runs.refreshConcurrency();
  }

  private render(view: string) {
    const content = document.getElementById('content')!;
    if (view === 'agent') {
      void this.settings.render(content);
      return;
    }
    content.replaceChildren();
    const h1 = document.createElement('h1');
    h1.textContent = view.replace('-', ' ').replace(/^\w/, c => c.toUpperCase());
    content.append(h1);
  }

  // --- FluxPageHandlerObserver ---------------------------------------------

  onRunProgress(progress: RunProgress) {
    this.runs.update(progress);
  }

  onAction(runId: string, action: ActionRecord) {
    this.runs.appendAction(runId, action);
  }

  onApprovalRequested(request: ApprovalRequest) {
    // Surfaced immediately and unconditionally: a run blocked on a human is
    // doing nothing until someone answers, and a blocked run the user cannot
    // see is the worst state this product has.
    this.approvals.enqueue(request);
  }

  onRunFinished(runId: string, state: RunState, summary: string|null) {
    this.runs.finish(runId, state, summary);
    this.approvals.dismissFor(runId);
  }

  onLearnedFact(fact: string, sourceRunId: string) {
    // The agent writing back into the Instructions buffer is shown with
    // provenance rather than silently mutating what the user wrote.
    this.runs.noteLearned(fact, sourceRunId);
  }
}

new FluxApp();
