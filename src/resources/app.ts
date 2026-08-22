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
import {WelcomeView} from './welcome_view.js';
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
  private welcome: WelcomeView;
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
    this.welcome = new WelcomeView(this.handler);

    window.addEventListener('hashchange', () => this.renderFromHash());
    this.renderFromHash();
  }

  private renderFromHash() {
    // "#templates/skills" - screen, then whatever that screen needs. Keeping
    // the route in the fragment is what lets the shell's sidebar, which lives
    // in another document entirely, reach any screen with a plain link.
    const hash = window.location.hash.replace(/^#/, '');
    if (!hash) {
      void this.renderDefault();
      return;
    }
    const [screen, sub] = hash.split('/');
    this.render(screen ?? 'new-task', sub);
  }

  /**
   * A new tab lands here with no fragment. Without a model key the product
   * cannot do anything at all, so that - rather than a first-run flag - is the
   * signal to run setup: it is the actual precondition, it is still true after
   * a profile is copied to a new machine, and it stops being true the moment
   * it is fixed.
   */
  private async renderDefault() {
    const {statuses} = await this.handler.listProviderKeys();
    this.render(statuses.some(s => s.configured) ? 'new-task' : 'welcome');
  }

  private render(view: string, sub?: string) {
    const content = document.getElementById('content')!;
    content.classList.remove('two-column', 'four-column');
    showSkeleton(content);

    switch (view) {
      case 'new-task':
        void this.newTask.render(content);
        return;
      case 'welcome':
        void this.welcome.render(content);
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

/**
 * A grey stand-in for the screen that is loading. Deliberately shaped like the
 * content rather than a spinner: a spinner says "something is happening", a
 * skeleton says what is about to appear, and the difference matters most on
 * the screens that fetch a 250-row catalogue.
 */
function showSkeleton(content: HTMLElement) {
  content.replaceChildren();
  const screen = document.createElement('div');
  screen.className = 'screen';
  const bar = document.createElement('div');
  bar.className = 'skeleton skeleton-title';
  screen.append(bar);
  for (let i = 0; i < 6; i++) {
    const row = document.createElement('div');
    row.className = 'skeleton skeleton-row';
    screen.append(row);
  }
  content.append(screen);
}

new FluxApp();
