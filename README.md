# Flux

An agentic AI browser. A fork of Chromium with a Claude/OpenAI agent layer
built into the browser process.

---

## Status

**The fork infrastructure is complete. Nothing has been compiled yet.** See
[Building](#building) for why, and what machine you need.

| Component | State |
|---|---|
| Reference teardown (8 docs, 250 tasks, 117 skills, 37 connectors) | ✅ complete |
| Build system — fetch / sync / build, GN args, patch series | ✅ written, **unrun** |
| Mojo interface (`src/browser/mojom/flux.mojom`) | ✅ complete |
| Agent layer C++ — headers and architecture | ✅ headers; `.cc` implementations pending |
| WebUI console (`chrome://flux`) | ⬜ not started |
| First successful build | ⬜ **blocked on hardware** |

---

## Why a fork rather than Electron

Electron is Chromium, and it would have run today. It was rejected for three
reasons that only matter for *this* product:

1. **Page understanding.** The agent reads pages through Chromium's
   accessibility tree — the semantic model the renderer already computes for
   screen readers. From inside the browser process that is a direct read. From
   outside it is a CDP round-trip per snapshot, on every step, of every run.
2. **Extensions.** Electron is not a full extension host — no Web Store install
   flow, no `chrome.tabs` parity, no MV3 service-worker fidelity. The reference
   product supports extensions and imports them from other browsers.
3. **Profiles as isolation.** Running ten agents concurrently without their
   cookies colliding wants real Chromium profiles, not a bolted-on session
   abstraction.

The cost is real and worth stating: a fork means a **permanent rebase burden**
against a codebase that ships every four weeks, multi-hour builds, and a
~200GB working set per developer.

---

## Building

### Hardware

**A laptop can do this.** Disk and RAM decide whether it works; cores only
decide how long you wait. Full guide: **[`docs/10-building.md`](docs/10-building.md)**.

| Resource | Hard floor | Comfortable |
|---|---|---|
| Free disk | 150 GB SSD | 250 GB |
| RAM | 16 GB | 32 GB |
| Cores | 4 | 8+ |

First `dev` build: 8–14 h on a 4-core laptop, 3–5 h on an 8-core/32 GB one,
~2 h on a 16-core desktop. **Incremental rebuilds of the agent layer are
1–3 minutes** — the first build is the only painful one. Run it overnight.

Linking needs ~1.5 GB per parallel job; `build/build.sh` detects low RAM and
caps `-j` automatically rather than letting the link OOM.

### Steps

```bash
build/fetch.sh     # depot_tools + Chromium source. ~100GB, 1-3 hours. Once.
build/sync.sh      # apply Flux patches, symlink our modules. Fast, idempotent.
build/build.sh dev # gn gen + autoninja.
```

Then `../chromium/src/out/Dev/chrome`.

### Configs

| Config | Use | Relative build time |
|---|---|---|
| `dev` *(default)* | Day-to-day agent work. No LTO/PGO, component build. | 1× |
| `debug` | Stepping through C++ in a debugger. | ~1.5× |
| `release` | Shipping. Official build, ThinLTO + PGO. | **4–6×** |

Use `dev` unless you are cutting a binary for a user.

**On Windows:** WSL2 is far easier and gives you a Linux binary — fine for
developing the agent layer. A native `Flux.exe` needs Visual Studio 2022 and
several non-obvious setup steps. Both routes are in
[`docs/10-building.md`](docs/10-building.md).

### Two caveats before the first build

- **The patch series has never been applied.** The three patches in `patches/`
  are written against M152 file layouts but have not been verified against a
  real tree. Expect the first `build/sync.sh` to need a rebase — that is normal
  fork work, and `sync.sh` fails loudly with the offending patch rather than
  half-applying.
- **API keys are empty.** `build/args/common.gni` ships blank Google API keys.
  Without them sync, Safe Browsing, geolocation and translate are inert. Get
  your own at <https://www.chromium.org/developers/how-tos/api-keys/>.

---

## Repository layout

Chromium's source is **not** checked in here — it is fetched and patched. This
is how Brave and Vivaldi are structured, and it is what keeps the repo
reviewable.

```
chromium.version          Pinned upstream release (M152.0.7977.60)
build/                    fetch / sync / build scripts, GN configs
patches/                  Patch series applied to the Chromium tree
  series                    Ordered list; every line is rebase burden
src/browser/              The Flux agent layer -> //chrome/browser/flux
  mojom/flux.mojom          WebUI <-> browser contract
  agent/                    Run loop, page context, tools
  providers/                Claude + OpenAI behind one interface
  skills/  scheduler/
src/resources/            chrome://flux WebUI -> //chrome/browser/resources/flux
data/                     250 task templates, 117 skills
docs/                     Reference teardown (see below)
```

`build/sync.sh` **symlinks** `src/` into the Chromium tree, so editing agent
code is picked up by ninja without re-syncing.

---

## Design decisions

These come out of the reference teardown in `docs/`, where each is traced to
the specific gap that motivated it.

**Write scope is structural, not prose.** The reference encodes safety in copy
— *"left unsent"*, *"nothing sends without you"*. Flux makes it a typed enum
(`kReadOnly` / `kDraft` / `kSend` / `kPurchase`) that the runner enforces: any
tool call exceeding the declared scope blocks on human approval. See
`RequiresApproval()` in `agent/agent_runner.h`.

**API and browser transports are distinguished.** 51 of the 73 services in the
reference catalog have no connector and run purely on browser automation —
including LinkedIn, its single most-used service at 45 templates. An API call
and a bot clicking a hostile site have nothing in common in latency, failure
mode or risk, and the user is told which one a task depends on.

**Cost is visible before the run, not after.** The reference meters in credits
but surfaces cost nowhere in the task UI. Flux charges against a budget before
dispatch and fails closed.

**Runs compile to replays.** `CompileReplay()` turns a successful action trace
into a deterministic script that re-runs without model inference — the main
lever against per-run cost for scheduled work.

**Model routing is a runtime decision.** The reference exposes no model picker
anywhere, implying one fixed model. Cheap models handle mechanical extraction;
frontier models handle judgment; failover is automatic.

---

## Reference teardown

Eight documents reverse-engineering the reference product from screenshots.
`docs/09-synthesis.md` is the entry point.

| Doc | Contents |
|---|---|
| `01-ui-teardown` | Window shell, sidebar, screens, design tokens |
| `02-template-catalog` | All 250 task templates |
| `03-skills-catalog` | All 117 skills |
| `04-workflows-and-connectors` | Workflows tab, service inventory |
| `05-connectors-tab` | 37 connectors, cross-referenced against the catalog |
| `06-customize-tab` | Instructions memory, skill schema, 3 verbatim skill bodies |
| `07-account-menu` | Browser surface, profiles, credits billing |
| `08-settings` | Local RAM-bound parallelism, `Ctrl+K` palette, dark mode |

---

## License

Chromium is BSD-3-Clause, Copyright The Chromium Authors. Flux's own code
under `src/` is likewise BSD-3-Clause. Note that enabling `proprietary_codecs`
(H.264/AAC) carries patent-licensing obligations, and Widevine DRM requires a
separate agreement with Google — see `build/args/common.gni`.
