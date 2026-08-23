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

It also rejects an `@( )` array literal passed to `Invoke-Native`. Windows
PowerShell 5.1 flattens an array literal bound to a
`ValueFromRemainingArguments` parameter into ONE space-joined string, so
`Invoke-Native $python @($script, $dir)` hands python a single filename made
of two paths glued together. Splatting an array variable (`@vars`) and plain
positional arguments both work; only the literal collapses. It reads
correctly and parses correctly - that is the whole problem.

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

### The C++ is checked against the mistakes that have cost builds

`tools/check-cpp.sh` is not a compiler and cannot be one - Chromium does not
build in this container. It is a list of the specific Chromium-isms that have
each broken a real build once, as greps over `src/browser`:

- `JSONReader::Read` and friends lost their single-argument overloads; the
  `options` argument is required and the providers already pass
  `base::JSON_PARSE_RFC`.
- A reference-typed field is rejected outright by the `chromium-rawref`
  plugin. Use `raw_ref<T>`.
- A raw pointer field is rejected by the raw-ptr plugin. Use `raw_ptr<T>`.
- A KeyedService factory must be registered in
  `EnsureBrowserContextKeyedServiceFactoriesBuilt()` before any profile
  exists. A `NoDestructor` factory constructed lazily on first use is fatal:
  the console asking for `FluxAgentService` was the first use, and it happened
  long after profiles were built (`0016`).
- A pref registered `SYNCABLE_PREF` must also be in Chromium's central
  `SyncablePrefsDatabase`. It is a DCHECK, `dcheck_always_on` is on in
  `dev.gn`, and it is fatal at profile creation - the browser dies before
  drawing a window and exits **0**, with nothing on screen. Sync is inert in
  this fork anyway (no Google API keys), so register prefs non-syncable.
- Every `.cc`/`.h` under `src/browser` must be listed in `src/browser/BUILD.gn`
  - or added to a Chromium target by the patch series, as the views subclasses
  under `ui/` are. A file that is in neither compiles nowhere, and the error is
  an undefined symbol at LINK, at the very end of the build.

`tools/check-includes.sh` HEAD-requests every Chromium header `src/browser`
includes against the pinned tag, because `base/containers/contains.h` does not
exist in M152 and nothing here knew. The include reads correctly, no other
check looks at include paths, and it failed 334 targets into the user's build.
Headers this fork owns, grit output and mojom output are skipped; everything
else has to resolve. Answers are cached per header, so the first run is ~25s
and the rest are instant.

The lesson underneath it is the one already written above: `base::Contains`
was removed and I reached for it from memory. Chromium migrates its `base/`
container algorithms to `std::ranges::` over time, so a `base/` helper that
existed last year is not evidence it exists now - check the tag.

A guard can also be written against a failure mode it cannot observe. The
connector client checked `GURL::is_valid()` to catch an unsupplied path
placeholder, and its own comment said so - but `url/url_canon_path.cc` marks
`{` and `}` ESCAPE, not reject, so GURL percent-encodes them and reports the
URL valid. The check could never fire; a missing parameter would have sent
`/projects/%7Bproject_id%7D.json` to a live API and come back a 404 that reads
like the provider's fault. Before trusting a guard, confirm the thing it tests
actually changes when the bug is present.

Add a rule when something new costs a build, and **break it on purpose to
prove it fires** before trusting it - two checks in this repo have already
passed while the thing they were supposed to catch went through.

The deeper lesson each of these encodes: **match the surrounding code**. All
three were already done correctly elsewhere in `src/browser`, and grepping for
an existing use would have been faster than getting it wrong.

### Chromium's own name is renamed at sync time, not by a patch

Chromium hardcodes "Chromium" as a literal in `chrome/app/chromium_strings.grd`
and `settings_chromium_strings.grdp` - 870 of them - rather than filling
IDS_PRODUCT_NAME into a placeholder. Patch 0009 renames the two that define
IDS_PRODUCT_NAME, which covers the window title and the about page. The rest
are strings like "Continue where you left off: Chromium restores your tabs
every time you restart", and each one is the browser telling the user it is
Chromium.

`tools/rebrand-strings.py` does the other 868, run by `build/sync` right after
the icon copy. A script rather than a patch on purpose: 870 hunks against a
file Chromium edits constantly would be the most expensive thing in the series
to rebase, and this derives its answer from whatever the tree currently says,
so an uprev costs nothing.

Two things keep the name. "The Chromium Authors" is a copyright attribution,
not a product name - renaming it would be a false claim in a string the about
page shows. And anything inside a URL, because a renamed host is a dead
support link. `ChromiumOS` and `ChromiumUpdater.exe` also survive, because the
rename is word-boundary matched; neither is reachable on Windows.

`tools/check-rebrand.sh` runs it against the real files at the pinned tag and
asserts all of that. The URL guard is checked against a synthetic fixture,
because no URL in those files contains "Chromium" today - so that assertion
would otherwise pass with the guard deleted, which is how a check in this repo
has already rotted once.

### The patch series is applied before it is handed over

`tools/check-patches.sh` fetches only the files the series touches from the
pinned tag - eighteen of them, a few seconds, cached - and applies the whole
series in order. A rotted patch is otherwise found by the user, on their
machine, at the start of a build they were about to spend two hours on. It is
on the same hook as the other checks.

If the fetch fails the check exits 0 and says so, which means the series is
**unverified** - say that rather than claiming it applies.

### Reading the log: turn the noise off, do not filter it out

Two filtering mistakes, in order of how much time each has cost:

**Filtering *in* on keywords** (`ERROR|FATAL|CHECK|flux`) finds the crash and
hides everything around it. Warnings that name no keyword, and the whole
positive record of what *did* load, never appear.

**Filtering *out* on `:VERBOSE\d:` leaks.** A multi-line `VLOG` writes the
`[pid:tid:...:VERBOSE1:file.cc:NN]` prefix on the FIRST line only; the body
lines carry no prefix and survive the filter. That is where the orphaned
`extension id: / context_type: WEBUI` blocks come from - they are the tail of
`VLOG(1) << "Created context:\n" << GetDebugString()` in
`extensions/renderer/script_context.cc`, with their header stripped away.

So do not generate the noise in the first place. `--v=1` is what turns ~9,800
lines of field-trial and module-loader `VERBOSE1` on; drop it and the log is
short enough to read end to end:

    Get-Content 'C:\flux-build\flux-test\chrome_debug.log' | Select-Object -Last 200

Add `--v=1` back only when chasing something that needs it, and then filter:

    Get-Content 'C:\flux-build\flux-test\chrome_debug.log' | Where-Object { $_ -notmatch ':VERBOSE\d:' } | Select-Object -Last 200

Chromium noise that is NOT a Flux bug, confirmed against the M152 source and
not worth re-diagnosing:

- `ERROR:direct_composition_support.cc` `AMD VideoProcessorGetOutputExtension
  failed` - a driver capability probe, every run, on this GPU.
- `VERBOSE1 ... QUIC_DECRYPTION_FAILURE ... (missing key)` - packets that
  arrive before key negotiation finishes. Normal QUIC.
- `WARNING:runtime_features.cc` `SharedStorage / AttributionReporting cannot be
  enabled in this configuration` - those APIs need an explicit
  `--enable-features`, and no field-trial config ships in this fork.
- `INFO:paint_property_tree_printer.cc:231/236/240/244` - Blink dumping its
  transform/clip/effect/scroll trees on every layout. The whole file is
  `#if DCHECK_IS_ON()`, so this exists **only** because `dev.gn` sets
  `dcheck_always_on = true`. Noise, not a signal.

### Running it, when it will not run

A fork with `dcheck_always_on = true` fails in a way no build error prepares
you for: the browser process dies inside a DCHECK, the GPU process starts and
stops, no window ever appears, and the exit code is **0**. Double-clicking
looks like nothing happened at all.

Get the reason rather than guessing. One line, and the exit code matters as
much as the log:

    (Start-Process 'C:\flux-build\chromium\src\out\Dev\flux.exe' -ArgumentList '--user-data-dir=C:\flux-build\flux-test','--enable-logging' -PassThru -Wait).ExitCode

    Get-Content 'C:\flux-build\flux-test\chrome_debug.log' | Select-Object -Last 200

`--enable-logging` writes `chrome_debug.log` into the user-data dir. A separate
`--user-data-dir` keeps test tokens out of a real profile and makes a clean
first run a matter of deleting the folder.

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
- **A GN action's `inputs`/`outputs` are a promise to ninja, not a rename.**
  Renaming the executable in `chrome/BUILD.gn` left `reorder-imports.py` -
  which the action passes *directories* to, and which hardcodes `chrome.exe`
  on both sides - reading the PREVIOUS build's binary out of `initialexe/`,
  reprocessing it, and reporting success. The build was green, `flux.exe`
  never appeared in `out/`, and the thing in `out/` was stale. When a rename
  touches an action, read the script it runs.

## What is called "chrome" and stays that way

`chrome/browser/...` is Chromium's source tree, and the fork keeps it. Every
`#include "chrome/..."`, every GN label and every one of ~40,000 files would
have to change to rename it, and the reward would be a tree that conflicts
with upstream on every uprev. No fork does this - not Brave, Edge, Vivaldi or
Opera. Build output scrolling past as `obj/chrome/browser/...` is Chromium
compiling its own code, not a naming oversight. Flux's own objects appear
under `obj/chrome/browser/flux/flux/`.

What a user can actually see IS renamed, and each one took a patch:

- `flux.exe`, not chrome.exe (`0014`)
- `%LOCALAPPDATA%\Flux\User Data`, not Chromium's - which otherwise means
  two browsers sharing one profile directory (`0015`)
- The window title, task manager and about page (`0009`)
- The icon and taskbar identity (`0001`, `branding/`)

If something else user-visible still says Chromium, that is a bug. If it is a
path inside the source tree or the build directory, it is not.

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
