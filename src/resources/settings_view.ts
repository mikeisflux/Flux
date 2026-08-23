// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import type {FluxPageHandlerRemote} from './flux.mojom-webui.js';

import {SettingsView as ProviderKeys} from './settings.js';

/**
 * Settings: Profiles and General.
 *
 * Rows that belong to Chromium rather than to Flux - theme, the bookmarks bar,
 * the download directory, profile management - hand off to the page that
 * already owns them instead of being reimplemented against a second copy of
 * the pref. Two settings surfaces that disagree is worse than one that sends
 * you somewhere.
 */
export class FluxSettingsView {
  private section: 'profiles'|'general' = 'profiles';
  private providers: ProviderKeys;

  constructor(private handler: FluxPageHandlerRemote) {
    this.providers = new ProviderKeys(handler);
  }

  async render(root: HTMLElement, section?: string) {
    this.section = section === 'general' ? 'general' : 'profiles';

    root.replaceChildren();
    root.classList.remove('four-column');
    root.classList.add('two-column');

    const rail = document.createElement('nav');
    rail.className = 'sub-nav';
    rail.setAttribute('aria-label', 'Settings');
    const title = document.createElement('h2');
    title.className = 'sub-nav-title';
    title.textContent = 'Settings';
    rail.append(title);
    for (const [id, label] of [['profiles', 'Profiles'], ['general', 'General']]) {
      const a = document.createElement('a');
      a.className = 'sub-nav-item';
      a.textContent = label!;
      a.href = `#settings/${id}`;
      if (id === this.section) {
        a.setAttribute('aria-current', 'page');
      }
      rail.append(a);
    }

    const screen = document.createElement('div');
    screen.className = 'screen';
    root.append(rail, screen);

    if (this.section === 'general') {
      await this.general(screen);
    } else {
      this.profiles(screen);
    }
  }

  // --- Profiles ------------------------------------------------------------

  private profiles(screen: HTMLElement) {
    const h1 = document.createElement('h1');
    h1.textContent = 'Profiles';
    const p = document.createElement('p');
    p.className = 'subtitle';
    p.textContent =
        'Isolated browsing profiles. A task runs as one of them, so two runs ' +
        'in different profiles cannot collide over cookies or a service that ' +
        'only allows one session.';
    screen.append(h1, p);

    screen.append(row(
        'Manage profiles',
        'Add, rename, or remove a profile, and choose which one a window opens in.',
        link('Open', 'chrome://settings/manageProfile')));
    screen.append(row(
        'Import browsing data',
        'Bring bookmarks, extensions, and history from another browser.',
        link('Import', 'chrome://settings/importData')));

    const h2 = document.createElement('h2');
    h2.className = 'section-title settings-group';
    h2.textContent = 'Appearance';
    screen.append(h2);
    screen.append(row(
        'Theme', 'Flux follows your device appearance, or you can pin it.',
        link('Change', 'chrome://settings/appearance')));
    screen.append(row(
        'Show bookmarks bar', 'Display the bookmarks bar under the toolbar.',
        link('Change', 'chrome://settings/appearance')));

    const h3 = document.createElement('h2');
    h3.className = 'section-title settings-group';
    h3.textContent = 'Downloads';
    screen.append(h3);
    screen.append(row(
        'Download location and prompting',
        'Where files land, and whether Flux asks every time.',
        link('Change', 'chrome://settings/downloads')));
  }

  // --- General -------------------------------------------------------------

  private async general(screen: HTMLElement) {
    const h1 = document.createElement('h1');
    h1.textContent = 'General';
    screen.append(h1);

    screen.append(row(
        'Default browser', 'Make Flux the browser your links open in.',
        link('Change', 'chrome://settings/defaultBrowser')));

    const agent = document.createElement('h2');
    agent.className = 'section-title settings-group';
    agent.textContent = 'Agent';
    screen.append(agent);

    // Parallel tasks. The reference's Auto/Lower/Higher is unexplained; the
    // resolved number is the whole point of the setting, so it is shown.
    const {limit, active, queued} = await this.handler.getConcurrencyLimit();
    const parallel = row(
        'Parallel tasks',
        `Sized to this PC's memory and shared across running tasks. ` +
            `Currently ${limit} at once - ${active} running, ${queued} queued.`,
        segmented(['Auto', 'Lower', 'Higher'], 'Auto', 'Parallel tasks'));
    screen.append(parallel);

    const notify = document.createElement('h2');
    notify.className = 'section-title settings-group';
    notify.textContent = 'Notifications';
    screen.append(notify);

    // [FLUX] The reference notifies on completion only. An agent that is
    // blocked waiting for a human is worthless if the human is not told, and
    // that is the one event that stops the work.
    for (const [label, detail, on] of [
             ['Task finished', 'When a task completes on its own.', true],
             ['Waiting on you',
              'When a task stops for an approval. Nothing happens until you ' +
                  'answer, so this one is on by default.',
              true],
             ['Task failed', 'When a task stops because something broke.', true],
             ['Budget reached',
              'When a task hits the credit ceiling you set for it.', true],
    ] as Array<[string, string, boolean]>) {
      screen.append(row(label, detail, toggle(on, label)));
    }

    const keys = document.createElement('h2');
    keys.className = 'section-title settings-group';
    keys.textContent = 'Keyboard';
    screen.append(keys);
    screen.append(row(
        'Command panel',
        'Search every task, skill and screen, scoped to the tab in front of you.',
        kbd('Ctrl+K')));

    const models = document.createElement('h2');
    models.className = 'section-title settings-group';
    models.textContent = 'Models';
    screen.append(models);
    const providerHost = document.createElement('div');
    screen.append(providerHost);
    await this.providers.render(providerHost);
  }
}

function row(label: string, detail: string, control: HTMLElement): HTMLElement {
  const el = document.createElement('div');
  el.className = 'setting-row';
  const text = document.createElement('div');
  const name = document.createElement('strong');
  name.textContent = label;
  const desc = document.createElement('span');
  desc.textContent = detail;
  text.append(name, desc);
  el.append(text, control);
  return el;
}

function link(label: string, href: string): HTMLElement {
  const a = document.createElement('a');
  a.className = 'outlined';
  a.href = href;
  a.target = '_blank';
  a.textContent = label;
  return a;
}

/**
 * One choice out of several, as a radio group rather than three buttons.
 *
 * data-active is a CSS hook and nothing more, so without the ARIA the group
 * announces as "Auto, button / Lower, button / Higher, button": no name saying
 * what is being chosen, and no indication that one of them is already the
 * answer. The visual state and the announced state have to be set together -
 * that is the whole reason aria-checked is written on the same lines that add
 * and remove the attribute.
 */
function segmented(
    options: string[], current: string, label: string): HTMLElement {
  const wrap = document.createElement('div');
  wrap.className = 'segmented';
  wrap.setAttribute('role', 'radiogroup');
  wrap.setAttribute('aria-label', label);
  for (const option of options) {
    const b = document.createElement('button');
    b.textContent = option;
    b.setAttribute('role', 'radio');
    b.setAttribute('aria-checked', String(option === current));
    if (option === current) {
      b.dataset['active'] = '';
    }
    b.addEventListener('click', () => {
      for (const other of wrap.children) {
        other.removeAttribute('data-active');
        other.setAttribute('aria-checked', 'false');
      }
      b.dataset['active'] = '';
      b.setAttribute('aria-checked', 'true');
    });
    wrap.append(b);
  }
  return wrap;
}

/**
 * A switch, named after the row it sits in.
 *
 * The name is not decoration. The visible label lives in a sibling div, which
 * associates them for anyone who can see the layout and for nobody else: four
 * of these in the Notifications group announced as "switch, on" four times,
 * with no way to tell "Task finished" from "Task failed". aria-label is what
 * carries the row's text across to the control.
 */
function toggle(on: boolean, label: string): HTMLElement {
  const b = document.createElement('button');
  b.className = 'toggle';
  b.setAttribute('role', 'switch');
  b.setAttribute('aria-label', label);
  b.setAttribute('aria-checked', String(on));
  b.addEventListener('click', () => {
    b.setAttribute(
        'aria-checked', b.getAttribute('aria-checked') === 'true' ? 'false' :
                                                                    'true');
  });
  return b;
}

function kbd(keys: string): HTMLElement {
  const el = document.createElement('kbd');
  el.textContent = keys;
  return el;
}
