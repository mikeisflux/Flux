# Design canvas

`*.dc.html` and `canvas.json` are the authoring source. `flux-product-design.html`
is the published canvas, seeded from them and gitignored - it is ~2MB of editor
payload and is regenerated, never edited.

Five artboards, all drawn from what ships rather than from imagination: the
tokens are lifted verbatim out of `src/resources/app.css`, and the first-run
copy out of `src/resources/welcome_view.ts`. If the product changes, these are
wrong until someone re-derives them.

- `Main.dc.html`, `FirstRunSkills.dc.html`, `FirstRunDone.dc.html` - first run
- `Connectors.dc.html` - the connectors screen, six real catalogue rows chosen
  to show all four card states: connected, connectable, needs an OAuth app,
  and nothing to connect
- `Mark.dc.html` - the mark at 16-64px, the three accent tokens, and the one
  rule the brand turns on: lime is a fill, never text

Two tweaks on every artboard: `theme` (dark/light, because the browser ships
both) and `accent`. Everything else is literal, so it paints while streaming
and so the properties panel can edit it directly.
