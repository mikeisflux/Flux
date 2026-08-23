// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {RunState, TaskStepState} from './flux.mojom-webui.js';
import type {
  ActionRecord,
  FluxPageHandlerRemote,
  RunArtifact,
  RunProgress,
  SubagentSummary,
  TaskStep,
} from './flux.mojom-webui.js';

import {pathIcon, shape, svgRoot} from './icons.js';
import {renderMarkdown} from './skills.js';

/**
 * A run, as it happens.
 *
 * This is where the product actually does its work, and it is the only screen
 * that is a transcript rather than a form: the agent's turns, the tools it
 * called, the plan it set itself, and whatever it produced.
 *
 * Live rather than polled. Every run event the browser process emits is
 * already pushed to the console for the sidebar's run list, so this listens to
 * the same stream and appends. Reloading mid-run refetches with getRun() and
 * continues from there, which is why the browser keeps the summary and the
 * artifacts rather than only forwarding them once.
 */
export class RunView {
  private handler: FluxPageHandlerRemote;
  private runId = '';
  private root: HTMLElement|null = null;

  private stream!: HTMLElement;
  private planPanel!: HTMLElement;
  private subagentPanel!: HTMLElement;
  private headLabel!: HTMLElement;
  private headLink!: HTMLAnchorElement;
  private artifactHost!: HTMLElement;
  private composer!: HTMLTextAreaElement;
  private sendButton!: HTMLButtonElement;
  private stepCount!: HTMLElement;

  private progress: RunProgress|null = null;
  private artifacts: RunArtifact[] = [];
  private actionCount = 0;

  constructor(handler: FluxPageHandlerRemote) {
    this.handler = handler;
  }

  async render(root: HTMLElement, runId: string|undefined) {
    this.root = root;
    this.runId = runId ?? '';
    root.replaceChildren();
    root.classList.remove('two-column');

    if (!this.runId) {
      const screen = document.createElement('div');
      screen.className = 'screen';
      const h1 = document.createElement('h1');
      h1.textContent = 'No run selected';
      const p = document.createElement('p');
      p.className = 'subtitle';
      p.textContent = 'Pick a run from the sidebar, or start a new task.';
      screen.append(h1, p);
      root.append(screen);
      return;
    }

    root.append(this.chrome());

    const {progress, actions, summary, artifacts} =
        await this.handler.getRun(this.runId);
    this.progress = progress;
    this.artifacts = artifacts;

    if (!progress) {
      this.appendProse('This run is no longer available.');
      return;
    }

    for (const action of actions) {
      this.appendAction(action);
    }
    if (summary) {
      this.appendSummary(summary);
    }
    for (const artifact of artifacts) {
      this.appendArtifact(artifact);
    }
    this.paintHead();
    this.paintPlan();
    this.paintSubagents();
    this.paintComposer();
  }

  /** The frame: header, transcript, subagents, plan panel, composer. */
  private chrome(): HTMLElement {
    const screen = document.createElement('div');
    screen.className = 'screen run-screen';

    const head = document.createElement('div');
    head.className = 'run-head';
    this.headLabel = document.createElement('span');
    this.headLabel.className = 'muted';
    this.headLabel.textContent = 'Run 1 of 1';
    this.headLink = document.createElement('a');
    this.headLink.className = 'run-head-link';
    this.headLink.href = '#workflows';
    this.headLink.textContent = 'View workflow →';
    head.append(this.headLabel, this.headLink);

    this.stepCount = document.createElement('button');
    this.stepCount.className = 'step-count';
    this.stepCount.textContent = '0 steps';
    this.stepCount.setAttribute('aria-expanded', 'true');
    this.stepCount.addEventListener('click', () => {
      const collapsed = this.stream.hasAttribute('data-collapsed');
      this.stream.toggleAttribute('data-collapsed', !collapsed);
      this.stepCount.setAttribute('aria-expanded', String(collapsed));
    });

    this.stream = document.createElement('div');
    this.stream.className = 'run-stream';

    this.artifactHost = document.createElement('div');
    this.artifactHost.className = 'run-artifacts';

    this.planPanel = document.createElement('div');
    this.planPanel.className = 'plan-panel';
    this.planPanel.hidden = true;

    // Sits above the plan, because when a run has children they are what is
    // happening and the plan is the frame around them.
    this.subagentPanel = document.createElement('div');
    this.subagentPanel.className = 'plan-panel subagent-panel';
    this.subagentPanel.hidden = true;

    screen.append(head, this.stepCount, this.stream, this.artifactHost,
                  this.subagentPanel, this.planPanel, this.composerBox());
    return screen;
  }

  private composerBox(): HTMLElement {
    const box = document.createElement('div');
    box.className = 'composer';

    this.composer = document.createElement('textarea');
    this.composer.rows = 2;
    this.composer.placeholder =
        'Send a follow up, / for commands, @ for context';
    this.composer.addEventListener('keydown', event => {
      if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        void this.send();
      }
    });

    const bar = document.createElement('div');
    bar.className = 'composer-bar';

    this.sendButton = document.createElement('button');
    this.sendButton.className = 'send';
    this.sendButton.title = 'Send';
    this.sendButton.setAttribute('aria-label', 'Send');
    this.sendButton.addEventListener('click', () => void this.send());

    bar.append(this.sendButton);
    box.append(this.composer, bar);
    return box;
  }

  /**
   * Send, or stop. One button, because the run is either taking instructions
   * or it is not, and two buttons where only one is ever live is a button that
   * does nothing.
   */
  private paintComposer() {
    const running = this.progress?.state === RunState.kRunning ||
        this.progress?.state === RunState.kQueued;
    this.sendButton.replaceChildren(
        running ? pathIcon('M6 6h8v8H6z') : pathIcon('M10 16V5m0 0l-4 4m4-4l4 4'));
    this.sendButton.dataset['mode'] = running ? 'stop' : 'send';
    this.sendButton.title = running ? 'Stop this run' : 'Send';
  }

  private async send() {
    if (this.sendButton.dataset['mode'] === 'stop') {
      this.handler.cancelRun(this.runId);
      return;
    }
    const text = this.composer.value.trim();
    if (!text) {
      return;
    }
    this.composer.value = '';
    this.appendUser(text);
    const {accepted, error} =
        await this.handler.sendFollowUp(this.runId, text);
    if (!accepted && error) {
      this.appendProse(error);
    }
  }

  // --- Stream ---------------------------------------------------------------

  private appendUser(text: string) {
    const bubble = document.createElement('div');
    bubble.className = 'run-user';
    bubble.textContent = text;
    this.stream.append(bubble);
  }

  private appendProse(text: string) {
    const p = document.createElement('p');
    p.className = 'run-prose';
    p.textContent = text;
    this.stream.append(p);
  }

  /**
   * The closing answer, as markdown. It is the one part of a run that is
   * written for a person rather than logged, so it gets headings, bullets and
   * emphasis - a wall of plain text buries the number the run was asked for.
   */
  private appendSummary(summary: string) {
    const block = document.createElement('div');
    block.className = 'run-summary';
    block.append(renderMarkdown(summary));
    this.stream.append(block);
  }

  private appendAction(action: ActionRecord) {
    this.actionCount++;
    this.stepCount.textContent =
        `${this.actionCount} ${this.actionCount === 1 ? 'step' : 'steps'}`;

    const chip = document.createElement('div');
    chip.className = 'tool-chip';
    chip.dataset['ok'] = String(action.succeeded);

    const name = document.createElement('span');
    name.className = 'tool-name';
    name.textContent = action.summary || action.toolName;
    chip.append(name);

    // The target, in monospace beside the verb: "Reading File all.txt" reads
    // as one thing, and the filename is the part being scanned for.
    const target = targetOf(action);
    if (target) {
      const code = document.createElement('code');
      code.textContent = target;
      chip.append(code);
    }

    if (!action.succeeded && action.error) {
      const error = document.createElement('span');
      error.className = 'tool-error';
      error.textContent = action.error;
      chip.append(error);
    }

    this.stream.append(chip);
    this.stream.scrollIntoView({block: 'end'});
  }

  private appendArtifact(artifact: RunArtifact) {
    const card = document.createElement('section');
    card.className = 'artifact-card';

    const title = document.createElement('h3');
    title.textContent = artifact.title;

    const primary = document.createElement('div');
    primary.className = 'artifact-primary';
    const icon = svgRoot('0 0 20 20', 'artifact-icon');
    shape(icon, 'rect', {x: 3, y: 2, width: 14, height: 16, rx: 2});
    shape(icon, 'path', {d: 'M7 8h6M7 11h6M7 14h4'});
    const kind = document.createElement('span');
    kind.textContent = artifact.kind || 'File';
    const open = document.createElement('a');
    open.className = 'artifact-open';
    open.href = artifact.url;
    open.target = '_blank';
    open.rel = 'noopener';
    open.textContent = 'Open ↗';
    primary.append(icon, kind, open);
    card.append(title, primary);

    if (artifact.files.length > 0) {
      const label = document.createElement('p');
      label.className = 'muted';
      label.textContent = 'Additional files';
      const list = document.createElement('div');
      list.className = 'artifact-files';
      for (const file of artifact.files) {
        const link = document.createElement('a');
        link.className = 'artifact-file';
        link.href = file.url;
        link.target = '_blank';
        link.rel = 'noopener';
        link.textContent = file.name;
        list.append(link);
      }
      card.append(label, list);
    }

    this.artifactHost.append(card);
  }

  // --- Plan -----------------------------------------------------------------

  private paintPlan() {
    const plan = this.progress?.plan ?? [];
    this.planPanel.hidden = plan.length === 0;
    if (plan.length === 0) {
      return;
    }

    const done = plan.filter(s => s.state === TaskStepState.kDone).length;
    this.planPanel.replaceChildren();

    const head = document.createElement('div');
    head.className = 'plan-head';
    const title = document.createElement('span');
    title.textContent =
        done === plan.length ? 'All tasks complete' : 'Task progress';
    const count = document.createElement('span');
    count.className = 'muted';
    count.textContent = `${done}/${plan.length}`;
    head.append(title, count);
    this.planPanel.append(head);

    for (const step of plan) {
      this.planPanel.append(planRow(step));
    }
  }

  // --- Subagents ------------------------------------------------------------

  /**
   * A child run opened on its own is otherwise a dead end: it belongs to a
   * task the sidebar does not list, so there is nothing on screen that leads
   * back to the run that started it.
   */
  private paintHead() {
    const parent = this.progress?.parentRunId;
    if (!parent) {
      return;
    }
    this.headLabel.textContent = 'Subagent';
    this.headLink.href = `#run/${encodeURIComponent(parent)}`;
    this.headLink.textContent = 'Back to the main run →';
  }

  private paintSubagents() {
    const children = this.progress?.subagents ?? [];
    this.subagentPanel.hidden = children.length === 0;
    if (children.length === 0) {
      return;
    }

    const done = children.filter(c => FINISHED.has(c.state)).length;
    const running = children.length - done;
    this.subagentPanel.replaceChildren();

    const head = document.createElement('div');
    head.className = 'plan-head';
    const title = document.createElement('span');
    title.textContent = 'Subagents';
    const count = document.createElement('span');
    count.className = 'muted';
    // "4/4 completed" on its own reads as finished even when three failed, so
    // the running tail stays until there is nothing left running.
    count.textContent = running > 0 ?
        `${done}/${children.length} completed · ${running} running` :
        `${done}/${children.length} completed`;
    head.append(title, count);
    this.subagentPanel.append(head);

    children.forEach((child, index) => {
      this.subagentPanel.append(subagentRow(child, index));
    });
  }

  // --- Live events ----------------------------------------------------------

  onProgress(progress: RunProgress) {
    if (progress.runId !== this.runId || !this.root) {
      return;
    }
    this.progress = progress;
    this.paintHead();
    this.paintPlan();
    this.paintSubagents();
    this.paintComposer();
  }

  onAction(runId: string, action: ActionRecord) {
    if (runId === this.runId && this.root) {
      this.appendAction(action);
    }
  }

  onArtifact(runId: string, artifact: RunArtifact) {
    if (runId === this.runId && this.root) {
      this.artifacts.push(artifact);
      this.appendArtifact(artifact);
    }
  }

  onFinished(runId: string, state: RunState, summary: string|null) {
    if (runId !== this.runId || !this.root) {
      return;
    }
    if (this.progress) {
      this.progress.state = state;
    }
    if (summary) {
      this.appendSummary(summary);
    }
    this.paintComposer();
  }
}

function planRow(step: TaskStep): HTMLElement {
  const row = document.createElement('div');
  row.className = 'plan-row';
  row.dataset['state'] = STATE_CLASS[step.state] ?? 'pending';

  const mark = svgRoot('0 0 20 20', 'plan-mark');
  if (step.state === TaskStepState.kDone) {
    shape(mark, 'circle', {cx: 10, cy: 10, r: 8});
    shape(mark, 'path', {d: 'M6 10.5l3 3 5-6'});
  } else if (step.state === TaskStepState.kActive) {
    shape(mark, 'circle', {cx: 10, cy: 10, r: 8});
    shape(mark, 'path', {d: 'M10 10V5a5 5 0 0 1 0 10z'});
  } else {
    shape(mark, 'circle', {cx: 10, cy: 10, r: 8});
  }

  const text = document.createElement('span');
  text.textContent = step.text;
  row.append(mark, text);
  return row;
}

/**
 * One child run. Numbered rather than bulleted because the parent's summary
 * refers to them by position, and the state word is spelled out - a coloured
 * dot alone cannot distinguish "failed" from "cancelled".
 */
function subagentRow(child: SubagentSummary, index: number): HTMLAnchorElement {
  // An anchor, not a div with a click handler: #run/<id> is the same link the
  // sidebar hands out, and a child is a real run with its own transcript.
  const row = document.createElement('a');
  row.className = 'subagent-row';
  row.href = `#run/${encodeURIComponent(child.runId)}`;
  row.dataset['state'] = RUN_STATE_CLASS[child.state] ?? 'queued';

  const ordinal = document.createElement('span');
  ordinal.className = 'subagent-ordinal';
  ordinal.textContent = String(index + 1);

  const body = document.createElement('div');
  body.className = 'subagent-body';
  const label = document.createElement('span');
  label.className = 'subagent-label';
  label.textContent = child.label;
  body.append(label);
  if (child.currentStep) {
    const step = document.createElement('span');
    step.className = 'muted';
    step.textContent = child.currentStep;
    body.append(step);
  }

  const badge = document.createElement('span');
  badge.className = 'subagent-state';
  badge.textContent = RUN_STATE_LABEL[child.state] ?? 'Queued';

  row.append(ordinal, body, badge);
  return row;
}

const FINISHED: ReadonlySet<RunState> = new Set([
  RunState.kSucceeded,
  RunState.kFailed,
  RunState.kCancelled,
]);

const RUN_STATE_CLASS: Record<number, string> = {
  [RunState.kQueued]: 'queued',
  [RunState.kRunning]: 'running',
  [RunState.kAwaitingApproval]: 'waiting',
  [RunState.kPaused]: 'paused',
  [RunState.kSucceeded]: 'done',
  [RunState.kFailed]: 'failed',
  [RunState.kCancelled]: 'cancelled',
};

const RUN_STATE_LABEL: Record<number, string> = {
  [RunState.kQueued]: 'Queued',
  [RunState.kRunning]: 'Running',
  [RunState.kAwaitingApproval]: 'Needs you',
  [RunState.kPaused]: 'Paused',
  [RunState.kSucceeded]: 'Done',
  [RunState.kFailed]: 'Failed',
  [RunState.kCancelled]: 'Cancelled',
};

const STATE_CLASS: Record<number, string> = {
  [TaskStepState.kPending]: 'pending',
  [TaskStepState.kActive]: 'active',
  [TaskStepState.kDone]: 'done',
  [TaskStepState.kSkipped]: 'skipped',
};

/**
 * The thing a tool acted on, for the chip. A URL is shortened to its path -
 * the host is on screen in the omnibox and the path is what changes between
 * one step and the next.
 */
function targetOf(action: ActionRecord): string {
  if (!action.pageUrl) {
    return '';
  }
  try {
    const url = new URL(action.pageUrl);
    return url.pathname === '/' ? url.host : url.host + url.pathname;
  } catch {
    return action.pageUrl;
  }
}
