// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {Provider} from './flux.mojom-webui.js';
import type {FluxPageHandlerRemote} from './flux.mojom-webui.js';

import {loadSkills} from './skills.js';
import type {Skill} from './skills.js';

/**
 * First run.
 *
 * Three steps, in the order that decides whether the product works at all: a
 * model key, a skill set, and a first task. The key is first because without
 * one every other screen is a catalogue of things that will fail, and finding
 * that out three clicks in is the worst version of this.
 *
 * Nothing here is skippable-by-accident but everything is skippable on
 * purpose - a setup flow that traps you is worse than one you leave half done.
 */
export class WelcomeView {
  private step = 0;
  private root!: HTMLElement;
  private skills: Skill[] = [];
  private chosen = new Set<string>();

  constructor(private handler: FluxPageHandlerRemote) {}

  async render(root: HTMLElement) {
    this.skills = await loadSkills();
    root.replaceChildren();
    root.classList.remove('two-column', 'four-column');
    this.root = document.createElement('div');
    this.root.className = 'screen welcome';
    root.append(this.root);
    await this.paint();
  }

  private async paint() {
    this.root.replaceChildren();
    this.root.append(this.brand(), this.dots());

    switch (this.step) {
      case 0:
        await this.stepKey();
        break;
      case 1:
        this.stepSkills();
        break;
      default:
        this.stepDone();
        break;
    }
  }

  private brand(): HTMLElement {
    const brand = document.createElement('div');
    brand.className = 'brand';
    const mark = document.createElement('img');
    mark.className = 'brand-mark';
    mark.src = 'flux-mark.svg';
    mark.alt = '';
    const name = document.createElement('span');
    name.className = 'brand-name';
    name.textContent = 'Flux';
    brand.append(mark, name);
    return brand;
  }

  private dots(): HTMLElement {
    const dots = document.createElement('div');
    dots.className = 'steps';
    for (let i = 0; i < 3; i++) {
      const dot = document.createElement('span');
      dot.className = 'step-dot';
      if (i === this.step) {
        dot.dataset['current'] = '';
      } else if (i < this.step) {
        dot.dataset['done'] = '';
      }
      dots.append(dot);
    }
    return dots;
  }

  private heading(title: string, body: string) {
    const h1 = document.createElement('h1');
    h1.className = 'display';
    h1.textContent = title;
    const p = document.createElement('p');
    p.className = 'subtitle centered';
    p.textContent = body;
    this.root.append(h1, p);
  }

  /**
   * A message above the step's controls. Inserted rather than kept as state so
   * the next repaint clears it - first run moves forward, and a warning about
   * the previous step should not follow the user into the next one.
   */
  private notice(message: string) {
    let banner = this.root.querySelector<HTMLElement>('.inline-error');
    if (!banner) {
      banner = document.createElement('div');
      banner.className = 'inline-error';
      banner.setAttribute('role', 'alert');
      this.root.append(banner);
    }
    banner.textContent = message;
  }

  private nav(nextLabel: string, next: () => void, skip = true) {
    const nav = document.createElement('div');
    nav.className = 'welcome-nav';
    if (skip) {
      const later = document.createElement('button');
      later.className = 'link-button';
      later.textContent = 'Skip for now';
      later.addEventListener('click', () => {
        this.step += 1;
        void this.paint();
      });
      nav.append(later);
    }
    const go = document.createElement('button');
    go.className = 'primary accent';
    go.textContent = nextLabel;
    go.addEventListener('click', next);
    nav.append(go);
    this.root.append(nav);
  }

  // --- 1. A model key ------------------------------------------------------

  private async stepKey() {
    this.heading(
        'First, a model key',
        'Flux runs on Claude or GPT and calls the provider directly with your ' +
            'key. Nothing routes through us, and the key is stored encrypted ' +
            'by the browser, never sent to the page.');

    const {statuses} = await this.handler.listProviderKeys();
    const configured = statuses.some(s => s.configured);

    const field = document.createElement('div');
    field.className = 'welcome-field';

    const select = document.createElement('select');
    select.className = 'depth';
    for (const [value, label] of
             [[String(Provider.kAnthropic), 'Anthropic'],
              [String(Provider.kOpenAI), 'OpenAI']]) {
      const option = document.createElement('option');
      option.value = value!;
      option.textContent = label!;
      select.append(option);
    }

    const key = document.createElement('input');
    key.type = 'password';
    key.placeholder = 'Paste an API key';
    key.autocomplete = 'off';

    const status = document.createElement('p');
    status.className = 'composer-status';
    status.hidden = !configured;
    if (configured) {
      status.textContent = 'A key is already stored. You can move on.';
    }

    field.append(select, key);
    this.root.append(field, status);

    this.nav('Save and continue', async () => {
      if (!key.value.trim()) {
        this.step += 1;
        void this.paint();
        return;
      }
      status.hidden = false;
      status.classList.remove('error');
      // Validated against the provider before it is stored, so a wrong key is
      // found here rather than three steps into the first real task.
      status.textContent = 'Checking the key with the provider...';
      const provider = Number(select.value) as Provider;
      const {stored, error} =
          await this.handler.setProviderKey(provider, key.value.trim());
      if (!stored) {
        status.classList.add('error');
        status.textContent = error ?? 'That key was not accepted.';
        return;
      }
      this.step += 1;
      void this.paint();
    });
  }

  // --- 2. Skills -----------------------------------------------------------

  private stepSkills() {
    this.heading(
        'Pick a few skills',
        'Skills are know-how Flux applies on its own when a task calls for ' +
            'it. Start with a couple; you can adopt more, or write your own, ' +
            'under Customize.');

    const grid = document.createElement('div');
    grid.className = 'welcome-picks';
    // Only well-authored bodies are offered here. A skeleton body gives the
    // agent less than nothing to go on, and a first run is the worst possible
    // place to find that out.
    for (const skill of this.skills.filter(s => s.bodyStatus === 'authored')
             .slice(0, 8)) {
      const pick = document.createElement('button');
      pick.className = 'pick';
      const name = document.createElement('strong');
      name.textContent = skill.name;
      const desc = document.createElement('span');
      desc.textContent = skill.description;
      pick.append(name, desc);
      pick.addEventListener('click', () => {
        if (this.chosen.has(skill.command)) {
          this.chosen.delete(skill.command);
          pick.removeAttribute('data-chosen');
        } else {
          this.chosen.add(skill.command);
          pick.dataset['chosen'] = '';
        }
      });
      grid.append(pick);
    }
    this.root.append(grid);

    this.nav('Add these', async () => {
      // The result was discarded, so a skill that failed to adopt advanced the
      // user to "done" having added nothing. First run is exactly where that
      // is least recoverable: they have no reason to suspect anything failed.
      const failed: string[] = [];
      for (const command of this.chosen) {
        const skill = this.skills.find(s => s.command === command);
        if (!skill) {
          continue;
        }
        const {adopted} = await this.handler.adoptSkill(
            skill.command, skill.name, skill.description, skill.body);
        if (!adopted) {
          failed.push(skill.name);
        }
      }
      if (failed.length > 0) {
        // Named rather than counted, and the flow still advances: these are
        // recoverable from Customize, and blocking first run over them would
        // be worse than telling the user which ones to add later.
        this.notice(
            `Could not add ${failed.join(', ')}. You can add ${
                failed.length === 1 ? 'it' : 'them'} from Customize.`);
      }
      this.step += 1;
      void this.paint();
    });
  }

  // --- 3. Done -------------------------------------------------------------

  private stepDone() {
    this.heading(
        'That is the setup',
        'The console lives in the column on the left, on every window. Type a ' +
            'task on a new tab, or start from one of 250 in Templates.');

    const nav = document.createElement('div');
    nav.className = 'welcome-nav';
    const templates = document.createElement('a');
    templates.className = 'outlined';
    templates.href = '#templates/tasks';
    templates.textContent = 'Browse templates';
    const start = document.createElement('a');
    start.className = 'primary accent';
    start.href = '#new-task';
    start.textContent = 'Start a task';
    nav.append(templates, start);
    this.root.append(nav);
  }
}
