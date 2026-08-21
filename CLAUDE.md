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

## Layout

- `src/browser/` - agent layer, junctioned into `//chrome/browser/flux`
- `src/resources/` - the chrome://flux console, junctioned into
  `//chrome/browser/resources/flux`
- `patches/` - the Chromium patch series, applied in `patches/series` order
- `build/` - fetch/sync/build, in both bash and PowerShell
- `go.ps1` - the single entrypoint: pull, sync, build
- `data/` - templates, skills, connector definitions
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

## Product constraints

- The Facebook skill posts to the user's **personal profile**, not a Page.
  That decision is final; do not re-litigate it.
- Three posts a day, 90 minutes apart, one per run.
