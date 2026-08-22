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

import {ApprovalQueue} from './approvals.js';
import {SettingsView} from './settings.js';
import {ConnectorsView} from './connectors_view.js';
import {CustomizeView} from './customize_view.js';
import {NewTaskView} from './new_task_view.js';
import {TemplatesView} from './templates_view.js';
import {WorkflowsView} from './workflows_view.js';

/**
 * The console's content column, running in a tab.
 *
 * The nav, the run list and the approvals badge are not here: they are in
 * sidebar.html, hosted by the browser frame, because they have to stay on
 * screen no matter what this tab is showing. What is left is the screen the
 * user asked for, chosen by the fragment so the shell can route to it with an
 * ordinary link, and the approval dialog, which needs the width.
 *
 * All state lives in the browser process. The console holds nothing that
 * matters, so reloading chrome://flux mid-run is harmless - a deliberate
 * difference from a console that would lose a run's history on refresh.
 */
class FluxApp {
  private handler: FluxPageHandlerRemote;
  private approvals: ApprovalQueue;
  private settings: SettingsView;
  private connectors = new ConnectorsView();
  private customize: CustomizeView;
  private newTask: NewTaskView;
  private templates = new TemplatesView();
  private workflows = new WorkflowsView();

  constructor() {
    this.handler = new FluxPageHandlerRemote();
    const observer = new FluxPageHandlerObserverReceiver(this);

    FluxPageHandlerFactory.getRemote().createPageHandler(
        observer.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());

    this.approvals = new ApprovalQueue(
        document.getElementById('approval-dialog') as HTMLDialogElement,
        // The visible approvals row is in the shell's sidebar, a different
        // document; this one owns only the dialog.
        /*nav=*/null,
        /*badge=*/null,
        this.handler);

    this.settings = new SettingsView(this.handler);
    this.newTask = new NewTaskView(this.handler);
    this.customize = new CustomizeView(this.handler);

    window.addEventListener('hashchange', () => this.renderFromHash());
    this.renderFromHash();
  }

  private renderFromHash() {
    // "#templates/skills" - screen, then whatever that screen needs. Keeping
    // the route in the fragment is what lets the shell's sidebar, which lives
    // in another document entirely, reach any screen with a plain link.
    const [screen, sub] = (window.location.hash.replace(/^#/, '') || 'new-task')
                              .split('/');
    this.render(screen ?? 'new-task', sub);
  }

  private render(view: string, sub?: string) {
    const content = document.getElementById('content')!;
    content.classList.remove('two-column');

    switch (view) {
      case 'new-task':
        void this.newTask.render(content);
        return;
      case 'customize':
        void this.customize.render(content, sub === 'skills' ? 'skills' : 'instructions');
        return;
      case 'connectors':
        void this.connectors.render(content);
        return;
      case 'templates':
        void this.templates.render(content, sub === 'skills' ? 'skills' : 'tasks');
        return;
      case 'workflows':
        void this.workflows.render(content);
        return;
      case 'agent':
      case 'settings':
        void this.settings.render(content);
        return;
      default:
        break;
    }

    content.replaceChildren();
    const screen = document.createElement('div');
    screen.className = 'screen';
    const h1 = document.createElement('h1');
    h1.textContent = view.replace('-', ' ').replace(/^\w/, c => c.toUpperCase());
    screen.append(h1);
    content.append(screen);
  }

  // --- FluxPageHandlerObserver ---------------------------------------------

  onRunProgress(_progress: RunProgress) {
    // The run list lives in the shell's sidebar; nothing in the tab tracks it.
  }

  onAction(_runId: string, _action: ActionRecord) {}

  onApprovalRequested(request: ApprovalRequest) {
    // Surfaced immediately and unconditionally: a run blocked on a human is
    // doing nothing until someone answers, and a blocked run the user cannot
    // see is the worst state this product has.
    this.approvals.enqueue(request);
  }

  onRunFinished(runId: string, _state: RunState, _summary: string|null) {
    this.approvals.dismissFor(runId);
  }

  onLearnedFact(_fact: string, _sourceRunId: string) {
    // The agent writing back into the Instructions buffer is shown with
    // provenance in the sidebar's run list rather than silently mutating what
    // the user wrote.
  }
}

new FluxApp();
