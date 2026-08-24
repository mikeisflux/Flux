// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import type {
  ApprovalRequest,
  FluxPageHandlerRemote,
} from './flux.mojom-webui.js';

/**
 * Blocking approval prompts for actions beyond a task's declared write scope.
 *
 * The run is stopped in the browser process while this is open, so there is no
 * risk of the action proceeding if the user walks away - the failure mode is a
 * stalled task, never an unintended send.
 */
export class ApprovalQueue {
  private pending: ApprovalRequest[] = [];

  // `nav` and `badge` are null in the tab: the console's shell owns the
  // sidebar now, so the visible approvals row lives in a different document
  // from the dialog. The dialog stays here, where there is room for it - a
  // modal inside a 305px column is not a place to read a payload preview.
  constructor(
      private dialog: HTMLDialogElement,
      private nav: HTMLElement|null,
      private badge: HTMLElement|null,
      private handler: FluxPageHandlerRemote) {
    this.dialog.querySelector('#approval-allow')!
        .addEventListener('click', () => this.resolve(true));
    this.dialog.querySelector('#approval-deny')!
        .addEventListener('click', () => this.resolve(false));

    // Esc closes a <dialog> by default. Treat that as a denial rather than
    // letting the run hang with no answer.
    this.dialog.addEventListener('cancel', event => {
      event.preventDefault();
      this.resolve(false);
    });

    this.nav?.addEventListener('click', () => this.showNext());
  }

  /**
   * The Approvals screen: everything blocked on this person, in one place.
   *
   * The modal is the primary surface and it is the right one - an approval is
   * urgent and interrupting is the point. This is the recovery path for when
   * that modal was missed: the console was on another screen, the tab was
   * closed, or chrome://flux was reloaded. Without it the nav row led to a
   * heading and an empty page, which is what the badge had been pointing at.
   */
  async renderScreen(root: HTMLElement) {
    root.replaceChildren();
    const screen = document.createElement('div');
    screen.className = 'screen';

    const h1 = document.createElement('h1');
    h1.textContent = 'Approvals';
    const subtitle = document.createElement('p');
    subtitle.className = 'subtitle';
    subtitle.textContent =
        'Runs that have stopped and are waiting on you. Nothing happens on ' +
        'these until you answer.';
    screen.append(h1, subtitle);

    const {approvals, questions} = await this.handler.listPending();

    if (approvals.length === 0 && questions.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'subtitle';
      empty.textContent = 'Nothing is waiting on you.';
      screen.append(empty);
      root.append(screen);
      return;
    }

    for (const request of approvals) {
      screen.append(this.approvalCard(request, root));
    }

    for (const request of questions) {
      const card = document.createElement('div');
      card.className = 'approval-card';
      const what = document.createElement('strong');
      what.textContent = request.preamble || 'The task asked you something.';
      const detail = document.createElement('p');
      detail.className = 'subtitle';
      detail.textContent = request.questions.length === 1 ?
          request.questions[0]!.text :
          `${request.questions.length} questions`;
      // Answering needs the fields, which live in the run view - so this is a
      // way through to them rather than a second copy of the panel.
      const open = document.createElement('button');
      open.className = 'primary';
      open.textContent = 'Answer';
      open.addEventListener('click', () => {
        window.location.hash = `#run/${request.runId}`;
      });
      card.append(what, detail, open);
      screen.append(card);
    }

    root.append(screen);
  }

  /** One blocked tool call, with the same two answers the dialog offers. */
  private approvalCard(request: ApprovalRequest, root: HTMLElement):
      HTMLElement {
    const card = document.createElement('div');
    card.className = 'approval-card';

    const what = document.createElement('strong');
    what.textContent = request.effectSummary || request.toolName;
    const why = document.createElement('p');
    why.className = 'subtitle';
    why.textContent = request.rationale || '';
    why.hidden = !request.rationale;
    card.append(what, why);

    if (request.payloadPreview) {
      const pre = document.createElement('pre');
      pre.className = 'approval-payload';
      pre.textContent = request.payloadPreview;
      card.append(pre);
    }

    const row = document.createElement('div');
    row.className = 'dialog-actions';
    const deny = document.createElement('button');
    deny.className = 'ghost';
    deny.textContent = "Don't do it";
    const allow = document.createElement('button');
    allow.className = 'primary';
    allow.textContent = 'Approve';
    for (const [button, approved] of
             [[deny, false], [allow, true]] as Array<[HTMLButtonElement,
                                                      boolean]>) {
      button.addEventListener('click', () => {
        this.handler.resolveApproval(request.runId, approved, null);
        this.dismissFor(request.runId);
        void this.renderScreen(root);
      });
    }
    row.append(deny, allow);
    card.append(row);
    return card;
  }

  enqueue(request: ApprovalRequest) {
    this.pending.push(request);
    this.syncBadge();
    if (!this.dialog.open) {
      this.showNext();
    }
  }

  dismissFor(runId: string) {
    this.pending = this.pending.filter(r => r.runId !== runId);
    this.syncBadge();
  }

  private showNext() {
    const request = this.pending[0];
    if (!request) {
      return;
    }

    // The effect line is what the user actually decides on, so it leads and is
    // stated in plain language by the tool itself.
    this.dialog.querySelector('#approval-effect')!.textContent =
        request.effectSummary;
    this.dialog.querySelector('#approval-rationale')!.textContent =
        request.rationale;

    const payload = this.dialog.querySelector('#approval-payload') as HTMLElement;
    if (request.payloadPreview) {
      payload.textContent = request.payloadPreview;
      payload.hidden = false;
    } else {
      payload.hidden = true;
    }

    (this.dialog.querySelector('#approval-note') as HTMLInputElement).value = '';
    this.dialog.showModal();
    // Focus the safe option, so a stray Enter denies rather than approves.
    (this.dialog.querySelector('#approval-deny') as HTMLElement).focus();
  }

  private resolve(approved: boolean) {
    const request = this.pending.shift();
    if (!request) {
      this.dialog.close();
      return;
    }
    const note =
        (this.dialog.querySelector('#approval-note') as HTMLInputElement).value;

    this.handler.resolveApproval(request.runId, approved, note || null);
    this.dialog.close();
    this.syncBadge();

    if (this.pending.length > 0) {
      this.showNext();
    }
  }

  private syncBadge() {
    if (this.badge) {
      this.badge.textContent = String(this.pending.length);
    }
    if (this.nav) {
      this.nav.hidden = this.pending.length === 0;
    }
  }
}
