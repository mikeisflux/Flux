// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {
  FluxPageHandlerFactory,
  FluxPageHandlerObserverReceiver,
  FluxPageHandlerRemote,
  RunState,
} from './flux.mojom-webui.js';
import type {
  ActionRecord,
  ApprovalRequest,
  RunProgress,
} from './flux.mojom-webui.js';

import {RunList} from './runs.js';

/**
 * The console's shell column.
 *
 * This document is not a tab. It is hosted by FluxSidebarView in the browser
 * frame, which is why every nav item is a target=_blank link: a same-frame
 * navigation would replace the sidebar with the page, whereas a new-window
 * request reaches the view's OpenURLFromTab() and is redirected into the
 * window's active tab. That indirection is the whole navigation model here.
 */
class FluxSidebar {
  private handler: FluxPageHandlerRemote;
  private runs: RunList;
  private approvalsNav: HTMLElement;
  private approvalsBadge: HTMLElement;
  private pendingApprovals = new Set<string>();

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

    this.approvalsNav = document.getElementById('approvals-nav')!;
    this.approvalsBadge = document.getElementById('approvals-badge')!;

    this.markCurrentOnClick();
    void this.bindCollapse();
    void this.refresh();
  }

  /**
   * Collapsing is a window layout change, so the browser owns the state and
   * this document only follows it. Reading it back on load rather than
   * assuming expanded keeps a collapsed window collapsed across a restart.
   */
  private async bindCollapse() {
    const button = document.getElementById('collapse')!;
    const {collapsed} = await this.handler.getSidebarCollapsed();
    const apply = (value: boolean) => {
      document.body.classList.toggle('collapsed', value);
      button.title = value ? 'Show sidebar' : 'Hide sidebar';
      button.setAttribute('aria-pressed', String(value));
    };
    apply(collapsed);

    button.addEventListener('click', () => {
      const next = !document.body.classList.contains('collapsed');
      apply(next);
      this.handler.setSidebarCollapsed(next);
    });
  }

  /**
   * The active row is set optimistically on click rather than read back from
   * the tab. The sidebar cannot see what the tab is showing, and a nav row
   * that lags a click by a round trip reads as a dropped input.
   */
  private markCurrentOnClick() {
    for (const el of document.querySelectorAll<HTMLElement>('.nav-item')) {
      el.addEventListener('click', () => {
        for (const other of document.querySelectorAll('.nav-item')) {
          other.removeAttribute('aria-current');
        }
        el.setAttribute('aria-current', 'page');
      });
    }
  }

  private async refresh() {
    const {runs} = await this.handler.listRuns();
    this.runs.replaceAll(runs);
    await this.runs.refreshConcurrency();
  }

  private syncApprovals() {
    this.approvalsBadge.textContent = String(this.pendingApprovals.size);
    this.approvalsNav.hidden = this.pendingApprovals.size === 0;
  }

  // --- FluxPageHandlerObserver ---------------------------------------------

  onRunProgress(progress: RunProgress) {
    this.runs.update(progress);
  }

  onAction(runId: string, action: ActionRecord) {
    this.runs.appendAction(runId, action);
  }

  onApprovalRequested(request: ApprovalRequest) {
    // The dialog itself belongs to the tab, which has room for a payload
    // preview. All the sidebar owes the user is that the badge appears the
    // instant a run blocks, on every screen, including one with no tab open on
    // the console at all.
    this.pendingApprovals.add(request.runId);
    this.syncApprovals();
  }

  onRunFinished(runId: string, state: RunState, summary: string|null) {
    this.runs.finish(runId, state, summary);
    this.pendingApprovals.delete(runId);
    this.syncApprovals();
  }

  onLearnedFact(fact: string, sourceRunId: string) {
    this.runs.noteLearned(fact, sourceRunId);
  }
}

new FluxSidebar();
