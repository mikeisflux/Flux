// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {Provider, WriteScope} from './flux.mojom-webui.js';
import type {FluxPageHandlerRemote, WorkflowDraft, WorkflowSummary} from
    './flux.mojom-webui.js';

/**
 * The Save workflow dialog.
 *
 * Reached from two places - "New workflow" on the workflows screen, and "Save
 * as workflow" on a template - so it takes a partial draft and does not care
 * which. Editing an existing workflow is the same dialog with its id filled
 * in, because the fields are identical and a separate Edit form is a second
 * place for them to disagree.
 */

/** What the caller knows before the dialog opens. */
export interface WorkflowSeed {
  id?: string;
  command?: string;
  name?: string;
  description?: string;
  instructions?: string;
  cron?: string;
  scheduleDisplay?: string;
  templateId?: string|null;
  writeScope?: WriteScope;
}

// The schedules the dialog offers, and the cron each one means. Presets rather
// than a cron field alone: five-field cron is a foot-gun, and a workflow that
// silently never fires because of a typo is the failure this product can least
// afford. The expression is still shown, and still editable, for anyone who
// wants it.
const PRESETS: Array<{label: string, cron: string}> = [
  {label: 'Only when I run it', cron: ''},
  {label: 'Every hour', cron: '0 * * * *'},
  {label: 'Every day at 7am', cron: '0 7 * * *'},
  {label: 'Every day at 9am', cron: '0 9 * * *'},
  {label: 'Weekdays at 8am', cron: '0 8 * * 1-5'},
  {label: 'Weekdays at 5pm', cron: '0 17 * * 1-5'},
  {label: 'Mondays at 9am', cron: '0 9 * * 1'},
  {label: 'First of the month at 9am', cron: '0 9 1 * *'},
];

// The credits are the per-run ceiling, and they are not optional: a spec with
// a zero budget is refused outright by the service ("Task has no credit
// budget"), which is the correct thing for it to do and meant that every
// workflow saved here was unrunnable from the moment it was created. The
// values mirror the Quick/Medium/Thorough ladder the composer uses, because
// this is the same choice wearing a different label.
const MODES: Array<{
  label: string,
  model: string,
  tokens: number,
  credits: bigint,
}> = [
  {label: 'Fast', model: 'claude-haiku-4-5-20251001', tokens: 4096,
   credits: 25_000n},
  {label: 'Medium', model: 'claude-sonnet-5', tokens: 8192, credits: 100_000n},
  {label: 'Thorough', model: 'claude-opus-5', tokens: 16384, credits: 500_000n},
];

export class WorkflowDialog {
  private dialog: HTMLDialogElement;
  private handler: FluxPageHandlerRemote;
  private onSaved: () => void;

  constructor(handler: FluxPageHandlerRemote, onSaved: () => void) {
    this.handler = handler;
    this.onSaved = onSaved;
    this.dialog = document.createElement('dialog');
    this.dialog.className = 'flux-dialog';
    document.body.append(this.dialog);
  }

  /** Opens the dialog over whatever is on screen. */
  open(seed: WorkflowSeed) {
    this.dialog.replaceChildren();

    const form = document.createElement('div');
    form.className = 'dialog-body';

    const head = document.createElement('div');
    head.className = 'dialog-head';
    const title = document.createElement('h2');
    title.textContent = seed.id ? 'Edit workflow' : 'Save workflow';
    const close = document.createElement('button');
    close.className = 'ghost-icon';
    close.setAttribute('aria-label', 'Close');
    close.textContent = '✕';
    close.addEventListener('click', () => this.dialog.close());
    head.append(title, close);

    const command = field('Command', 'input');
    const commandInput = command.control as HTMLInputElement;
    commandInput.value = seed.command ?? '';
    commandInput.placeholder = 'weekly-pipeline-report';

    const mode = field('Mode', 'select');
    const modeSelect = mode.control as HTMLSelectElement;
    for (const m of MODES) {
      const option = document.createElement('option');
      option.value = m.model;
      option.textContent = m.label;
      modeSelect.append(option);
    }
    modeSelect.value = MODES[1]!.model;

    const row = document.createElement('div');
    row.className = 'dialog-row';
    row.append(command.wrap, mode.wrap);

    const description = field('Description', 'input');
    (description.control as HTMLInputElement).value = seed.description ?? '';

    const instructions = field('Instructions', 'textarea');
    const instructionsBox = instructions.control as HTMLTextAreaElement;
    instructionsBox.rows = 8;
    instructionsBox.value = seed.instructions ?? '';
    instructionsBox.placeholder =
        'What should Flux do each time this runs? Anything in [brackets] is ' +
        'yours to fill in before saving.';

    const schedule = field('Schedule', 'select');
    const scheduleSelect = schedule.control as HTMLSelectElement;
    for (const preset of PRESETS) {
      const option = document.createElement('option');
      option.value = preset.cron;
      option.textContent = preset.label;
      scheduleSelect.append(option);
    }
    scheduleSelect.value =
        PRESETS.some(p => p.cron === seed.cron) ? (seed.cron ?? '') : '';

    // The expression the preset means, editable. Shown always rather than
    // behind a toggle: a schedule the user cannot read is one they cannot
    // check, and "Mondays at 9am" in whose timezone is a fair question.
    const cron = field('Cron (local time)', 'input');
    const cronInput = cron.control as HTMLInputElement;
    cronInput.value = seed.cron ?? '';
    cronInput.placeholder = 'blank = only when I run it';
    scheduleSelect.addEventListener('change', () => {
      cronInput.value = scheduleSelect.value;
    });

    const scheduleRow = document.createElement('div');
    scheduleRow.className = 'dialog-row';
    scheduleRow.append(schedule.wrap, cron.wrap);

    const error = document.createElement('p');
    error.className = 'dialog-error';
    error.hidden = true;

    const actions = document.createElement('div');
    actions.className = 'dialog-actions';
    const cancel = document.createElement('button');
    cancel.className = 'ghost';
    cancel.textContent = 'Cancel';
    cancel.addEventListener('click', () => this.dialog.close());
    const save = document.createElement('button');
    save.className = 'primary';
    save.textContent = seed.id ? 'Save changes' : 'Save workflow';
    actions.append(cancel, save);

    save.addEventListener('click', () => {
      const chosen = MODES.find(m => m.model === modeSelect.value) ?? MODES[1]!;
      const label = PRESETS.find(p => p.cron === cronInput.value.trim());
      const draft: WorkflowDraft = {
        id: seed.id ?? '',
        command: commandInput.value.trim(),
        name: seed.name ?? (description.control as HTMLInputElement).value,
        description: (description.control as HTMLInputElement).value,
        cron: cronInput.value.trim(),
        scheduleDisplay: label ? label.label : cronInput.value.trim(),
        enabled: true,
        spec: {
          prompt: instructionsBox.value,
          templateId: seed.templateId ?? null,
          writeScope: seed.writeScope ?? WriteScope.kReadOnly,
          model: {
            provider: Provider.kAnthropic,
            model: chosen.model,
            maxOutputTokens: chosen.tokens,
            allowFailover: true,
          },
          profileId: '',
          creditBudget: chosen.credits,
        },
      };

      save.disabled = true;
      void this.handler.saveWorkflow(draft).then(result => {
        save.disabled = false;
        if (result.error) {
          // Reported in the dialog, not a toast. The user is looking at the
          // field that caused it and the dialog stays open on their input.
          error.textContent = result.error;
          error.hidden = false;
          return;
        }
        this.dialog.close();
        this.onSaved();
      });
    });

    form.append(head, row, description.wrap, instructions.wrap, scheduleRow,
                error, actions);
    this.dialog.append(form);
    this.dialog.showModal();
    commandInput.focus();
  }

  /** Opens it on an existing workflow, for the row menu's Edit. */
  edit(workflow: WorkflowSummary, instructions: string) {
    this.open({
      id: workflow.id,
      command: workflow.command,
      name: workflow.name,
      description: workflow.description,
      instructions,
      cron: workflow.cron,
      scheduleDisplay: workflow.scheduleDisplay,
      writeScope: workflow.writeScope,
    });
  }
}

function field(label: string, kind: 'input'|'select'|'textarea'):
    {wrap: HTMLElement, control: HTMLElement} {
  const wrap = document.createElement('label');
  wrap.className = 'dialog-field';
  const text = document.createElement('span');
  text.textContent = label;
  const control = document.createElement(kind);
  wrap.append(text, control);
  return {wrap, control};
}
