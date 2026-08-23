// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {RunState, TaskStepState} from './flux.mojom-webui.js';
import type {
  ActionRecord,
  FluxPageHandlerRemote,
  QuestionAnswer,
  QuestionRequest,
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
  private questionHost!: HTMLElement;
  private headLabel!: HTMLElement;
  private headLink!: HTMLAnchorElement;
  private pauseButton!: HTMLButtonElement;
  private saveButton!: HTMLButtonElement;
  private artifactHost!: HTMLElement;
  private composer!: HTMLTextAreaElement;
  private sendButton!: HTMLButtonElement;
  private stepCount!: HTMLElement;

  private progress: RunProgress|null = null;
  private artifacts: RunArtifact[] = [];
  private actionCount = 0;
  private lastStep = '';

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
    // Pause, and save-as-workflow. Both were fully implemented in the browser
    // and unreachable from here: a long task could only be killed, losing
    // everything it had done, and a run that turned out to be worth repeating
    // could not be turned into the workflow the browser already knew how to
    // compile from it.
    this.pauseButton = document.createElement('button');
    this.pauseButton.className = 'ghost';
    this.pauseButton.addEventListener('click', () => {
      if (this.progress?.state === RunState.kPaused) {
        this.handler.resumeRun(this.runId);
      } else {
        this.handler.pauseRun(this.runId);
      }
    });

    this.saveButton = document.createElement('button');
    this.saveButton.className = 'ghost';
    this.saveButton.textContent = 'Save as workflow';
    this.saveButton.hidden = true;
    this.saveButton.addEventListener('click', () => void this.saveAsWorkflow());

    const actions = document.createElement('div');
    actions.className = 'head-actions';
    actions.append(this.pauseButton, this.saveButton, this.headLink);
    head.append(this.headLabel, actions);

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

    // Between the transcript and the composer: the questions are the newest
    // thing the agent said and the next thing the user has to act on, so they
    // sit where the eye already is rather than above the plan.
    this.questionHost = document.createElement('div');
    this.questionHost.className = 'question-host';

    // Sits above the plan, because when a run has children they are what is
    // happening and the plan is the frame around them.
    this.subagentPanel = document.createElement('div');
    this.subagentPanel.className = 'plan-panel subagent-panel';
    this.subagentPanel.hidden = true;

    screen.append(head, this.stepCount, this.stream, this.artifactHost,
                  this.subagentPanel, this.planPanel, this.questionHost,
                  this.composerBox());
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
    // kAwaitingInput and kAwaitingApproval count as running: the task has not
    // finished, and offering a Send button next to an open question panel
    // invites the user to answer in the wrong place.
    const running = this.progress?.state === RunState.kRunning ||
        this.progress?.state === RunState.kQueued ||
        this.progress?.state === RunState.kAwaitingInput ||
        this.progress?.state === RunState.kAwaitingApproval;
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
    // The composer is the "or reply directly" half of an open question panel,
    // and the browser treats a typed message as the answer. Leaving the panel
    // on screen would offer a second way to answer something already answered.
    this.questionHost.replaceChildren();
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

  /**
   * "Thought for 8s", above the turn it belongs to.
   *
   * Only when it is long enough to have been a visible wait. Stamping "Thought
   * for 0s" on every turn is noise, and the point of the line is to explain a
   * gap the user already noticed.
   */
  private appendThinking(ms: number) {
    if (ms < 1500) {
      return;
    }
    const note = document.createElement('div');
    note.className = 'run-thinking';
    note.textContent = ms < 60000 ?
        `Thought for ${Math.round(ms / 1000)}s` :
        `Thought for ${Math.round(ms / 60000)}m`;
    this.stream.append(note);
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

    // How long it took. Set on every action since the runner started
    // recording it, and rendered nowhere - so a step that took forty seconds
    // and one that took forty milliseconds looked identical.
    const took = durationOf(action);
    if (took) {
      const ms = document.createElement('span');
      ms.className = 'tool-took';
      ms.textContent = took;
      chip.append(ms);
    }

    // An approved action is the one the user personally allowed past the write
    // scope. That is the whole point of the approval, and the transcript had
    // no trace of which step it was.
    if (action.wasApproved) {
      const mark = document.createElement('span');
      mark.className = 'tool-approved';
      mark.title = 'You approved this step';
      mark.append(pathIcon('M4 10.5l4 4 8-9'), 'Approved');
      chip.append(mark);
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
    const state = this.progress?.state;
    const live = state === RunState.kRunning || state === RunState.kQueued;
    const paused = state === RunState.kPaused;
    // Hidden rather than disabled once the run is over: a greyed Pause on a
    // finished task is a control that will never do anything.
    this.pauseButton.hidden = !live && !paused;
    this.pauseButton.textContent = paused ? 'Resume' : 'Pause';

    // Only once there is something to compile. CompileReplay works from the
    // action trace, and a run that has not acted yet has nothing to replay.
    this.saveButton.hidden =
        !(state === RunState.kSucceeded && this.actionCount > 0);

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

  /**
   * Turns a finished run into a saved workflow.
   *
   * The browser compiles it from the action trace, so this asks for nothing:
   * the run already contains the prompt, the scope and every step it took.
   */
  private async saveAsWorkflow() {
    this.saveButton.disabled = true;
    const {workflowId, error} = await this.handler.compileReplay(this.runId);
    this.saveButton.disabled = false;
    if (!workflowId) {
      this.appendProse(error ?? 'This run could not be saved as a workflow.');
      return;
    }
    this.saveButton.hidden = true;
    this.appendProse('Saved as a workflow. It is on the Workflows screen.');
  }

  // --- Questions ------------------------------------------------------------

  /**
   * The agent asking for something it cannot work out.
   *
   * One question on screen at a time with pagination, rather than a stack of
   * six fields. Every template prompt in the catalogue carries several
   * [placeholders], so a run can easily need four answers, and four textareas
   * at once reads as a form to fill in rather than a conversation.
   *
   * Answers are held locally and submitted together: the run is blocked on the
   * whole set, and sending them one at a time would wake it four times.
   */
  onQuestionsAsked(request: QuestionRequest) {
    if (request.runId !== this.runId || !this.root) {
      return;
    }
    const answers = new Map<string, string>();
    const skipped = new Set<string>();
    let index = 0;

    const panel = document.createElement('div');
    panel.className = 'question-panel';
    panel.setAttribute('role', 'group');
    panel.setAttribute('aria-label', 'Questions for you');

    if (request.preamble) {
      const why = document.createElement('div');
      why.className = 'question-preamble';
      why.append(renderMarkdown(request.preamble));
      panel.append(why);
    }

    const head = document.createElement('div');
    head.className = 'question-head';
    const title = document.createElement('span');
    title.className = 'question-title';
    title.append(pathIcon('M4 4h12v8H8l-4 3V4z'), 'Questions For You');

    const pager = document.createElement('div');
    pager.className = 'question-pager';
    const prev = document.createElement('button');
    prev.className = 'ghost-icon';
    prev.setAttribute('aria-label', 'Previous question');
    prev.append(pathIcon('M12 4l-6 6 6 6'));
    const count = document.createElement('span');
    count.className = 'muted';
    const next = document.createElement('button');
    next.className = 'ghost-icon';
    next.setAttribute('aria-label', 'Next question');
    next.append(pathIcon('M8 4l6 6-6 6'));
    pager.append(prev, count, next);
    head.append(title, pager);

    const body = document.createElement('div');
    body.className = 'question-body';
    const label = document.createElement('label');
    label.className = 'question-text';
    const field = document.createElement('textarea');
    field.rows = 3;
    label.append(field);

    const foot = document.createElement('div');
    foot.className = 'question-foot';
    const skip = document.createElement('button');
    skip.className = 'ghost';
    skip.textContent = 'Skip';
    const done = document.createElement('button');
    done.className = 'primary';
    foot.append(skip, done);

    const paint = () => {
      const q = request.questions[index]!;
      label.replaceChildren();
      const n = document.createElement('span');
      n.className = 'question-number';
      n.textContent = `${index + 1}.`;
      const t = document.createElement('span');
      t.textContent = q.text;
      label.append(n, t, field);
      field.placeholder = q.placeholder ?? 'Type or paste here...';
      field.value = answers.get(q.id) ?? '';
      count.textContent =
          `${index + 1} of ${request.questions.length}`;
      prev.disabled = index === 0;
      next.disabled = index === request.questions.length - 1;
      // The last question's button submits, so the user is never left hunting
      // for how to finish.
      done.textContent = index === request.questions.length - 1 ?
          'Send answers' : 'Next';
      field.focus();
    };

    const remember = () => {
      const q = request.questions[index]!;
      const text = field.value.trim();
      if (text) {
        answers.set(q.id, text);
        skipped.delete(q.id);
      } else {
        answers.delete(q.id);
      }
    };

    const submit = () => {
      remember();
      const payload: QuestionAnswer[] = request.questions.map(q => ({
        id: q.id,
        // Null is skipped; an empty string would read as "the answer is
        // nothing", which is a different thing and leads somewhere else.
        text: answers.has(q.id) ? answers.get(q.id)! : null,
      }));
      this.handler.answerQuestions(this.runId, payload);
      panel.remove();
      const echo = request.questions
          .map(q => `${q.text}\n${answers.get(q.id) ?? '(skipped)'}`)
          .join('\n\n');
      this.appendUser(echo);
    };

    prev.addEventListener('click', () => {
      remember();
      index = Math.max(0, index - 1);
      paint();
    });
    next.addEventListener('click', () => {
      remember();
      index = Math.min(request.questions.length - 1, index + 1);
      paint();
    });
    skip.addEventListener('click', () => {
      const q = request.questions[index]!;
      answers.delete(q.id);
      skipped.add(q.id);
      if (index === request.questions.length - 1) {
        submit();
        return;
      }
      index += 1;
      paint();
    });
    done.addEventListener('click', () => {
      remember();
      if (index === request.questions.length - 1) {
        submit();
        return;
      }
      index += 1;
      paint();
    });
    field.addEventListener('keydown', event => {
      if (event.key === 'Enter' && (event.metaKey || event.ctrlKey)) {
        event.preventDefault();
        submit();
      }
    });

    panel.append(head, body, foot);
    body.append(label);
    this.questionHost.replaceChildren(panel);
    paint();
  }

  // --- Live events ----------------------------------------------------------

  onProgress(progress: RunProgress) {
    if (progress.runId !== this.runId || !this.root) {
      return;
    }
    // One line per turn: the runner reports thinking_ms with every progress
    // update, and stamping it on each one would print the same "Thought for
    // 8s" repeatedly through a single turn.
    if (progress.currentStep && progress.currentStep !== this.lastStep) {
      this.lastStep = progress.currentStep;
      this.appendThinking(progress.thinkingMs);
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
    // A run that ended is not waiting for an answer, however the panel got
    // there - cancelled, out of budget, or the tab closed under it.
    this.questionHost.replaceChildren();
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
  [RunState.kAwaitingInput]: 'waiting',
  [RunState.kRunning]: 'running',
  [RunState.kAwaitingApproval]: 'waiting',
  [RunState.kPaused]: 'paused',
  [RunState.kSucceeded]: 'done',
  [RunState.kFailed]: 'failed',
  [RunState.kCancelled]: 'cancelled',
};

const RUN_STATE_LABEL: Record<number, string> = {
  [RunState.kQueued]: 'Queued',
  [RunState.kAwaitingInput]: 'Needs an answer',
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

/** How long an action took, as a label, or empty when it was instant. */
function durationOf(action: ActionRecord): string {
  const ms = Number(
      (action.finishedAt.internalValue - action.startedAt.internalValue) /
      1000n);
  if (!Number.isFinite(ms) || ms < 250) {
    return '';
  }
  return ms < 1000 ? `${ms}ms` :
      ms < 60000  ? `${(ms / 1000).toFixed(1)}s` :
                    `${Math.round(ms / 60000)}m`;
}

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
