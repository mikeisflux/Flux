// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

import {connectorMark, pathIcon, searchIcon} from './icons.js';

import {loadConnectorActions} from './catalog.js';
import type {ConnectorAction} from './catalog.js';

import {loadPackedJson, once} from './resource.js';

import {ConnectorAuth} from './flux.mojom-webui.js';
import type {ConnectorStatus, FluxPageHandlerRemote} from './flux.mojom-webui.js';

export interface Connector {
  id: string;
  name: string;
  description: string;
  transport: 'api'|'browser'|'mcp';
  badge: string|null;
  definition: 'authored'|'pending';
}

// The plug on each action chip - the reference marks every action with one,
// and it is what makes a dense two-column list read as a list of capabilities
// rather than a wall of sentences.
const kPlugPath = 'M6 2v5m8-5v5M4 7h12v3a6 6 0 0 1-12 0V7zm6 9v3';

export const loadConnectors = once(
    () => loadPackedJson<{connectors: Connector[]}>('connectors.json')
              .then(d => d.connectors));

/**
 * Connectors.
 *
 * The subtitle is the product's own thesis and is quoted almost verbatim: the
 * browser is the universal fallback, and a connector is an optimisation over
 * it rather than a capability unlock. Every screen in the console is built on
 * that assumption, so it is worth stating on the one screen that sells them.
 */
export class ConnectorsView {
  private all: Connector[] = [];
  private status = new Map<string, ConnectorStatus>();
  private query = '';
  private grid!: HTMLElement;
  private count!: HTMLElement;

  constructor(private handler: FluxPageHandlerRemote) {}

  /**
   * Redraws one card after the browser process reports a change. Connecting
   * is not a request-response - the user has to go and approve it in another
   * tab - so the result arrives on the observer, out of band.
   */
  onConnectorChanged(status: ConnectorStatus, error: string|null) {
    this.status.set(status.id, status);
    if (this.grid) {
      this.paint();
    }
    if (error) {
      this.notice(error);
    }
  }

  private notice(text: string) {
    const bar = document.getElementById('connector-notice');
    if (bar) {
      bar.textContent = text;
      bar.hidden = false;
    }
  }

  async render(root: HTMLElement) {
    this.all = await loadConnectors();
    await this.refreshStatus();

    root.replaceChildren();
    root.classList.remove('two-column');

    const screen = document.createElement('div');
    screen.className = 'screen';

    const h1 = document.createElement('h1');
    h1.textContent = 'Connectors';

    const subtitle = document.createElement('p');
    subtitle.className = 'subtitle';
    subtitle.textContent =
        'Flux can already use any website. A connector gives it a direct ' +
        'line into an app like Gmail or Slack, which for some actions is ' +
        'faster and more reliable than clicking through the site.';

    const label = document.createElement('div');
    label.className = 'section-label';
    const labelText = document.createElement('span');
    labelText.textContent = 'AVAILABLE';
    this.count = document.createElement('span');
    this.count.className = 'muted';
    const custom = document.createElement('button');
    custom.className = 'outlined';
    custom.textContent = '+ Custom';
    custom.title =
        'A custom connector is an MCP server or an OpenAPI spec. Not built yet.';
    custom.disabled = true;
    label.append(labelText, this.count, custom);

    const search = document.createElement('div');
    search.className = 'search';
    search.append(searchIcon());
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Search connectors...';
    input.addEventListener('input', () => {
      this.query = input.value.toLowerCase();
      this.paint();
    });
    search.append(input);

    // Out-of-band failures land here: an authorization the user abandoned, a
    // token endpoint that refused. A card cannot say it, because by then the
    // card is just "not connected" again.
    const notice = document.createElement('p');
    notice.id = 'connector-notice';
    notice.className = 'connector-notice';
    notice.hidden = true;

    this.grid = document.createElement('div');
    this.grid.className = 'connector-grid';

    screen.append(h1, subtitle, label, search, notice, this.grid);
    root.append(screen);
    this.paint();
  }

  private paint() {
    const rows = this.all.filter(
        c => !this.query || c.name.toLowerCase().includes(this.query) ||
            c.description.toLowerCase().includes(this.query));

    this.count.textContent = `${rows.length}`;
    this.grid.replaceChildren();

    if (rows.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'subtitle';
      empty.textContent =
          'No connector for that. Flux can still drive the site in a browser ' +
          '- start a task and point it at the page.';
      this.grid.append(empty);
      return;
    }

    for (const c of rows) {
      this.grid.append(this.card(c));
    }
  }

  private async refreshStatus() {
    const {statuses} = await this.handler.listConnectors();
    this.status = new Map(statuses.map(s => [s.id, s]));
  }

  private card(c: Connector): HTMLElement {

    const card = document.createElement('article');
    card.className = 'connector-card';
    // The dialog needs to find this card again if it hands off to the OAuth
    // registration form, which still renders inline underneath it.
    card.dataset['connector'] = c.id;

    const mark = connectorMark(c.id, 'connector-avatar', c.name);

    const body = document.createElement('div');
    const head = document.createElement('div');
    head.className = 'connector-head';
    const name = document.createElement('h3');
    name.textContent = c.name;
    head.append(name);

    // Eligibility, stated before the user tries and fails rather than after.
    if (c.badge) {
      const badge = document.createElement('span');
      badge.className = 'caveat';
      badge.textContent = c.badge;
      head.append(badge);
    }

    const desc = document.createElement('p');
    desc.textContent = c.description;
    body.append(head, desc);

    const state = this.status.get(c.id);

    const add = document.createElement('button');
    add.className = 'ghost-icon add';

    // Four states, and each one is a different thing for the user to do, so
    // each gets its own affordance rather than one button that fails
    // differently.
    if (c.definition === 'pending') {
      // Listed but never written against the real API.
      add.disabled = true;
      add.setAttribute('aria-label', `${c.name} is not wired up`);
      add.replaceChildren(plusIcon());
      add.title =
          `${c.name} is listed but not wired up: its auth and operations have ` +
          'not been written against the real API yet. Flux can still drive ' +
          'the site in a browser in the meantime.';
      card.dataset['pending'] = '';
    } else if (state && !state.connectable) {
      // Nothing to connect - a local reader, or an MCP server configured
      // elsewhere. The reason is the definition's, not invented here.
      add.disabled = true;
      add.setAttribute('aria-label', `${c.name} needs no connection`);
      add.replaceChildren(plusIcon());
      add.title = state.detail ?? `${c.name} needs no connection.`;
      card.dataset['pending'] = '';
    } else if (state?.connected) {
      add.classList.add('connected');
      add.setAttribute('aria-label', `Disconnect ${c.name}`);
      add.replaceChildren(tickIcon());
      add.title = state.expired ?
          `${c.name} is connected but its access has expired. Flux will renew ` +
          'it on the next task, or click to disconnect.' :
          `${c.name} is connected. Click to disconnect.`;
      if (state.expired) {
        card.dataset['expired'] = '';
      }
      add.addEventListener('click', () => {
        this.handler.disconnect(c.id);
      });
    } else {
      add.setAttribute('aria-label', `Connect ${c.name}`);
      add.replaceChildren(plusIcon());
      // Flux ships no client secrets, so an OAuth connector needs the user's
      // own app registration before there is anything to connect with.
      add.title = state?.hasClient === false && state?.detail ?
          state.detail :
          `Connect ${c.name}`;
      // One dialog for every connector, whatever its auth. The old behaviour
      // branched here into three different inline forms, so what "+" did
      // depended on a distinction - OAuth app versus pasted token versus
      // already-registered - that means nothing to the person clicking it.
      add.addEventListener('click', () => void this.openDialog(c, state));
    }

    card.append(mark, body, add);
    return card;
  }

  /**
   * Everything about one connector, in a dialog, with a single button.
   *
   * The actions list is the point of it. "Connect Google" on its own asks a
   * person to grant access to their mail on trust; the same button under
   * twelve named actions - List Gmail messages, Create a Gmail draft, Read a
   * Sheets range - tells them exactly what they are agreeing to. It is also
   * the honest answer to "what does a connector even do", which the old card
   * never gave.
   */
  private async openDialog(c: Connector, state: ConnectorStatus|undefined) {
    const dialog = document.createElement('dialog');
    dialog.className = 'flux-dialog connector-dialog';

    const head = document.createElement('div');
    head.className = 'connector-dialog-head';
    const title = document.createElement('h2');
    title.textContent = c.name;
    const blurb = document.createElement('p');
    blurb.className = 'subtitle';
    blurb.textContent = c.description;
    const text = document.createElement('div');
    text.append(title, blurb);
    head.append(connectorMark(c.id, 'connector-mark'), text);

    const body = document.createElement('div');
    body.className = 'connector-dialog-body';

    // Loaded rather than passed in: the actions live in the packed
    // definitions, and a dialog that opens instantly with an empty list and
    // fills in is better than a card that stalls on click.
    let actions: ConnectorAction[] = [];
    try {
      actions = (await loadConnectorActions()).get(c.id) ?? [];
    } catch (error) {
      console.error('Could not read the connector actions.', error);
    }

    if (actions.length > 0) {
      const label = document.createElement('h3');
      label.className = 'section-title';
      label.textContent = `AVAILABLE ACTIONS (${actions.length})`;
      const note = document.createElement('p');
      note.className = 'subtitle';
      note.textContent =
          'These are the actions Flux can use after you connect this account.';
      const grid = document.createElement('div');
      grid.className = 'action-grid';
      for (const action of actions) {
        const chip = document.createElement('div');
        chip.className = 'action-chip';
        chip.dataset['scope'] = action.writeScope;
        chip.append(pathIcon(kPlugPath));
        chip.append(action.label);
        grid.append(chip);
      }
      body.append(label, note, grid);
    }

    // An API key connector has nothing to authorize against - it wants a
    // token pasted - so the field belongs in the same dialog rather than
    // behind a different button.
    let tokenInput: HTMLInputElement|null = null;
    if (state?.auth === ConnectorAuth.kApiKey) {
      const wrap = document.createElement('label');
      wrap.className = 'dialog-field';
      const caption = document.createElement('span');
      caption.textContent = state.detail || 'API key';
      tokenInput = document.createElement('input');
      tokenInput.type = 'password';
      tokenInput.autocomplete = 'off';
      wrap.append(caption, tokenInput);
      body.append(wrap);
    }

    const error = document.createElement('p');
    error.className = 'dialog-error';
    error.hidden = true;
    body.append(error);

    const foot = document.createElement('div');
    foot.className = 'dialog-actions';
    const close = document.createElement('button');
    close.className = 'ghost';
    close.textContent = 'Close';
    close.addEventListener('click', () => dialog.close());
    const connect = document.createElement('button');
    connect.className = 'primary';
    connect.textContent =
        state?.connected ? `Disconnect ${c.name}` : `Connect ${c.name}`;
    foot.append(close, connect);

    connect.addEventListener('click', () => {
      void this.resolveDialog(c, state, tokenInput, connect, error, dialog);
    });

    dialog.append(head, body, foot);
    dialog.addEventListener('close', () => dialog.remove());
    document.body.append(dialog);
    dialog.showModal();
  }

  /**
   * What the one button does, decided here rather than by which button the
   * user found.
   */
  private async resolveDialog(
      c: Connector, state: ConnectorStatus|undefined,
      tokenInput: HTMLInputElement|null, button: HTMLButtonElement,
      error: HTMLElement, dialog: HTMLDialogElement) {
    const fail = (message: string) => {
      error.textContent = message;
      error.hidden = false;
      button.disabled = false;
    };
    button.disabled = true;
    error.hidden = true;

    if (state?.connected) {
      this.handler.disconnect(c.id);
      dialog.close();
      return;
    }

    if (tokenInput) {
      const token = tokenInput.value.trim();
      if (!token) {
        fail('Paste the key first.');
        return;
      }
      const {stored, error: why} =
          await this.handler.setPersonalToken(c.id, token);
      if (!stored) {
        fail(why || 'That key was not accepted.');
        return;
      }
      dialog.close();
      return;
    }

    // hasClient is true when the user registered an app OR when Flux ships one
    // for this provider. Only the connectors with neither still need the
    // registration form, and they say so instead of failing silently.
    if (state && !state.hasClient) {
      dialog.close();
      const card = this.grid.querySelector<HTMLElement>(
          `[data-connector="${CSS.escape(c.id)}"]`);
      if (card) {
        void this.showClientForm(card, c);
      }
      return;
    }

    const {started, error: why} = await this.handler.beginConnect(c.id);
    if (!started) {
      fail(why || 'Could not start the connection.');
      return;
    }
    dialog.close();
  }


  /**
   * The OAuth app registration form, inline under the card.
   *
   * Inline rather than a dialog because it is a step in connecting, not a
   * separate task, and because the redirect URI has to be copied out of here
   * and pasted into the provider's own form - which is easier next to the
   * card it belongs to than in a modal over it.
   */
  private async showClientForm(card: HTMLElement, c: Connector) {
    const existing = card.parentElement?.querySelector('.connector-form');
    if (existing) {
      existing.remove();
    }

    const {clientId, redirectUri, hasSecret} =
        await this.handler.getConnectorClient(c.id);

    const form = document.createElement('form');
    form.className = 'connector-form';

    const intro = document.createElement('p');
    intro.textContent =
        `Register an OAuth app with ${c.name}, then paste its details here. ` +
        'Flux ships no client secrets of its own - one inside a binary anyone ' +
        'can download is not a secret - so the app is yours, not Flux\'s.';

    const idInput = field(form, 'Client ID', clientId, 'text');
    // A stored secret is never sent back to the console, so the field starts
    // empty with a placeholder saying one is already held. Leaving it empty
    // keeps it.
    const secretInput = field(
        form, 'Client secret', '', 'password',
        hasSecret ? 'Stored - leave blank to keep it' : '');
    const redirectInput = field(
        form, 'Redirect URI', redirectUri || 'http://127.0.0.1/flux/oauth',
        'text');
    redirectInput.readOnly = false;

    const hint = document.createElement('p');
    hint.className = 'connector-form-hint';
    hint.textContent =
        'The redirect URI must match what you registered exactly. Flux ' +
        'catches the redirect in the tab, so the address does not have to ' +
        'resolve to anything.';

    const save = document.createElement('button');
    save.type = 'submit';
    save.textContent = 'Save and connect';

    const cancel = document.createElement('button');
    cancel.type = 'button';
    cancel.className = 'outlined';
    cancel.textContent = 'Cancel';
    cancel.addEventListener('click', () => form.remove());

    const row = document.createElement('div');
    row.className = 'connector-form-actions';
    row.append(save, cancel);

    form.prepend(intro);
    form.append(hint, row);

    form.addEventListener('submit', async event => {
      event.preventDefault();
      const {stored, error} = await this.handler.setConnectorClient(
          c.id, idInput.value.trim(), secretInput.value,
          redirectInput.value.trim());
      if (!stored) {
        this.notice(error ?? 'The registration could not be saved.');
        return;
      }
      form.remove();
      await this.refreshStatus();
      this.paint();
      const result = await this.handler.beginConnect(c.id);
      if (!result.started && result.error) {
        this.notice(result.error);
      }
    });

    card.after(form);
    idInput.focus();
  }
}

function field(form: HTMLFormElement, label: string, value: string,
               type: string, placeholder = ''): HTMLInputElement {
  const wrap = document.createElement('label');
  wrap.className = 'connector-field';
  const text = document.createElement('span');
  text.textContent = label;
  const input = document.createElement('input');
  input.type = type;
  input.value = value;
  input.placeholder = placeholder;
  if (type !== 'password') {
    input.required = true;
  }
  wrap.append(text, input);
  form.append(wrap);
  return input;
}

function plusIcon(): SVGSVGElement {
  return pathIcon('M10 4v12M4 10h12');
}

function tickIcon(): SVGSVGElement {
  return pathIcon('M4 10.5l4 4 8-9');
}
