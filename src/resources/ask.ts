// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {
  FluxPageHandlerFactory,
  FluxPageHandlerObserverReceiver,
  FluxPageHandlerRemote,
} from './flux.mojom-webui.js';
import type {
  ActionRecord,
  AskAttachment,
  AskTurn,
  ApprovalRequest,
  ConnectorStatus,
  QuestionRequest,
  RunArtifact,
  RunProgress,
  RunState,
} from './flux.mojom-webui.js';

import {renderMarkdown} from './skills.js';
import {pathIcon} from './icons.js';

/** What the intelligence selector picks between. */
interface Depth {
  label: string;
  hint: string;
  model: string;
}

// Three, matching the composer's ladder rather than inventing a second one.
// The model name is what the browser process routes on - a Claude name goes to
// Anthropic, anything else to OpenAI - so this doubles as the provider choice.
const DEPTHS: Depth[] = [
  {label: 'Fast', hint: 'Quick answers about Flux.',
   model: 'claude-haiku-4-5-20251001'},
  {label: 'Medium', hint: 'For a balance of speed and intelligence.',
   model: 'claude-sonnet-5'},
  {label: 'Thorough', hint: 'For working something out properly.',
   model: 'claude-opus-5'},
];

// A file bigger than this is not going to help: the bytes are inlined into the
// prompt, so the ceiling is the model's context rather than the disk.
const kMaxAttachmentBytes = 512 * 1024;

/**
 * The Ask Flux panel.
 *
 * Its own document beside the page. The thread it draws is not its own - the
 * browser process owns it - so this reads it back on load and redraws from
 * whatever arrives. Closing the panel mid-answer and reopening it shows the
 * answer, which is the difference between a panel and a chat window.
 */
class AskPanel {
  private handler: FluxPageHandlerRemote;
  private thread = document.getElementById('ask-thread')!;
  private questions = document.getElementById('ask-questions')!;
  private input = document.getElementById('ask-text') as HTMLTextAreaElement;
  private send = document.getElementById('ask-send') as HTMLButtonElement;
  private depthButton = document.getElementById('ask-depth') as HTMLButtonElement;
  private fileInput =
      document.getElementById('ask-file-input') as HTMLInputElement;
  private fileLabel = document.getElementById('ask-files')!;
  private depth = DEPTHS[1]!;
  private attachments: AskAttachment[] = [];
  private busy = false;
  // getAskThread is a round trip, and a turn can land inside it. Without this
  // the restore that follows replaces the thread's children and that turn is
  // gone from the screen until the next one arrives.
  private restored = false;
  private early: AskTurn[] = [];

  constructor() {
    this.handler = new FluxPageHandlerRemote();
    const observer = new FluxPageHandlerObserverReceiver(this);
    FluxPageHandlerFactory.getRemote().createPageHandler(
        observer.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());

    this.send.addEventListener('click', () => this.submit());
    // Enter sends, Shift+Enter is a newline. The panel is narrow and most
    // messages are one line, so the reverse would cost a modifier every time.
    this.input.addEventListener('keydown', event => {
      if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        this.submit();
      }
    });

    document.getElementById('ask-new')!.addEventListener('click', () => {
      this.handler.newAskThread();
      this.thread.replaceChildren();
      this.questions.replaceChildren();
      this.early = [];
      this.restored = true;
      this.setBusy(false);
    });
    document.getElementById('ask-close')!.addEventListener('click', () => {
      // Through the pref, not by hiding anything: the window layout reserves
      // the panel's width from that pref, so a view that hid itself would
      // leave a gap where it used to be.
      this.handler.setAskPanelOpen(false);
    });

    this.depthButton.textContent = this.depth.label;
    this.depthButton.addEventListener('click', () => this.showDepthMenu());

    document.getElementById('ask-attach')!.addEventListener(
        'click', () => this.fileInput.click());
    this.fileInput.addEventListener('change', () => void this.takeFiles());

    window.addEventListener('unhandledrejection', event => {
      console.error('Unhandled rejection in the Ask panel', event.reason);
      event.preventDefault();
    });

    void this.restore();
  }

  /** The thread as the browser process has it, on load and after a reload. */
  private async restore() {
    const {turns, busy, pending} = await this.handler.getAskThread();
    this.thread.replaceChildren();
    for (const turn of turns) {
      this.thread.append(this.turnEl(turn));
    }
    // Anything that arrived while that was in flight. The snapshot may already
    // contain it, so this only appends what came after the count it returned.
    for (const turn of this.early.slice(turns.length)) {
      this.thread.append(this.turnEl(turn));
    }
    this.early = [];
    this.restored = true;
    this.setBusy(busy);
    // Put the open question back. It is not part of the thread, so without
    // this a panel reopened mid-question showed a reply ending in a question
    // and no way to answer it.
    if (pending) {
      this.showQuestions(pending);
    }
    this.scroll();
  }

  private setBusy(busy: boolean) {
    this.busy = busy;
    this.send.disabled = busy;
    this.input.disabled = busy;
  }

  private submit() {
    const text = this.input.value.trim();
    if (!text || this.busy) {
      return;
    }
    this.input.value = '';
    this.handler.sendAsk(text, this.depth.model, this.attachments);
    this.attachments = [];
    this.fileLabel.textContent = '';
    this.setBusy(true);
  }

  /**
   * Reads the picked files into memory.
   *
   * Held here until the message is sent rather than uploaded on pick: an
   * attachment with no message is not a thing the model can be asked about,
   * and a file the user changed their mind about should not have travelled.
   */
  private async takeFiles() {
    for (const file of Array.from(this.fileInput.files ?? [])) {
      if (file.size > kMaxAttachmentBytes) {
        this.fileLabel.textContent = `${file.name} is too large`;
        continue;
      }
      const bytes = new Uint8Array(await file.arrayBuffer());
      this.attachments.push({
        name: file.name,
        mimeType: file.type || 'application/octet-stream',
        bytes,
      });
    }
    this.fileInput.value = '';
    this.fileLabel.textContent = this.attachments.length === 1 ?
        this.attachments[0]!.name :
        `${this.attachments.length} files`;
  }

  private showDepthMenu() {
    const existing = document.querySelector('.ask-depth-menu');
    if (existing) {
      existing.remove();
      this.depthButton.setAttribute('aria-expanded', 'false');
      return;
    }
    const menu = document.createElement('div');
    menu.className = 'ask-depth-menu';
    for (const depth of DEPTHS) {
      const row = document.createElement('button');
      row.className = 'ask-depth-row';
      row.setAttribute('aria-pressed', String(depth === this.depth));
      const name = document.createElement('strong');
      name.textContent = depth.label;
      const hint = document.createElement('span');
      hint.textContent = depth.hint;
      row.append(name, hint);
      row.addEventListener('click', () => {
        this.depth = depth;
        this.depthButton.textContent = depth.label;
        menu.remove();
        this.depthButton.setAttribute('aria-expanded', 'false');
      });
      menu.append(row);
    }
    this.depthButton.setAttribute('aria-expanded', 'true');
    this.depthButton.after(menu);
  }

  private turnEl(turn: AskTurn): HTMLElement {
    const el = document.createElement('div');
    el.className = turn.fromUser ? 'ask-turn ask-user' : 'ask-turn';

    if (!turn.fromUser && turn.thinkingMs > 0) {
      const thought = document.createElement('p');
      thought.className = 'ask-thought';
      thought.textContent =
          `Thought for ${Math.max(1, Math.round(turn.thinkingMs / 1000))}s`;
      el.append(thought);
    }

    // The steps sit above the reply, in the order they happened: the model
    // saved the workflow and then told you it had, and reading it the other
    // way round makes the chip look like a button.
    for (const step of turn.steps) {
      const chip = document.createElement('div');
      chip.className = 'ask-step';
      chip.dataset['ok'] = String(step.succeeded);
      chip.append(pathIcon('M4 10.5l4 4 8-9'));
      chip.append(step.label);
      el.append(chip);
    }

    if (turn.text) {
      const body = document.createElement('div');
      body.className = 'ask-text';
      if (turn.fromUser) {
        body.textContent = turn.text;
      } else {
        body.append(renderMarkdown(turn.text));
      }
      el.append(body);
    }
    return el;
  }

  private scroll() {
    this.thread.scrollTop = this.thread.scrollHeight;
  }

  /**
   * The questions panel, with the choices as taps.
   *
   * One question at a time with a pager, because the panel is narrow and four
   * questions stacked in it is a form. Choices are lettered so the reply can
   * refer to one out loud.
   */
  private showQuestions(request: QuestionRequest) {
    this.questions.replaceChildren();
    if (request.questions.length === 0) {
      return;
    }
    // text is nullable in the mojom and null means skipped, which is
    // documented there as a different thing from an empty answer. Starting
    // them at null means Skip sends "not answered" rather than "answered with
    // nothing", and the agent is told which happened.
    const answers: Array<{id: string, text: string|null}> =
        request.questions.map(q => ({id: q.id, text: null}));
    let index = 0;

    const panel = document.createElement('div');
    panel.className = 'question-panel';

    const head = document.createElement('div');
    head.className = 'question-head';
    const title = document.createElement('strong');
    title.textContent = 'Questions For You';
    const count = document.createElement('span');
    count.className = 'muted';
    head.append(title, count);

    const label = document.createElement('div');
    const body = document.createElement('div');
    const field = document.createElement('textarea');
    field.rows = 2;

    const foot = document.createElement('div');
    foot.className = 'question-foot';
    const skip = document.createElement('button');
    skip.className = 'ghost';
    skip.textContent = 'Skip';
    const next = document.createElement('button');
    next.className = 'primary';

    const finish = () => {
      this.questions.replaceChildren();
      this.handler.answerAsk(answers);
      this.setBusy(true);
    };

    const advance = () => {
      const typed = field.value.trim();
      answers[index]!.text = typed === '' ? null : typed;
      if (index === request.questions.length - 1) {
        finish();
        return;
      }
      index++;
      paint();
    };

    const paint = () => {
      const question = request.questions[index]!;
      count.textContent = `${index + 1} of ${request.questions.length}`;
      label.replaceChildren();
      const n = document.createElement('span');
      n.className = 'question-number';
      n.textContent = `${index + 1}.`;
      const text = document.createElement('span');
      text.textContent = question.text;
      label.append(n, text);

      body.replaceChildren();
      for (const [i, choice] of question.choices.entries()) {
        const pick = document.createElement('button');
        pick.className = 'question-choice';
        const letter = document.createElement('span');
        letter.className = 'question-letter';
        letter.textContent = String.fromCharCode(65 + i);
        const what = document.createElement('span');
        what.textContent = choice;
        pick.append(letter, what);
        pick.addEventListener('click', () => {
          field.value = choice;
          advance();
        });
        body.append(pick);
      }
      field.placeholder = question.choices.length > 0 ?
          'Or reply directly…' :
          question.placeholder || 'Your answer';
      field.value = answers[index]!.text ?? '';
      body.append(field);
      next.textContent =
          index === request.questions.length - 1 ? 'Done' : 'Next';
    };

    skip.addEventListener('click', finish);
    next.addEventListener('click', advance);
    foot.append(skip, next);
    panel.append(head, label, body, foot);
    this.questions.append(panel);
    paint();
    field.focus();
  }

  // --- FluxPageHandlerObserver ---------------------------------------------

  onAskTurn(turn: AskTurn, busy: boolean) {
    if (!this.restored) {
      this.early.push(turn);
      return;
    }
    this.thread.append(this.turnEl(turn));
    this.setBusy(busy);
    this.scroll();
  }

  onAskQuestions(request: QuestionRequest) {
    this.setBusy(false);
    this.showQuestions(request);
    this.scroll();
  }

  // The panel is not the console. Everything below belongs to a run, and runs
  // are drawn in the tab and the sidebar - implemented because the observer
  // interface is one interface, not because this document has a use for them.
  onRunProgress(_progress: RunProgress) {}
  onAction(_runId: string, _action: ActionRecord) {}
  onArtifact(_runId: string, _artifact: RunArtifact) {}
  onQuestionsAsked(_request: QuestionRequest) {}
  onApprovalRequested(_request: ApprovalRequest) {}
  onRunFinished(_runId: string, _state: RunState, _summary: string|null) {}
  onLearnedFact(_fact: string, _sourceRunId: string) {}
  onConnectorChanged(_status: ConnectorStatus, _error: string|null) {}
}

new AskPanel();
