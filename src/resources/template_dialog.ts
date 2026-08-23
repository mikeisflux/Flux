// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {hasAuthoredPrompt, placeholders, promptFor, transportLabel, trustLabel} from './catalog.js';
import type {Template} from './catalog.js';
import {connectorMark} from './icons.js';

/**
 * One template, opened.
 *
 * The prompt is the whole point of this view. A card can say what a task
 * produces; only this can show what the agent will actually be told, which is
 * the thing a person needs to read before they trust it with their inbox.
 *
 * Placeholders are marked rather than merely present. A template run with
 * "[sheet link]" still in it fails on its first step, and the failure looks
 * like the agent being stupid rather than the user having skipped a blank.
 */
export class TemplateDialog {
  private dialog: HTMLDialogElement;
  private onUse: (template: Template, prompt: string) => void;
  private onSaveWorkflow: (template: Template, prompt: string) => void;

  constructor(
      onUse: (template: Template, prompt: string) => void,
      onSaveWorkflow: (template: Template, prompt: string) => void) {
    this.onUse = onUse;
    this.onSaveWorkflow = onSaveWorkflow;
    this.dialog = document.createElement('dialog');
    this.dialog.className = 'flux-dialog template-dialog';
    document.body.append(this.dialog);
  }

  open(template: Template, related: Template[]) {
    this.dialog.replaceChildren();

    const body = document.createElement('div');
    body.className = 'dialog-body';

    // Header: the mark of the first service it touches, then the title.
    const head = document.createElement('div');
    head.className = 'template-head';
    const first = template.connectors[0];
    if (first) {
      head.append(connectorMark(first.id, 'connector-avatar'));
    }
    const heading = document.createElement('div');
    const title = document.createElement('h2');
    title.textContent = template.title;
    const outcome = document.createElement('p');
    outcome.className = 'subtitle';
    outcome.textContent = template.outcome;
    heading.append(title, outcome);
    head.append(heading);

    const pills = document.createElement('div');
    pills.className = 'template-pills';
    for (const role of template.roles ?? []) {
      const pill = document.createElement('span');
      pill.className = 'pill static';
      pill.textContent = `For ${role}`;
      pills.append(pill);
    }
    if (template.schedule) {
      const pill = document.createElement('span');
      pill.className = 'pill static';
      pill.textContent = template.schedule.display;
      pill.title = template.schedule.cron;
      pills.append(pill);
    }
    const trust = document.createElement('span');
    trust.className = 'trust';
    trust.dataset['scope'] = template.writeScope;
    trust.textContent = trustLabel(template.writeScope);
    pills.append(trust);

    const promptLabel = document.createElement('h3');
    promptLabel.className = 'section-title';
    promptLabel.textContent = 'Prompt';

    // Editable, and it is the same text the two buttons below act on. A
    // read-only preview beside a button that runs something slightly different
    // is how a person stops trusting the preview.
    const prompt = document.createElement('textarea');
    prompt.className = 'template-prompt';
    const text = promptFor(template);
    prompt.value = text;
    prompt.rows = Math.min(16, text.split('\n').length + 3);

    const blanks = placeholders(text);
    const hint = document.createElement('p');
    hint.className = 'muted';
    if (!hasAuthoredPrompt(template)) {
      // Said plainly. A derived prompt will run, but it is a starting point
      // and the user should know that before it touches their inbox.
      hint.textContent =
          'This one has no written prompt yet - the text above is a starting ' +
          'point built from the template. Edit it before running.';
    } else {
      hint.textContent = blanks.length > 0 ?
          `Fill in ${blanks.length === 1 ? 'the blank' : 'the blanks'}: ${
              blanks.map(b => `[${b}]`).join(', ')}` :
          'Nothing to fill in - this one is ready to run.';
    }

    const sitesLabel = document.createElement('h3');
    sitesLabel.className = 'section-title';
    sitesLabel.textContent = 'Sites';
    const sites = document.createElement('div');
    sites.className = 'connector-marks';
    for (const connector of template.connectors) {
      const mark = connectorMark(connector.id, 'connector-mark');
      mark.dataset['transport'] = connector.transport;
      mark.title = transportLabel(connector.id, connector.transport);
      sites.append(mark);
    }

    const actions = document.createElement('div');
    actions.className = 'template-actions';
    const save = document.createElement('button');
    save.className = 'primary';
    save.textContent = 'Save as workflow';
    save.addEventListener('click', () => {
      this.dialog.close();
      this.onSaveWorkflow(template, prompt.value);
    });
    const use = document.createElement('button');
    use.className = 'button';
    use.textContent = 'Use this template';
    use.addEventListener('click', () => {
      this.dialog.close();
      this.onUse(template, prompt.value);
    });
    actions.append(save, use);

    body.append(head, pills, promptLabel, prompt, hint, sitesLabel, sites,
                actions);

    if (related.length > 0) {
      const relatedLabel = document.createElement('h3');
      relatedLabel.className = 'section-title';
      relatedLabel.textContent = 'Related templates';
      const list = document.createElement('div');
      list.className = 'related-list';
      for (const other of related) {
        const link = document.createElement('button');
        link.className = 'related-item';
        link.textContent = other.title;
        link.addEventListener('click', () => this.open(other, related));
        list.append(link);
      }
      body.append(relatedLabel, list);
    }

    const close = document.createElement('button');
    close.className = 'ghost-icon dialog-close';
    close.setAttribute('aria-label', 'Close');
    close.textContent = '✕';
    close.addEventListener('click', () => this.dialog.close());
    body.append(close);

    this.dialog.append(body);
    this.dialog.showModal();
  }
}
