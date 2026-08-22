// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {Provider} from './flux.mojom-webui.js';
import type {
  FluxPageHandlerRemote,
  ProviderKeyStatus,
} from './flux.mojom-webui.js';

const PROVIDERS = [
  {
    id: Provider.kAnthropic,
    name: 'Claude',
    vendor: 'Anthropic',
    keysUrl: 'https://console.anthropic.com/settings/keys',
    prefix: 'sk-ant-',
    note: 'Billed per token by Anthropic. This is separate from a Claude ' +
          'Pro or Max subscription — those cannot be used by third-party apps.',
  },
  {
    id: Provider.kOpenAI,
    name: 'OpenAI',
    vendor: 'OpenAI',
    keysUrl: 'https://platform.openai.com/api-keys',
    prefix: 'sk-',
    note: 'Billed per token by OpenAI. Separate from a ChatGPT Plus ' +
          'subscription, which cannot be used by third-party apps.',
  },
];

/**
 * Provider credential settings.
 *
 * The key is write-only from the console's perspective: it goes to the browser
 * process and never comes back. What returns is a status and a four-character
 * hint, which is enough to tell two keys apart and worth nothing if leaked.
 */
export class SettingsView {
  constructor(private handler: FluxPageHandlerRemote) {}

  async render(container: HTMLElement) {
    container.replaceChildren();

    const h1 = document.createElement('h1');
    h1.textContent = 'Agent';
    const intro = document.createElement('p');
    intro.className = 'settings-intro';
    intro.textContent =
        'Flux needs an API key to run tasks. Add one for either provider, ' +
        'or both — tasks can pick per run, and fall back to the other if one ' +
        'is rate limited.';
    container.append(h1, intro);

    const {statuses} = await this.handler.listProviderKeys();
    for (const meta of PROVIDERS) {
      const status = statuses.find(s => s.provider === meta.id);
      container.append(this.renderProvider(meta, status));
    }
  }

  private renderProvider(
      meta: typeof PROVIDERS[number],
      status: ProviderKeyStatus|undefined): HTMLElement {
    const card = document.createElement('section');
    card.className = 'provider-card';

    const head = document.createElement('div');
    head.className = 'provider-head';
    const title = document.createElement('h2');
    title.textContent = meta.name;
    const state = document.createElement('span');
    state.className = 'provider-state';
    if (status?.configured) {
      state.textContent = status.validated
          ? `Connected · ····${status.hint}`
          : `Saved · ····${status.hint} · not verified`;
      state.dataset['ok'] = status.validated ? 'yes' : 'unknown';
    } else {
      state.textContent = 'Not connected';
      state.dataset['ok'] = 'no';
    }
    head.append(title, state);

    const note = document.createElement('p');
    note.className = 'provider-note';
    note.textContent = meta.note;

    const link = document.createElement('a');
    link.href = meta.keysUrl;
    link.target = '_blank';
    link.rel = 'noreferrer';
    link.textContent = `Get a key from ${meta.vendor} →`;
    link.className = 'provider-link';

    const row = document.createElement('div');
    row.className = 'provider-row';

    const input = document.createElement('input');
    input.type = 'password';
    input.placeholder = status?.configured
        ? 'Enter a new key to replace the current one'
        : `${meta.prefix}…`;
    input.autocomplete = 'off';
    input.spellcheck = false;
    input.className = 'provider-input';

    const save = document.createElement('button');
    save.className = 'primary';
    save.textContent = 'Connect';

    row.append(input, save);

    const message = document.createElement('p');
    message.className = 'provider-message';

    save.addEventListener('click', async () => {
      const key = input.value.trim();
      if (!key) {
        return;
      }
      save.disabled = true;
      // The check is a real request to the provider, so it takes a moment.
      // Saying so beats an unexplained pause.
      message.textContent = 'Checking the key with ' + meta.vendor + '…';
      message.dataset['tone'] = 'neutral';

      const {stored, error} = await this.handler.setProviderKey(meta.id, key);
      save.disabled = false;

      if (stored) {
        input.value = '';
        message.textContent = 'Connected.';
        message.dataset['tone'] = 'ok';
        state.textContent = `Connected · ····${key.slice(-4)}`;
        state.dataset['ok'] = 'yes';
      } else {
        // Nothing was stored - the key is not saved in a broken state.
        message.textContent = error ?? 'That key could not be verified.';
        message.dataset['tone'] = 'bad';
      }
    });

    input.addEventListener('keydown', event => {
      if (event.key === 'Enter') {
        save.click();
      }
    });

    card.append(head, note, link, row, message);

    if (status?.configured) {
      const actions = document.createElement('div');
      actions.className = 'provider-actions';

      const recheck = document.createElement('button');
      recheck.className = 'ghost';
      recheck.textContent = 'Re-check';
      recheck.addEventListener('click', async () => {
        recheck.disabled = true;
        message.textContent = 'Checking…';
        message.dataset['tone'] = 'neutral';
        const {valid, error} = await this.handler.validateProviderKey(meta.id);
        recheck.disabled = false;
        message.textContent = valid ? 'Key still works.'
                                    : (error ?? 'Key no longer works.');
        message.dataset['tone'] = valid ? 'ok' : 'bad';
        state.dataset['ok'] = valid ? 'yes' : 'no';
      });

      const remove = document.createElement('button');
      remove.className = 'ghost danger';
      remove.textContent = 'Disconnect';
      remove.addEventListener('click', () => {
        this.handler.clearProviderKey(meta.id);
        state.textContent = 'Not connected';
        state.dataset['ok'] = 'no';
        message.textContent = 'Key removed.';
        message.dataset['tone'] = 'neutral';
        actions.remove();
      });

      actions.append(recheck, remove);
      card.append(actions);
    }

    return card;
  }
}
