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

  constructor(
      private dialog: HTMLDialogElement,
      private nav: HTMLElement,
      private badge: HTMLElement,
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

    this.nav.addEventListener('click', () => this.showNext());
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
    this.badge.textContent = String(this.pending.length);
    this.nav.hidden = this.pending.length === 0;
  }
}
