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
  AskTurn,
  QuestionRequest,
  RunArtifact,
  RunProgress,
} from './flux.mojom-webui.js';

import {ApprovalQueue} from './approvals.js';
import {FluxSettingsView} from './settings_view.js';
import {CommandPalette} from './command_palette.js';
import {ConnectorsView} from './connectors_view.js';
import {CustomizeView} from './customize_view.js';
import {WelcomeView} from './welcome_view.js';
import {NewTaskView} from './new_task_view.js';
import {TemplatesView} from './templates_view.js';
import {WorkflowsView} from './workflows_view.js';
import {RunView} from './run_view.js';

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
  private settings: FluxSettingsView;
  private connectors: ConnectorsView;
  private palette = new CommandPalette();
  private customize: CustomizeView;
  private welcome: WelcomeView;
  private newTask: NewTaskView;
  private templates: TemplatesView;
  private workflows: WorkflowsView;
  private run: RunView;

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

    this.settings = new FluxSettingsView(this.handler);
    this.connectors = new ConnectorsView(this.handler);
    this.newTask = new NewTaskView(this.handler);
    this.customize = new CustomizeView(this.handler);
    this.welcome = new WelcomeView(this.handler);
    this.workflows = new WorkflowsView(this.handler);
    this.run = new RunView(this.handler);
    this.templates = new TemplatesView(this.handler);

    // Ctrl+K reaches the panel from any console screen. The same chord over a
    // web page is handled in the frame - see patches/0013 - because a page that
    // binds Ctrl+K itself would otherwise swallow it, and the sites worth
    // automating (Slack, Linear, GitHub) all do. Both paths land here.
    window.addEventListener('keydown', event => {
      if (event.key.toLowerCase() === 'k' && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        window.location.hash = '#search';
      }
    });

    // Every `void this.x()` in the console starts async work whose rejection
    // nobody holds. There are two dozen of them - an event handler cannot
    // await, so the pattern is unavoidable - and each one currently fails by
    // doing nothing at all: no error, no empty state, a button that shrugs.
    //
    // This is the net under all of them. It does not replace a real error path
    // where a screen has one; it makes the ones that do not fail loudly rather
    // than silently, which is the difference between a bug the user can report
    // and one they work around forever.
    window.addEventListener('unhandledrejection', event => {
      console.error('Unhandled rejection in the console', event.reason);
      event.preventDefault();
      this.toast(event.reason instanceof Error ?
                     event.reason.message :
                     'Something went wrong. The last action did not complete.');
    });

    window.addEventListener('hashchange', () => this.renderFromHash());
    this.renderFromHash();
  }

  /**
   * A message that outlives the action that failed.
   *
   * Not a dialog: this fires for anything that went wrong anywhere, including
   * things the user was not waiting on, and a modal for a failed background
   * refresh is worse than the silence it replaces. It stays until dismissed,
   * because a message that removes itself after four seconds is one they will
   * miss - which is how these failed in the first place.
   */
  private toast(message: string) {
    let bar = document.getElementById('flux-toast');
    if (!bar) {
      bar = document.createElement('div');
      bar.id = 'flux-toast';
      bar.className = 'toast';
      bar.setAttribute('role', 'alert');
      const close = document.createElement('button');
      close.className = 'ghost-icon';
      close.setAttribute('aria-label', 'Dismiss');
      close.textContent = '\u00d7';
      close.addEventListener('click', () => bar!.remove());
      const text = document.createElement('span');
      text.className = 'toast-text';
      bar.append(text, close);
      document.body.append(bar);
    }
    bar.querySelector('.toast-text')!.textContent = message;
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
    // Falls through to the welcome screen if the browser process cannot be
    // reached, rather than leaving a blank tab: welcome is the screen that
    // works with nothing configured, so it is the safe answer to "I could not
    // find out what is configured".
    let configured = false;
    try {
      const {statuses} = await this.handler.listProviderKeys();
      configured = statuses.some(s => s.configured);
    } catch (error) {
      console.error('Could not read the configured providers.', error);
    }
    this.render(configured ? 'new-task' : 'welcome');
  }

  private render(view: string, sub?: string) {
    const content = document.getElementById('content')!;
    content.classList.remove('two-column', 'four-column');
    showSkeleton(content);

    // Every screen goes through settle(), and none of them are allowed to be
    // fired off with a bare `void`. A render that rejects used to leave the
    // skeleton drawn above it on screen for good: no error, no empty state,
    // just grey bars that look like a load which never finishes. One bad path
    // - fetch() cannot read a chrome:// URL, so all three catalogues threw -
    // made every screen in the console look identically broken with nothing
    // on screen saying why. The skeleton is a promise that something is
    // coming; if it is not, say so.
    switch (view) {
      case 'new-task':
        return settle(content, this.newTask.render(content));
      case 'welcome':
        return settle(content, this.welcome.render(content));
      case 'customize':
        return settle(
            content,
            this.customize.render(
                content, sub === 'skills' ? 'skills' : 'instructions'));
      case 'approvals':
        // The nav row has always linked here; until now it fell through to the
        // default branch below, which draws the view's name as a heading and
        // nothing else. A badge that says one thing is waiting, pointing at a
        // blank page, is worse than no badge.
        return settle(content, this.approvals.renderScreen(content));
      case 'connectors':
        return settle(content, this.connectors.render(content));
      case 'templates':
        return settle(
            content,
            this.templates.render(
                content, sub === 'skills' ? 'skills' : 'tasks'));
      case 'workflows':
        return settle(content, this.workflows.render(content));
      case 'run':
        // The sub-segment is the run id, so #run/<id> is a link anything can
        // hand out - the sidebar's run list, a notification, the composer.
        return settle(content, this.run.render(content, sub));
      case 'search':
        return settle(content, this.palette.render(content));
      case 'agent':
      case 'settings':
        return settle(content, this.settings.render(content, sub));
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

  onRunProgress(progress: RunProgress) {
    // The sidebar owns the run list; the tab owns the open run, if it is this
    // one. Both listen to the same stream rather than one relaying to the
    // other - they are separate documents.
    this.run.onProgress(progress);
  }

  onAction(runId: string, action: ActionRecord) {
    this.run.onAction(runId, action);
  }

  onArtifact(runId: string, artifact: RunArtifact) {
    this.run.onArtifact(runId, artifact);
  }

  onQuestionsAsked(request: QuestionRequest) {
    // The tab owns this, not the sidebar: answering needs the question text
    // and a field, which is not something a one-line run row can hold.
    this.run.onQuestionsAsked(request);
  }

  onApprovalRequested(request: ApprovalRequest) {
    // Surfaced immediately and unconditionally: a run blocked on a human is
    // doing nothing until someone answers, and a blocked run the user cannot
    // see is the worst state this product has.
    this.approvals.enqueue(request);
  }

  onRunFinished(runId: string, state: RunState, summary: string|null) {
    this.approvals.dismissFor(runId);
    this.run.onFinished(runId, state, summary);
  }

  onLearnedFact(_fact: string, _sourceRunId: string) {
    // The agent writing back into the Instructions buffer is shown with
    // provenance in the sidebar's run list rather than silently mutating what
    // the user wrote.
  }

  onAskTurn(_turn: AskTurn, _busy: boolean) {
    // The Ask panel is its own document, hosted in the browser frame. Every
    // observer receives every callback, so this is here to say the tab
    // deliberately ignores it rather than to leave a hole in the interface.
  }

  onAskQuestions(_request: QuestionRequest) {
    // As above - the panel draws its own questions.
  }

  onConnectorChanged(status: ConnectorStatus, error: string|null) {
    // Connecting finishes in another tab, so the result arrives here rather
    // than as the answer to the click that started it.
    this.connectors.onConnectorChanged(status, error);
  }
}

/**
 * Waits for a screen to finish drawing, and puts a readable failure on screen
 * if it does not.
 *
 * The console has no server to fall back on: every screen is the browser
 * process plus a packed catalogue, so when one of those is unreachable there
 * is nothing to show but the reason. Rendering the reason is the point - the
 * failure mode this replaces was a skeleton that stayed up forever, which
 * reads as "slow" and is impossible to report.
 */
function settle(content: HTMLElement, drawn: Promise<void>) {
  void drawn.catch((error: unknown) => {
    console.error(error);
    content.replaceChildren();

    const screen = document.createElement('div');
    screen.className = 'screen';

    const h1 = document.createElement('h1');
    h1.textContent = 'This screen could not load';

    const detail = document.createElement('p');
    detail.className = 'subtitle';
    detail.textContent = error instanceof Error ?
        error.message :
        'Something went wrong reading this screen\u2019s data.';

    const retry = document.createElement('button');
    retry.className = 'ghost';
    retry.textContent = 'Try again';
    retry.addEventListener('click', () => window.location.reload());

    screen.append(h1, detail, retry);
    content.append(screen);
  });
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
