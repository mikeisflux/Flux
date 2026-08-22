// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import type {FluxPageHandlerRemote, LearnedFact} from './flux.mojom-webui.js';

import {loadSkills, related, renderMarkdown} from './skills.js';
import type {Skill} from './skills.js';

/**
 * Customize - the two things the agent carries between tasks.
 *
 * Instructions is a memory store both sides write to. The reference keeps one
 * buffer for both, which is admirably transparent and also lets the agent
 * quietly overwrite what the user wrote with no way to tell afterwards who
 * said what. Here it is still one screen and still fully editable, but the
 * agent's conclusions are a separate, individually dismissable list carrying
 * the run that produced them.
 */
export class CustomizeView {
  private all: Skill[] = [];
  private adopted = new Set<string>();
  private selected: Skill|null = null;
  private detail!: HTMLElement;
  private list!: HTMLElement;

  constructor(private handler: FluxPageHandlerRemote) {}

  async render(root: HTMLElement, section: 'instructions'|'skills') {
    root.replaceChildren();
    root.classList.add('two-column');

    const rail = document.createElement('nav');
    rail.className = 'sub-nav';
    rail.setAttribute('aria-label', 'Customize');
    const title = document.createElement('h2');
    title.className = 'sub-nav-title';
    title.textContent = 'Customize';
    rail.append(title);
    for (const [id, label] of
             [['instructions', 'Instructions'], ['skills', 'Skills']]) {
      const a = document.createElement('a');
      a.className = 'sub-nav-item';
      a.textContent = label!;
      a.href = `#customize/${id}`;
      if (id === section) {
        a.setAttribute('aria-current', 'page');
      }
      rail.append(a);
    }
    root.append(rail);

    if (section === 'skills') {
      await this.renderSkills(root);
    } else {
      await this.renderInstructions(root);
    }
  }

  // --- Instructions --------------------------------------------------------

  private async renderInstructions(root: HTMLElement) {
    const screen = document.createElement('div');
    screen.className = 'screen';

    const h1 = document.createElement('h1');
    h1.textContent = 'Instructions';

    const subtitle = document.createElement('p');
    subtitle.className = 'subtitle';
    subtitle.textContent =
        'Anything Flux should know on every task. Context about you, how you ' +
        'like things done, things to avoid.';

    const area = document.createElement('textarea');
    area.className = 'instructions';
    area.placeholder = 'e.g. "Keep answers short and direct."';

    const actions = document.createElement('div');
    actions.className = 'right-actions';
    const status = document.createElement('span');
    status.className = 'muted';
    const save = document.createElement('button');
    save.className = 'primary';
    save.textContent = 'Save';
    save.disabled = true;
    actions.append(status, save);

    const {text, learned} = await this.handler.getInstructions();
    area.value = text;

    // Disabled until the text actually differs, so the button means something.
    let saved = text;
    const sync = () => {
      save.disabled = area.value === saved;
      status.textContent = '';
    };
    area.addEventListener('input', sync);
    save.addEventListener('click', () => {
      this.handler.setInstructions(area.value);
      saved = area.value;
      status.textContent = 'Saved.';
      save.disabled = true;
    });

    screen.append(h1, subtitle, area, actions,
                  this.learnedSection(learned));
    root.append(screen);
  }

  private learnedSection(learned: LearnedFact[]): HTMLElement {
    const section = document.createElement('section');
    section.className = 'learned';

    const h2 = document.createElement('h2');
    h2.className = 'section-title';
    h2.textContent = 'What Flux has learned';

    const note = document.createElement('p');
    note.className = 'subtitle';
    note.textContent =
        'Written by the agent, not by you. Each entry says which run it came ' +
        'from, and removing one is how you disagree with it.';

    section.append(h2, note);

    if (learned.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'muted';
      empty.textContent = 'Nothing yet. It fills in as tasks run.';
      section.append(empty);
      return section;
    }

    for (const fact of learned) {
      const row = document.createElement('div');
      row.className = 'learned-row';

      const text = document.createElement('p');
      text.textContent = fact.text;

      const provenance = document.createElement('span');
      provenance.className = 'muted';
      provenance.textContent = `from run ${fact.sourceRunId.slice(0, 8)}`;

      const drop = document.createElement('button');
      drop.className = 'link-button';
      drop.textContent = 'Remove';
      drop.addEventListener('click', () => {
        this.handler.dismissLearnedFact(fact.id);
        row.remove();
      });

      row.append(text, provenance, drop);
      section.append(row);
    }
    return section;
  }

  // --- Skills --------------------------------------------------------------

  private async renderSkills(root: HTMLElement) {
    this.all = await loadSkills();
    const {commands} = await this.handler.listAdoptedSkills();
    this.adopted = new Set(commands);

    // The only four-column screen in the product: shell, Customize nav, the
    // skill list, and the detail pane.
    root.classList.add('four-column');

    this.list = document.createElement('div');
    this.list.className = 'skill-list';

    this.detail = document.createElement('div');
    this.detail.className = 'skill-detail';

    root.append(this.list, this.detail);
    this.paintList();
    this.paintDetail();
  }

  private paintList() {
    this.list.replaceChildren();

    const head = document.createElement('div');
    head.className = 'skill-list-head';
    const h2 = document.createElement('h2');
    h2.textContent = 'Skills';
    h2.title =
        'Know-how the agent applies on its own when a task calls for it. ' +
        'Adopting one copies it into your set so you can edit it.';
    head.append(h2);
    this.list.append(head);

    const mine = this.all.filter(s => this.adopted.has(s.command));
    // Suggested is ranked, not fixed: with nothing adopted the reference shows
    // one category's skills, which is what ranking by an empty profile looks
    // like. Ranked here by how well-authored the body is, because a skeleton
    // body is worse than no skill at all.
    const suggested = this.all.filter(s => !this.adopted.has(s.command))
                          .sort((a, b) => Number(b.bodyStatus === 'authored') -
                                    Number(a.bodyStatus === 'authored'))
                          .slice(0, 12);

    this.list.append(this.group('Your skills', mine, 'None yet'));
    this.list.append(this.group('Suggested', suggested, ''));
  }

  private group(label: string, skills: Skill[], empty: string): HTMLElement {
    const group = document.createElement('div');
    group.className = 'skill-group';

    const h3 = document.createElement('h3');
    h3.textContent = label;
    group.append(h3);

    if (skills.length === 0) {
      const none = document.createElement('p');
      none.className = 'muted';
      none.textContent = empty;
      group.append(none);
      return group;
    }

    for (const s of skills) {
      const row = document.createElement('button');
      row.className = 'skill-row';
      row.textContent = s.name;
      if (this.selected?.command === s.command) {
        row.setAttribute('aria-current', 'true');
      }
      if (s.bodyStatus === 'skeleton') {
        const flag = document.createElement('span');
        flag.className = 'muted';
        flag.textContent = 'outline';
        flag.title =
            'This skill has a real trigger but a placeholder body. Adopting ' +
            'it gives the agent less than nothing to go on.';
        row.append(flag);
      }
      row.addEventListener('click', () => {
        this.selected = s;
        this.paintList();
        this.paintDetail();
      });
      group.append(row);
    }
    return group;
  }

  private paintDetail() {
    this.detail.replaceChildren();

    if (!this.selected) {
      const empty = document.createElement('div');
      empty.className = 'empty-state';
      empty.innerHTML =
          '<svg class="empty-icon" viewBox="0 0 32 32" aria-hidden="true">' +
          '<path d="M16 4l12 6-12 6-12-6 12-6zm12 12l-12 6-12-6m24 6l-12 6' +
          '-12-6"/></svg>';
      const h2 = document.createElement('h2');
      h2.textContent = 'Skills';
      const p = document.createElement('p');
      p.textContent =
          'Know-how Flux applies on its own when a task calls for it. ' +
          'Writing a document in your format, say, or checking a figure the ' +
          'way your team checks it.';
      empty.append(h2, p);
      this.detail.append(empty);
      return;
    }

    const s = this.selected;

    const head = document.createElement('div');
    head.className = 'skill-head';
    const tile = document.createElement('span');
    tile.className = 'skill-tile';
    tile.textContent = s.name.slice(0, 1);
    const heading = document.createElement('div');
    const h1 = document.createElement('h1');
    h1.textContent = s.name;
    const desc = document.createElement('p');
    desc.className = 'subtitle';
    desc.textContent = s.description;
    heading.append(h1, desc);
    head.append(tile, heading);

    const chips = document.createElement('div');
    chips.className = 'role-chips';
    // These are the data behind the Tasks screen's unexplained persona
    // dropdown; every skill carries the role array it filters on.
    for (const role of s.roles) {
      const chip = document.createElement('span');
      chip.className = 'role-chip';
      chip.textContent = `For ${role}`;
      chips.append(chip);
    }
    const scope = document.createElement('span');
    scope.className = 'trust';
    scope.dataset['scope'] = s.writeScope;
    scope.textContent = s.writeScope === 'readonly' ?
        'Reads only' :
        s.writeScope === 'draft' ? 'Writes drafts' :
        s.writeScope === 'send'  ? 'Sends' :
                                   'Spends money';
    chips.append(scope);

    this.detail.append(head, chips);

    if (s.worksWith.length > 0) {
      const works = document.createElement('div');
      works.className = 'works-with';
      const label = document.createElement('span');
      label.className = 'muted';
      label.textContent = 'Works with';
      works.append(label);
      for (const c of s.worksWith) {
        const mark = document.createElement('span');
        mark.className = 'connector-mark';
        mark.dataset['transport'] = c.transport;
        mark.textContent = c.id.slice(0, 1);
        mark.title = c.transport === 'api' ? `${c.id} - direct API` :
                                             `${c.id} - driven in the browser`;
        works.append(mark);
      }
      this.detail.append(works);
    }

    const body = document.createElement('div');
    body.className = 'skill-body';
    body.append(renderMarkdown(s.body));
    this.detail.append(body);

    const actions = document.createElement('div');
    actions.className = 'right-actions';
    if (this.adopted.has(s.command)) {
      const remove = document.createElement('button');
      remove.className = 'outlined-button';
      remove.textContent = 'Remove from my skills';
      remove.addEventListener('click', () => {
        this.handler.removeSkill(s.command);
        this.adopted.delete(s.command);
        this.paintList();
        this.paintDetail();
      });
      actions.append(remove);
    } else {
      const add = document.createElement('button');
      add.className = 'primary';
      add.textContent = '+ Add to my skills';
      add.addEventListener('click', () => this.openAdoptForm(s));
      actions.append(add);
    }
    this.detail.append(actions);

    const siblings = related(this.all, s, 3);
    if (siblings.length > 0) {
      const rel = document.createElement('div');
      rel.className = 'related';
      const label = document.createElement('span');
      label.className = 'muted';
      label.textContent = 'Related skills';
      rel.append(label);
      for (const other of siblings) {
        const link = document.createElement('button');
        link.className = 'link-button';
        link.textContent = other.name;
        link.addEventListener('click', () => {
          this.selected = other;
          this.paintList();
          this.paintDetail();
        });
        rel.append(link);
      }
      this.detail.append(rel);
    }
  }

  // --- Add to my skills ----------------------------------------------------

  private openAdoptForm(skill: Skill) {
    const dialog = document.createElement('dialog');
    dialog.className = 'approval adopt';

    const h2 = document.createElement('h2');
    h2.textContent = 'Add to my skills';
    const note = document.createElement('p');
    note.className = 'subtitle';
    note.textContent =
        'Flux will apply this on its own when it is relevant. Edit anything ' +
        'before saving.';

    const command = field(dialog, 'Command', skill.command);
    const name = field(dialog, 'Name', skill.name);
    const description = field(dialog, 'Description', skill.description);

    const instructions = document.createElement('textarea');
    instructions.className = 'instructions';
    instructions.value = skill.body;
    const instructionsLabel = document.createElement('label');
    instructionsLabel.className = 'form-field';
    instructionsLabel.append('Instructions', instructions);

    const error = document.createElement('p');
    error.className = 'composer-status error';
    error.hidden = true;

    const actions = document.createElement('div');
    actions.className = 'approval-actions';
    const cancel = document.createElement('button');
    cancel.className = 'ghost';
    cancel.textContent = 'Cancel';
    cancel.addEventListener('click', () => dialog.close());
    const add = document.createElement('button');
    add.className = 'primary';
    add.textContent = 'Add skill';
    add.addEventListener('click', async () => {
      const {adopted, error: why} = await this.handler.adoptSkill(
          command.value, name.value, description.value, instructions.value);
      if (!adopted) {
        error.hidden = false;
        error.textContent = why ?? 'Could not add that skill.';
        return;
      }
      this.adopted.add(command.value);
      dialog.close();
      dialog.remove();
      this.paintList();
      this.paintDetail();
    });
    actions.append(cancel, add);

    dialog.prepend(h2, note);
    dialog.append(instructionsLabel, error, actions);
    document.body.append(dialog);
    dialog.showModal();
    dialog.addEventListener('close', () => dialog.remove());
  }
}

function field(parent: HTMLElement, label: string, value: string):
    HTMLInputElement {
  const wrap = document.createElement('label');
  wrap.className = 'form-field';
  const input = document.createElement('input');
  input.type = 'text';
  input.value = value;
  wrap.append(label, input);
  parent.append(wrap);
  return input;
}
