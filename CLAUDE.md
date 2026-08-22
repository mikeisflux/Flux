# Flux

A Chromium fork (pinned at M152.0.7977.60) with a built-in AI agent. The user
builds on Windows; this container cannot build Chromium, so **every command
handed over is run by a human on their machine and every failure costs them a
round trip.**

## Verify commands before handing them over

This is the rule. Broken commands have burned more of the user's time on this
project than every real bug combined.

**Never hand over a command you have not checked.** Specifically:

- **Prefer one self-contained command.** No `cd` followed by `;`. Absolute
  paths, or a script that resolves its own location from `$PSScriptRoot`. A
  chained command that depends on the shell's current directory is broken by
  anything that reorders the paste - and pastes do get reordered.
- **Never send a multi-line block to paste.** PowerShell in particular will
  eat it, reorder it, or drop into a `>>` continuation prompt. One line.
- **Windows PowerShell 5.1** is the target. No `&&`, no `||`, no bash-isms.
  `Start-Process -Wait`, not `--wait`.
- **Check every flag against the actual tool**, not memory. `vswhere` without
  `-products *` cannot see Build Tools SKUs. That single missing flag sent a
  false "ATL is not installed" and cost an install cycle.
- **Read the real source before predicting behavior.** Chromium moves. When
  something fails, fetch the actual file at the pinned tag from
  `raw.githubusercontent.com/chromium/chromium/<version>/<path>` and read it.
  Every guess made about M152's APIs from memory has been wrong; every answer
  read out of the real file has been right.
- **Fix the script, not the instructions.** If a command needs a caveat to
  work, the script is wrong. `sync.ps1` stranding the caller in the Chromium
  tree was a script bug, not something to warn about.

When something fails, the first question is "what did I get wrong", not "what
did they do wrong". So far the answer has been the former every single time.

### PowerShell is parse-checked, not eyeballed

`tools/check-powershell.sh` parses every `.ps1` with the real PowerShell parser
(downloading pwsh on first run and caching it). A PostToolUse hook runs it
after every edit, so a syntax error cannot reach the user's machine.

It exists because these errors are invisible to review. `"$runner: gn clean"`
reads correctly and parses as a drive-qualified variable reference, which is a
hard parse failure - the script dies at line 1, having done nothing. Reading
carefully does not catch this class of bug. A parser does.

It also compares parameter names against local assignments, because variable
names are case-insensitive: a local `$jobs` and a parameter `[int]$Jobs` are
one variable, and the parameter's type sticks. Assigning an array to the local
then fails at runtime with a conversion error naming neither variable. Parsing
cannot see that; comparing the two can.

If the download fails the check exits 0 and says so. That means the scripts are
**unverified** - say that rather than claiming they work.

### The WebUI console is linted the same way

`build_webui()` runs stylelint and eslint as *build steps*, so a formatting nit
in `app.css` is a hard build failure - and it surfaces about 44,000 targets in,
roughly two hours. `tools/check-webui.sh` runs the same checks in seconds:
stylelint against `tools/stylelint.config.mjs` (a mirror of Chromium's config,
keep it in sync on uprev), plus the mixed type/value import rule that
`@webui-eslint` enforces and npm has no copy of. Both are on the same hook.

`tools/check-webui.sh` also type-checks every `.ts` with the real `tsc` under
the same strict settings `build_webui` uses. The mojom bindings only exist
inside a Chromium build, so `tools/webui-typecheck/stubs/flux.mojom-webui.d.ts`
stands in for them; it is transcribed by hand and **must be updated whenever
`src/browser/mojom/flux.mojom` changes**. It errs toward being incomplete
rather than wrong: anything the console calls that the stub does not declare
fails the check, which is the signal to add it.

Node and npm are available in this container, so there is no excuse for
hand-formatting CSS to satisfy a linter, or for shipping TypeScript nobody
compiled - install them and run the real thing.

## Layout

- `src/browser/` - agent layer, junctioned into `//chrome/browser/flux`
- `src/resources/` - the chrome://flux console, junctioned into
  `//chrome/browser/resources/flux`
- `patches/` - the Chromium patch series, applied in `patches/series` order
- `build/` - fetch/sync/build, in both bash and PowerShell
- `go.ps1` - the single entrypoint: pull, sync, build
- `data/skills/*.md` - the skill library, in the frontmatter-plus-freeform-body
  format `docs/06` settles on. This is the authoring source;
  `tools/build-skills-json.py` packs it into `src/resources/skills.json`, and
  the check regenerates and diffs so the two cannot drift. `When to use` is
  required in every body - it is the retrieval trigger the whole feature rests
  on, and the packer refuses a file without one.
- `data/connectors/*.json` - per-connector auth endpoints and operation maps.
  Names, descriptions and badges live in `src/resources/connectors.json` so
  the console and the browser process cannot disagree about them.
- `src/resources/templates.json` - the 250-template catalog. It lives with the
  WebUI rather than in `data/` because it is packed into `flux_resources.pak`;
  `build_webui` cannot reach outside its own directory, and one copy read by
  both the console and the browser process beats two that drift.
- `branding/` - the Flux mark and the icon files generated from it by
  `tools/build-icons.py`. The output is committed, so a build machine needs
  nothing installed; `build/sync` copies it over `chrome/app/theme/chromium`
  after the patches apply, which keeps binaries out of the patch series.
- `docs/` - the Polar teardown these features are specified from

## Patches

Generate them mechanically with `diff -u` against the real file fetched from
the pinned tag. Never hand-write `@@` headers - the line counts will be wrong.
Write them LF-only; `.gitattributes` enforces it, because a CRLF patch fails
against Chromium's LF tree with a content error that says nothing about
encoding.

## Build system notes

Hard-won, each one from a failed build:

- `//` in GN is the Chromium root, not this repo. Args files must be
  self-contained - there is nothing here for them to import.
- Siso will not traverse a Windows junction; `use_siso = false` is required.
- `vs_toolchain.py` looks for VS 2022 under `%ProgramFiles%`, but Build Tools
  installs to `%ProgramFiles(x86)%`. Set `$env:vs2022_install`;
  `GYP_MSVS_OVERRIDE_PATH` does not work, because `GetToolchainDir` calls
  `GetVisualStudioVersion` separately.
- WebUIs register through `RegisterChromeWebUIConfigs` in
  `chrome_web_ui_configs.cc`. The old host-comparison chain in
  `chrome_web_ui_controller_factory.cc` handles DevTools only now.
- Native tools write progress to stderr, which `$ErrorActionPreference='Stop'`
  treats as fatal. Run them through `Invoke-Native`.

## Brand

Flux lime is `#b4f03c`, and it is a **fill** colour: near-black `#14200a` on
lime clears 13:1, lime on white fails at any size. `--accent-text` (`#4f7a08`)
is the darkened version for the rare case where the accent has to be text.

Spend it deliberately. The reference product has no accent at all, so that
connector icons are the only saturated pixels and the eye goes straight to
"what does this task touch". Flux keeps that discipline and buys exactly three
things with the accent: the composer's send button, keyboard focus, and the
first-run flow. Active pills and primary buttons stay black.

## Product constraints

- The Facebook skill posts to the user's **personal profile**, not a Page.
  That decision is final; do not re-litigate it.
- Three posts a day, 90 minutes apart, one per run.
