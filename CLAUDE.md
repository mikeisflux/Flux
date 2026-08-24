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

`tools/check-webui.sh` also reconciles `src/resources/BUILD.gn` with reality in
three directions. A file on disk that BUILD.gn does not list is not packed, and
a file BUILD.gn lists that is not on disk fails to resolve - both surface deep
inside the build. The third has no compiler on either side: a resource the
browser process looks up by **path string** rather than by a grit IDR symbol,
as `connector_registry.cc` does deliberately for `connector_defs.json`. A typo
there is not an error at all. It is an empty registry and one line in the log,
which reads as "no connectors are configured" rather than as a bug.

`tools/check-webui.sh` also type-checks every `.ts` with the real `tsc` under
the same strict settings `build_webui` uses. The mojom bindings only exist
inside a Chromium build, so `tools/webui-typecheck/stubs/flux.mojom-webui.d.ts`
stands in for them; it is transcribed by hand and **must be updated whenever
`src/browser/mojom/flux.mojom` changes**. It errs toward being incomplete
rather than wrong: anything the console calls that the stub does not declare
fails the check, which is the signal to add it.

The stub is the one input `tsc` cannot check, because it compiles the console
*against* the stub - a wrong stub is a self-consistent world where every line
that agrees with the fiction passes. `url.mojom.Url` is not `{url: string}`;
`url/mojom/BUILD.gn` declares `ts_typemaps` mapping it to a plain `string`, so
`action.pageUrl.url` type-checked here and died at target 114 of 1532 with
`Property 'url' does not exist on type 'string'`. A mojom struct can be
typemapped to any TS type and nothing in the `.mojom` says so - the declaration
lives in the BUILD.gn of whatever module owns it.

`tools/check-ts-typemaps.py` reads those declarations at the pinned tag, for
every mojom `flux.mojom` imports, and fails if the stub declares or uses a type
that Chromium maps away. It is the only check here that verifies the stub
against something outside itself. It exits 0 and says so if the fetch fails,
which means the stub is **unverified** - say that rather than claiming it
agrees.

`tools/check-mojom-stub-types.py` covers the other half: the stub against
`flux.mojom`'s own declarations. `array<uint8> bytes` was declared
`Uint8Array`, because that is what a bytes field obviously is. The generator
emits `number[]` - `_kind_to_ts_type` maps every integer width to `number`,
and an array of a non-nullable kind to `%s[]` - so `ask.ts` type-checked here
and died at target 116 of 1536. The mapping table is read out of
`mojom_ts_generator.py` at the pinned tag rather than from memory, which is
where the `Uint8Array` came from in the first place.

The two checks divide by where the answer lives. A type an imported module
typemaps away is invisible in the `.mojom` and only `check-ts-typemaps.py` can
find it; a type flux.mojom declares itself is fully determined and this one
compares it. Only fields present in BOTH are compared - an incomplete stub is
safe, because `tsc` fails on anything the console uses that the stub does not
declare. It is a **wrong** stub that is dangerous, and the danger is specific:
it is a self-consistent world where every line that agrees with the fiction
passes.

Watch `int64`/`uint64` in particular - those are `bigint`, not `number`, and
that is the same bug wearing different clothes.

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
- A **virtual method with a non-empty body declared inline in a header** is
  rejected by chromium-style's `find-bad-constructs` plugin, and `/WX` makes
  it an error. `virtual bool NeedsPage() const { return false; }` failed seven
  translation units at once, nine minutes in. It is ordinary C++ and reads
  like nothing at all. The same shape inside a `.cc` is accepted, which is why
  all seven `override { return true; }` compiled while the one declaration in
  the header did not - declare it in the header, define it in the `.cc`.
- `ui::AXTree::Unserialize` applies an **update**; it does not replace the
  tree. `RequestAXTreeSnapshot` returns a complete standalone tree each time,
  so feeding a second snapshot into the tree holding the first is read as an
  incremental change - and on a live page whose node ids have moved, that is
  an illegal reparent and a FATAL. The browser died the first time the agent
  read Gmail twice. Build a fresh `AXTree` per snapshot; it is held by
  `unique_ptr` because AXTree deletes copy and move assignment. The code that
  did this carried a comment asserting Unserialize "replaces the tree's
  contents in place" - **a comment is not a citation.** That claim was never
  read out of `ax_tree.h`, and writing it down made it look settled.
- `SimpleURLLoader::DownloadToString` DCHECKs `max_body_size <=
  kMaxBoundedStringDownloadSize` (5 MiB). It is a ceiling, not a clamp. Both
  providers passed 10 MiB, so the browser died on the first request it ever
  made - which was the API-key probe, so entering a key killed the browser
  from every entry point. Pass the constant, not a number.

That last one only works because the checker stopped throwing the line away.
A line beginning with a complete inline `/* ... */` - Chromium's
`/*max_body_size=*/N` argument style - was being treated as a comment and
skipped, so every annotated call site in the tree was invisible to every rule
here. The rule was added, ran green, and the 10 MiB call walked straight past
it. Breaking a new rule on purpose is what caught that, and it is the third
time a check in this repo has passed while the thing it was written for went
through.

### Async failure in the console is silent by default

An event handler cannot await, so the console is full of `void this.x()` -
two dozen of them - and each one discards a promise. A rejection there is not
an error the user sees; it is a button that shrugs. `settle()` covers a screen
render, and nothing covered the rest.

Both documents now install an `unhandledrejection` listener. The tab shows a
toast that stays until dismissed - a message that removes itself after four
seconds is one they will miss, which is how these failed in the first place -
and the sidebar logs, because it has nowhere to put a toast and its own
failure mode is a run list that is silently empty forever.

This is a net, not a substitute for a real error path. Where a screen can say
what went wrong, it should.

### What the agent can hand back

Four things, and the agent has to be told about all of them in the system
prompt or it uses none:

- `save_artifact` for output that lives somewhere - a Sheet, a Doc, a page.
- `write_file` for output that has nowhere to live: a CSV of scraped rows, a
  report, an export. It writes to Downloads on a worker thread, never
  overwrites (`GetUniquePath`), sanitises the model's filename down to a bare
  name, and records the result as an artifact so it appears with an Open
  button. Without it the agent's only option was to paste a thousand rows into
  the reply.
- `ask_user` for an unfilled `[placeholder]` or a choice only the user can
  make. Every template prompt in the catalogue is written with brackets, so
  this is the difference between the catalogue working and not. The run blocks
  in `kAwaitingInput` until answered - and note that without a blocking tool an
  agent that asks a question in prose ENDS the run, because `OnCompletion`
  reads "no tool calls" as "task finished".
- Markdown. `renderMarkdown` began as the slice the skill bodies used and was
  pointed at run output unchanged, so `[Prospects](https://...)` rendered as
  literal brackets and a table rendered as pipes. It now does links
  (scheme-checked - this is a privileged WebUI and a `javascript:` href from a
  model would run with chrome://flux's authority), tables, fenced code,
  blockquotes and rules.

### The build-breaking classes a grep list cannot see

Three checks stand in for the compiler this container does not have. Each
targets a failure that costs the user a whole build.

`tools/check-undefined-symbols.py` finds a method declared in a header and
defined nowhere. Every translation unit compiles and it dies at LINK, after
the whole two-hour build has already happened - the same ending as a file
missing from `BUILD.gn`, from the other cause.

`check-cpp.sh` gained two rules. A switch over an enum that misses a case and
has no `default` is a build failure, because Chromium builds `-Wswitch` as an
error; this happens every time an enum gains a value. And a method that
shadows a base virtual without saying `override` is one too
(`-Winconsistent-missing-override`), with a nastier variant behind it: if the
signature has drifted from the base, without `override` it becomes a brand new
method nothing calls, the base version runs instead, and the behaviour
disappears with no diagnostic at all.

All three were wrong before they were right, and the pattern is the same every
time: free functions invisible because the scan only matched `Class::name(`,
inline accessors reported as undefined because a greedy `(.*)` let
`scheduler() { return scheduler_.get(); }` end in a semicolon, macros parsed as
methods, and every correctly-marked `override` in the tree reported as missing
one because the parameter group swallowed the keyword. Sixteen false findings,
then twelve, then ten, then zero. A check is not finished when it prints
nothing - it is finished when it prints nothing AND fires on the bug removed
on purpose.

### The stall detector counted three websites as one repeated action

`read_page` is the only tool in the set whose `InputSchema()` is
`ObjectSchema({}, {})` - no arguments at all. `DigestOf` hashed name plus
input, so every `read_page` in a run produced the SAME hash for the life of
that run. With `kRepeatWindow = 6` and `kMaxRepeatsInWindow = 3`, reading
Slack, then Gmail, then Calendar was "the same call three times" and the run
was killed with "the task stopped making progress" - at 0/5 plan steps, having
done exactly what it was asked. Visiting several sites and reading each one is
the core loop of this product.

Two things were wrong. The digest ignored **which page the call was made
against**, and the window counted **observation** as an action that can fail to
make progress. A read always returns the current state; repeating one is at
worst wasteful, and waste is already bounded by the credit budget and by
`kMaxActions`. The signature this guard exists for - and its own comment says
so - is an ACTION that leaves the world unchanged and is tried again forever.

The wider lesson: a hash of "the call" that leaves out the state the call acts
on will collide for any tool that takes no arguments. There is exactly one such
tool today, and it was enough.

### A run that ends on our own decision has to say so

Nothing in `chrome_debug.log` explained that stop, because `ShouldStop()`
returned a bool and logged nothing. The user saw one sentence on screen that
reads identically whether the cause was repetition, three consecutive
failures, or the 200-action ceiling - and the log, which is the one artifact
that crosses the gap to this container, had no trace of it at all. It is
`StopReason()` now: the rule that fired, in the run summary and at
`LOG(WARNING)`.

### A name can be unreachable from where it is used

`tools/check-cpp-visibility.py` covers three shapes that arrived in one build,
all of which read as ordinary C++:

- `AskSession::RunTool` was **defined out of line and declared in no class
  body** - a method renamed into existence during a debug pass, with the
  header never told. `check-undefined-symbols.py` looks for the opposite
  (declared, never defined) and cannot see this one.
- `ask()`, `PendingApprovals()` and `PendingQuestions()` were **private** and
  called from the page handler. An insertion landed below `private:`, which
  is invisible in a diff.
- `NormalizeCommand` was **defined 500 lines below its first caller** with no
  declaration above it. Legal inside a class body, never at namespace scope.

The header parser keeps a STACK of open classes. The first version kept one
name, and a nested `class Delegate {` destroyed it: when the nested body
closed the enclosing class was forgotten, every member below it went
unrecorded, and the check reported **103 findings of which every one was
wrong**. `agent_runner.h` opens `class Delegate` on line 39 and `private:` on
line 98, so AgentRunner's entire private section was invisible.

Then three more false positives from `const Workflow* Get(...) const;` - the
return type is two tokens and the leading alternation had no `const`, so the
declaration did not parse and its definition was reported as undeclared.

Rule 3 is **type-directed** for the reason `check-null-deref.py` had to be:
judging by method name alone called `observer_->OnLearnedFact()` a private
member of FluxPageHandler, when the receiver is a `mojo::Remote` whose
interface is generated and lives in no header here. It resolves the
receiver's declared type first and only judges a call whose receiver it can
name.

Validated against the real pre-fix tree rather than a fixture: all three rules
fire on the exact clang errors, same files, same lines.

### The titlebar band is not client area, and a view there gets no clicks

Patch 0020 exists because of this, and it names `flux_sidebar_` and
`flux_avatar_` one at a time. The Ask Flux pill went into the same band as a
third view with no entry there, so Windows answered WM_NCHITTEST with
HTCAPTION, began a window drag, and never delivered a click. It painted
correctly, it was visible, and it did nothing - and no amount of reading its
click handler would have shown why.

It is in the toolbar row now, immediately left of the three-dot menu, which is
both where the reference product puts it and client area. The reasoning that
put it in the band - "the band is already laid out by code this fork owns, the
toolbar would mean patching toolbar_view.cc" - weighed the cost of a patch
against a few pixels of position and never counted the hit-testing, which is
the part that decides whether the control works at all.

**Anything new in that band needs a line in 0020.** Better: put it somewhere
that is client area to begin with.

### A grep list cannot see an incomplete type

`view.h` line 122 is `class ViewAccessibility;` and nothing more, so
`GetViewAccessibility().SetName(...)` needs
`ui/views/accessibility/view_accessibility.h`. FluxAskButton had the exact
line FluxAvatarButton has, minus the include the avatar carries for it, and
the error would have been "member access into incomplete type" nowhere near
the include block. `check-cpp.sh` now carries a table of symbol -> required
header; a table rather than a rule, because which types a common header
forward-declares is a fact about Chromium at this tag, not something derivable
from our source.

### Two structs named ProposedLayout, sharing nothing

`BrowserViewLayoutImpl::ProposedLayout` is **not** `views::ProposedLayout`.
The browser one is hierarchical - `bounds`, `visibility`, and a
`std::map<raw_ptr<View, CtnExperimental>, ProposedLayout> children` - with
`AddChild()` and a `GetLayoutFor()` that is **const-only**, returning a
`const ProposedLayout*`. The views one is flat, with `child_layouts` and a
non-const `GetLayoutFor()` returning a `ChildLayout*`.

The Ask panel's layout was written against the wrong one: it took a
`views::ChildLayout*` from a call that returns a const pointer to a different
type, and then mutated through it. Three errors in four lines, in a file that
compiles late because it belongs to a Chromium target.

Reaching into `children` directly is the fix, and it must go **through a plain
`views::View*`**. The map is keyed by `raw_ptr<View, CtnExperimental>` and
`contents_container` is a `raw_ptr<View>` with default traits; raw_ptr's
cross-kind constructor is `explicit` and `static_assert`s that the only
difference may be `kMayDangle`. Its own comment says the assert exists so the
compiler catches other conversions rather than letting the implicit
`raw_ptr<T> -> T* -> raw_ptr<>` route be taken - so the direct call is a hard
error, and the two-step conversion is the supported one.

### There is no compiler here, so arity is checked instead

`tools/check-arity.py` catches a call to one of this project's own methods
with the wrong number of arguments. Nothing else here can: `check-cpp.sh` is a
grep list, and Chromium does not build in this container, so a mismatch
reaches the user's machine and costs them twenty minutes. One did -
`RecordAction` grew from two parameters to four and the call site inside
`ResolveApproval` was left at two. It reads perfectly and every other check
passed.

It is deliberately narrow: uniquely-named methods only, skipping overloads,
templates and anything it cannot count. A false positive on a real build is
worse than a miss.

Getting it to work took four goes, and the first three all reported success
while the bug went through - destructors read as constructor calls (28 false
findings), wrapped declarations invisible to a line-at-a-time regex (so
`RecordAction`, the method it exists for, was never in the table), a
declaration guard that skipped every single-line call ending in `;`, and an
argument counter that read a four-argument call containing a `StrCat({...})`
and a ternary as two. It now carries a `_self_test()` over the nine argument
shapes that broke it, and refuses to run if any of them miscounts.

### Two checks for code that compiles and does nothing

`tools/check-never-assigned.py` looks for a `raw_ptr`, `unique_ptr`,
`optional` or `WeakPtr` field that nothing ever assigns. `AgentRunner`
declared `page_` and `web_contents_`, handed both to every tool on every
call, and assigned neither, anywhere - so the agent had no browsing context
at all. A run started, talked to the model, and the first `read_page` had
nothing to read. It compiles, links and runs; the feature simply is not
there, and the declaration sitting in the header reads like proof that it is.

The first version of that check counted `= nullptr` as an assignment, so it
missed `web_contents_` - the very field it was written for. The declaration's
own initializer is now excluded, and the check is validated against the
pre-fix tree rather than against the fixed one.

`tools/check-mojo-surface.py` cross-checks `flux.mojom` against the handler,
the TS stub and the console. It fails on a method with no implementation, a
method missing from the stub, and an observer callback the browser never
fires - that last one was `OnLearnedFact`, which meant the Customize screen's
learned-facts list was permanently empty by construction. It reports, without
failing, a method nothing in the console calls: dead surface is a decision,
though it is usually a feature nobody finished wiring up.

That one also had to be broken three ways before it was trusted, and the
observer case did not fire the first time: the pattern matched
`FluxAgentService`'s own C++ `Observer::OnLearnedFact` and reported the
callback as fired while the mojo forward to the console was missing. It now
looks only for `observer_->X(` in the page handler, which is the only call
that actually reaches the renderer.

`tools/check-connector-defs.py` catches a connector definition the client
cannot execute. `facebook_pages` declared five operations with paths relative
to a base URL it did not have, so `ConnectorClient` concatenated an empty base
with `/{page_id}/feed` and handed GURL a scheme-less string - all five failed
before a request was made, and nothing in the file looks wrong, because the
`api` block was simply absent. `plain` is GraphQL: one endpoint, one method,
the operation is a named mutation rather than a path, so every operation parsed
with an empty method and path and the connector still reported itself
connectable. A definition with no executable operation must now say which
transport it needs (`api.transport`), so the check can tell a deliberate gap
from a forgotten field. MCP definitions are exempt - their operations name a
`tool` and the server supplies the transport.

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

### An optional is not a string, and there is no compiler to say so

`tools/check-nullable-mojom.py` catches a mojom `string?` used where a string
is expected. `QuestionAnswer::text` is nullable, so it is
`std::optional<std::string>` in C++, and

    base::StrAppend(&joined, {answer->text, "\n"});

does not compile. It reads perfectly and it is the obvious thing to write.

Its first version matched field NAMES anywhere and reported seven things, of
which **every one was wrong**: `CompletionResponse::error` and `Message::text`
are plain `std::string` members that happen to share a name with a nullable
mojom field, and `text`, `error` and `detail` are far too common for a name to
be evidence of anything. It is type-directed now - it only looks at a variable
it has watched being declared `mojom::XPtr`, and only at fields nullable in
that exact struct.

The bug underneath also erased meaning before it failed to build. That field
documents null as "skipped" and an empty string as "there is no value", which
lead the agent somewhere different, and joining an optional throws the
distinction away first.

### A comment is not a citation, and neither is a commit message

Recorded next to the AXTree entry because it is the same failure. A commit
message here claimed the run view's question panel "gets choices for free,
since both draw the same request". It does not: the Ask panel and the run view
are two implementations of that panel, and only one of them had been taught
about choices. The claim was written because it *should* have been true.

Check the thing before writing it down - in a comment, in CLAUDE.md, or in a
commit message. Writing it down is what makes it look settled.

### A serializer that drops a field does not degrade, it crashes

`tools/check-persisted-spec.py` reads TaskSpec's fields out of `flux.mojom` and
requires the workflow scheduler's ToDict/FromDict pair to carry every one.

`model` was in neither half. `TaskSpec::New()` leaves it a null `StructPtr`,
`PumpQueue` builds the provider with `MakeProvider(*spec->model)`, and mojo's
`StructPtr::operator*` is a `CHECK`, not a DCHECK - so "Run now" on any saved
workflow killed the browser process. A missing field presented as a crash
rather than as a workflow with no model, and the stack named
`PumpQueue`/`StartRun`/`RunWorkflowNow` while the actual bug was two hundred
lines away in a serializer nobody was looking at.

`credit_budget` was the same pair's other miss - round-tripped correctly, but
written as zero by a console that had no budget to give it, and `StartRun`
refuses zero. Both bugs, in one file, on the one path a user reaches by
clicking the obvious button.

The fields come from the .mojom rather than a list in the check, so adding one
to TaskSpec fails until the serializer carries it. That is the point: whoever
adds the next field will not have read the scheduler.

Two lessons that are not about serializers. **Credits are thousandths of a
cent** - `ChargeAndCheckBudget` builds them with `cost * 100000` - and the
depth presets were authored as 200/1000/5000, which is $0.002/$0.01/$0.05
against a modest Opus turn costing about 3,500 of them. Every preset was
refused on its first turn. A budget constant means nothing without the price of
one turn written next to it. And **the agent's tab is opened on first demand**,
not when the run starts: opening it in `Start()` put an about:blank tab in
front of the user for every run, including runs that only call a connector and
runs that ended before browsing at all.

### Authored data is only as real as the parser

`check-connector-defs.py` also fails on an operation field that
`connector_registry.cc` never reads. `params` and `body` were authored for 35
operations - names, allowed values and defaults, "1-500, default 50" - and
parsed by nothing, so `connector_list` handed the model an operation's name,
method and path and left it to invent the rest. Its own description says
"operation names and their required parameters are not guessable". It was
right, and then it did not supply them.

The allowlist is what makes this usable rather than noisy. `scope`,
`permissions`, `rate_limit`, `response`, `mutation` and `tool` are notes for
whoever maintains the definition, and naming them in `DOCUMENTED_ONLY` turns
that into a decision - a field added later and not parsed is not on the list,
so it fires. Without it the check would have reported six things that were
fine and one that was not, which is the shape nobody reads.

The same sweep across the other catalogues found nothing, and the reason is
worth keeping: a template card names its services as labels ("LinkedIn",
"Docs") while the connector registry uses ids ("google"). Two namespaces that
look like one, reconciled in `connectorMark` by slugifying both. 367 of 387
template references resolve to a real mark; the 20 that do not are all on the
"Missing:" list at the top of `connector_icons.ts`.

### A feature can be wired to nothing and still look finished

`tools/check-pref-flow.py` catches a pref that nothing outside the console ever
reads. Four were in that state at once, and every one of them presented as a
working screen:

- `kInstructions` - Customize > Instructions saved to disk and reached no
  model. The pref's own comment in `flux_prefs.h` says "Prepended to every
  task"; the system prompt was a string literal that never mentioned it.
- `kLearnedFacts` - `remember` wrote a diary the next run could not open,
  which is the entire point of the tool.
- `kAdoptedSkills` / `kUserSkills` - adopting one of the 138 shipped skills
  changed no behaviour. `kUserSkills` holds the skill's actual instructions
  and was read by nothing at all.

This is the pref-shaped version of what `check-never-assigned.py` finds in C++
fields, and it needed its own check for the same reason: the declaration reads
like proof the feature is wired up, the screen saves, the value survives a
restart, and nothing happens. Registration is not a read, and a write is not a
read - what counts is a `Get*` outside `src/browser/webui`, the layer that
draws the screens.

The rule that makes it usable is the exemption. A pref name handed to
something else - `SecretStore(profile, prefs::kApiKeys)` - is read through a
member rather than a literal, and no scan of this kind can follow it. The
first version called all three `SecretStore` prefs unread; saying nothing
about an escaping symbol is the only honest answer, and it is the difference
between four true findings and seven mixed ones.

It is validated against the real pre-fix tree rather than a synthetic break:
`git stash` the fix, and all four fire.

### A check that cries wolf is worse than no check

`tools/check-null-deref.py` catches a `base::Value::Find*` or `GetIf*` result
dereferenced without a null check. Those accessors return null on an absent key
or a wrong type, and what they read is never ours: a tool call written by a
model, a JSON body from a provider or a connector's API, an OAuth token
response. A null that reaches the dereference is not a failed call, it is the
browser process going down - someone else's malformed reply crashing the user's
machine.

Its first version reported all 43 call sites in `src/browser`, and **every one
was a false positive.** The tree writes them correctly, in two idioms the scan
did not know:

    if (const base::ListValue* c = root.FindList("content")) { ... *c ... }
    const std::string* s = root.FindString("stop"); x = s && *s == "tool_use";

The first declares inside the condition, so the body only runs non-null; the
second short-circuits on the same line, which a scan starting at the *next*
line cannot see. A report with a 100% false-positive rate is not a weak check,
it is an anti-check: nobody reads the 44th line of it, so the first real
finding is the one that gets skipped. Fixing the checker was the whole job -
there was no bug to fix in the C++.

The counterpart to "break it on purpose" is therefore **read the code before
believing the report.** Both directions have now cost time here: a check that
stayed silent through a real bug, and a check that shouted about 43 things that
were already right.

It is checked against four deliberate breaks, including guarding the *wrong*
variable (`other && *stop_reason == ...`), which is the shape this bug actually
takes in review - a guard that is present, reads fine, and covers nothing.

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
- `INFO:desktop_native_cursor_manager_win.cc:46` `Unable to get cursor info.
  Error = 5` - repeats heavily. `GetCursorInfo` returns ACCESS_DENIED whenever
  the process is not on the active desktop (lock screen, sleep, RDP, fast user
  switching); the comment above the `PLOG(INFO)` says so and the code assumes
  the cursor is visible and carries on.
- `ERROR:usb_service_win.cc:76/108` `SetupDiGetDeviceProperty(...) failed:
  Element not found` - a USB device that does not publish the queried
  property. The accessor returns `std::nullopt` and enumeration continues.
  Logged at ERROR by `USB_PLOG`, which is what makes it look like a fault.
- `ERROR:egl_util.cc:92` `EGL Driver message (...) eglCreateContext` about a
  requested GLES version exceeding the max - that text is written by the
  **driver**, not by Chromium. `egl_util.cc` is 97 lines of error-code-to-
  string plus one `EGL_KHR_debug` callback that relays whatever the driver
  says at ERROR. A refused 3.1 context falls back to 3.0.
- `gcm ... registration_request.cc` `DEPRECATED_ENDPOINT` - GCM registration
  against an endpoint this fork has no keys for. Sync and push are inert here.
- `bulk_leak_check_service` - the password leak check, which needs Google API
  keys this fork does not ship.

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
- `tools/connector-icons/` - generates `src/resources/connector_icons.ts`
  from Simple Icons (CC0), Gil Barbara's SVG Logos (CC0) and Material Design
  Icons (Apache-2.0). The marks are the only saturated colour in the console,
  which is the whole reason the rest of the UI has no accent. 29 of the 40
  have one; the other 11 are in no source this repo can redistribute -
  several were pulled from Simple Icons at the trademark holder's request -
  and render as a monogram rather than a traced imitation. **Nothing is
  mapped on a name match**: `logos:apollostack` is Apollo GraphQL and the
  connector is Apollo.io, `mdi:grain` is a sheaf of wheat and the connector is
  Grain the meeting recorder. A confidently wrong logo is worse than a letter.
  A mark you hold the rights to goes in `branding/connectors/<id>.svg` and
  wins over all of it. Output is committed, so a build machine needs no npm.
- `tools/oauth-registrations.py` - the checklist for giving a provider a
  built-in app, derived from the definitions so it cannot drift. The split it
  prints is the one that matters: **PKCE is the precondition, not a nicety.**
  Flux's client secret ships in a downloadable binary, so everyone has it, and
  PKCE is the only thing binding an authorization code to the client that
  asked for it - without it an intercepted redirect is enough to take the
  tokens. 7 of the 26 OAuth definitions set it today; the other 19 keep the
  bring-your-own-app form until their provider's docs are read and it is
  turned on deliberately. `check-connector-defs.py` refuses an `auth.client`
  on a definition without pkce.
- `data/connectors/*.json` - per-connector auth endpoints and operation maps.
  Every operation carries a `label` ("Create a to-do"), because the Connectors
  dialog lists them as the actions the agent will be able to take and a raw id
  in that list is a screen nobody reads. `auth.client` is an OAuth app Flux
  itself registered with the provider: for a desktop application the secret is
  not confidential - the binary is downloadable and Google's installed-app flow
  says so, which is why PKCE is mandatory there - and shipping one turns
  "register an app, enable six APIs, configure a consent screen" into a single
  Connect button. A user-registered app always wins over it, and a connector
  with neither still gets the registration form.
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
- **A running flux.exe locks the build output.** The link fails minutes in
  with `failed to write output './components_startup_metric_utils.dll':
  permission denied` - a component DLL nobody touched, which reads like a
  toolchain fault and is actually a browser still open. `build.ps1` now closes
  any flux.exe running from the output directory before it starts, and only
  from that directory. Two builds were lost to this before it was scripted.
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
