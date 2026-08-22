// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

export interface Connector {
  id: string;
  name: string;
  description: string;
  transport: 'api'|'browser';
  badge: string|null;
  definition: 'authored'|'pending';
}

let cached: Promise<Connector[]>|null = null;

export function loadConnectors(): Promise<Connector[]> {
  if (!cached) {
    cached = fetch('connectors.json')
                 .then(r => r.json())
                 .then((d: {connectors: Connector[]}) => d.connectors);
  }
  return cached;
}

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
  private query = '';
  private grid!: HTMLElement;
  private count!: HTMLElement;

  async render(root: HTMLElement) {
    this.all = await loadConnectors();

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
    search.innerHTML =
        '<svg viewBox="0 0 20 20" aria-hidden="true">' +
        '<circle cx="9" cy="9" r="5.5"/><path d="M13 13l4 4"/></svg>';
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = 'Search connectors...';
    input.addEventListener('input', () => {
      this.query = input.value.toLowerCase();
      this.paint();
    });
    search.append(input);

    this.grid = document.createElement('div');
    this.grid.className = 'connector-grid';

    screen.append(h1, subtitle, label, search, this.grid);
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

  private card(c: Connector): HTMLElement {
    const card = document.createElement('article');
    card.className = 'connector-card';

    const mark = document.createElement('span');
    mark.className = 'connector-avatar';
    mark.textContent = c.name.slice(0, 1);

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

    const add = document.createElement('button');
    add.className = 'ghost-icon add';
    add.setAttribute('aria-label', `Connect ${c.name}`);
    add.innerHTML =
        '<svg viewBox="0 0 20 20" aria-hidden="true">' +
        '<path d="M10 4v12M4 10h12"/></svg>';

    // Every connector renders, because the list is what the product promises.
    // Only the ones whose auth endpoints and operation map have actually been
    // written against the vendor's API can connect, and the button says which
    // is which rather than failing at the OAuth redirect.
    if (c.definition === 'pending') {
      add.disabled = true;
      add.title =
          `${c.name} is listed but not wired up: its auth and operations have ` +
          'not been written against the real API yet. Flux can still drive ' +
          'the site in a browser in the meantime.';
      card.dataset['pending'] = '';
    } else {
      add.title = `Connect ${c.name}`;
    }

    card.append(mark, body, add);
    return card;
  }
}
