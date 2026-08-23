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
  ConnectorStatus,
  RunArtifact,
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
   * Nav rows navigate the window over the Mojo pipe, and mark themselves
   * current optimistically.
   *
   * They used to be target=_blank links, on the theory that a new-window
   * request would reach FluxSidebarView::OpenURLFromTab and be redirected into
   * the active tab. It did not arrive - the row highlighted and the tab never
   * moved - and the whole mechanism was a lot of Blink plumbing to depend on
   * for something the shell can just say. showScreen() is the browser process
   * doing the navigation, which is what OpenCommandPalette already did for
   * Ctrl+K, and that path has always worked.
   *
   * The href stays: it keeps the row a real link for focus and middle-click,
   * and it is where the screen name comes from if data-view is ever missing.
   *
   * Current is set on click rather than read back from the tab. The sidebar
   * cannot see what the tab is showing, and a nav row that lags a click by a
   * round trip reads as a dropped input.
   */
  private markCurrentOnClick() {
    // EVERY link in this document, not just the nav rows. The search icon
    // points at #search and is not a .nav-item; left to itself it would
    // navigate the sidebar's own frame and replace the shell with the console.
    // There is no link here that should ever load in this frame.
    for (const el of document.querySelectorAll<HTMLAnchorElement>('a[href]')) {
      el.addEventListener('click', event => {
        const screen = el.dataset['view'] || el.hash.replace(/^#/, '');
        if (!screen) {
          return;
        }
        // A modified click still means "open this somewhere else" - let the
        // browser have it. Those go out through AddNewContents, which the
        // sidebar does not handle, so they are inert rather than wrong.
        if (event.ctrlKey || event.metaKey || event.shiftKey ||
            event.button !== 0) {
          return;
        }
        event.preventDefault();
        this.handler.showScreen(screen);

        if (!el.classList.contains('nav-item')) {
          return;
        }
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
    // Children are drawn inside their parent's run view, not as siblings here.
    // ListRuns already filters them out; without the same filter on the live
    // stream a task that spawns four subagents grows five rows the moment it
    // starts and shrinks back to one on reload.
    if (progress.parentRunId) {
      return;
    }
    this.runs.update(progress);
  }

  onAction(runId: string, action: ActionRecord) {
    this.runs.appendAction(runId, action);
  }

  onArtifact(_runId: string, _artifact: RunArtifact) {
    // The tab's run view shows these. The sidebar's list is one line per run
    // and has nowhere to put a file.
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

  onConnectorChanged(_status: ConnectorStatus, _error: string|null) {
    // The rail shows runs, not connectors. Implemented because every observer
    // has to implement the whole interface, and doing nothing is the honest
    // behaviour here rather than an oversight - the connectors screen lives in
    // the tab and redraws itself.
  }
}

new FluxSidebar();
