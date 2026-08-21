# Building Flux on a laptop

Short version: **yes, if you have ~150 GB free and 16 GB of RAM.** Cores mostly
decide how long you wait, not whether it works. RAM and disk decide whether it
works at all.

---

## 1. Will your laptop do it?

| Resource | Hard floor | Comfortable | Why |
|---|---|---|---|
| **Free disk** | 150 GB | 250 GB SSD | Checkout ~100 GB + build output 30–80 GB. HDD is not viable — the checkout alone has ~400k files. |
| **RAM** | 16 GB | 32 GB | Linking is the constraint: ~1.5 GB per parallel job. Under 16 GB you must cap `-j`, which `build/build.sh` now does automatically. |
| **Cores** | 4 | 8+ | Purely a time multiplier. |
| **Power** | — | plugged in | Hours at 100% CPU. On battery, most laptops throttle hard or sleep mid-build. |

Disk is the one that actually stops people. Check first:

```bash
df -h .              # Linux/macOS
Get-PSDrive C        # Windows PowerShell
```

If you're under 150 GB free, stop here — it will fail three hours in.

---

## 2. Realistic build times

For the **`dev`** config (`build/build.sh dev`), which is what you want for
working on the agent layer:

| Machine | First build | Incremental (our code) |
|---|---|---|
| 4-core laptop, 16 GB | 8–14 h | 1–3 min |
| 8-core laptop, 16 GB | 4–7 h | 1–2 min |
| 8-core laptop, 32 GB | 3–5 h | under 1 min |
| 16-core desktop, 64 GB | 1.5–2.5 h | seconds |

The **`release`** config (official, ThinLTO + PGO) is **4–6× slower** — 20+
hours on a laptop. Only build it when you're cutting a real binary.

**The first build is the painful one.** After that you're editing
`src/browser/`, which is a handful of files, and rebuilds are minutes. Touching
a widely-included Chromium header is the exception and can trigger a large
rebuild.

Run it overnight. That is the normal Chromium workflow, not a workaround.

---

## 3. Windows specifically

Your screenshots show Windows 11, and this is the one place the answer gets
more complicated. **Building Chromium on Windows is meaningfully harder than
on Linux**, and there are two routes.

### Route A — native Windows build (gives you a real `Flux.exe`)

Required, and there's no way around it:

1. **Visual Studio 2022** (Community is fine) with:
   - *Desktop development with C++*
   - *Windows 11 SDK* — the version Chromium pins, currently 10.0.22621.x
   - *C++ ATL* and *C++ MFC* for v143 build tools
2. **Debugging Tools for Windows** — from *Windows SDK → Modify → Debugging Tools*.
   The build fails without it, with an unhelpful error.
3. Tell depot_tools to use your VS rather than Google's internal toolchain:
   ```
   setx DEPOT_TOOLS_WIN_TOOLCHAIN 0
   ```
4. **Enable long paths** — Chromium exceeds `MAX_PATH` constantly:
   ```powershell
   git config --system core.longpaths true
   # and in an elevated PowerShell:
   Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' `
     -Name LongPathsEnabled -Value 1
   ```
5. **Exclude the checkout from Microsoft Defender.** This is not optional
   advice — real-time scanning of a 400k-file build can *double* build time:
   ```powershell
   Add-MpPreference -ExclusionPath C:\src\chromium
   ```

Native Windows builds are roughly 1.3–1.8× slower than Linux on identical
hardware, mostly NTFS overhead.

### Route B — WSL2 (much easier, but gives a **Linux** binary)

If you just want to work on the agent code, WSL2 is the far smoother path —
`build/fetch.sh` and friends run as written, no Visual Studio.

The catch is real though: **you get a Linux `chrome` binary, not a Windows
`.exe`.** Fine for developing and testing the agent layer; useless for shipping
to Windows users.

Two WSL2 rules that matter more than anything else:

- **Put the checkout inside the WSL filesystem** (`~/chromium`), never on
  `/mnt/c/...`. The 9p filesystem bridge is ~10× slower — a build that takes
  4 hours on ext4 takes over a day on `/mnt/c`.
- **Give WSL2 enough RAM.** It defaults to a fraction of your total. In
  `C:\Users\<you>\.wslconfig`:
  ```ini
  [wsl2]
  memory=12GB
  processors=8
  swap=16GB
  ```
  Swap matters — it's what saves a 16 GB machine from OOMing during link.

**Recommendation:** start with WSL2 to get the agent layer working, and set up
the native Windows toolchain later, when you actually need a distributable
`.exe`.

---

## 4. If you're tight on resources

Applied roughly in order of payoff:

**Shrink the checkout (~40 GB saved).** `build/fetch.sh` already clones with
`--no-history` and skips Android/iOS/NaCl deps. If you never need to bisect
upstream, keep it that way.

**Use `dev`, not `release`.** Already the default in `build/build.sh`. The
single biggest lever — `is_official_build=false` alone accounts for most of
the difference.

**Build only what you need.** The full `chrome` target pulls in a lot you may
not be touching:
```bash
build/build.sh dev chrome            # the browser
build/build.sh dev flux_unittests    # just our tests — minutes, not hours
```

**Cap link parallelism.** `build/build.sh` now detects low RAM and passes
`-j2`/`-j4` automatically. If you still OOM, force it lower.

**Add ccache** (Linux/WSL). Roughly halves rebuild time after a `gclient sync`:
```bash
sudo apt install ccache
export CCACHE_DIR=~/.ccache CCACHE_MAXSIZE=50G
# then add to your dev.gn:  cc_wrapper = "ccache"
```

**Don't build on battery, and don't let it sleep:**
```bash
systemd-inhibit --what=idle:sleep build/build.sh dev   # Linux
powercfg /change standby-timeout-ac 0                  # Windows
```

---

## 5. What to expect the first time

Be prepared for these — they're normal, not signs something is broken:

1. **`build/sync.sh` will probably fail on a patch.** The three patches in
   `patches/` have never been applied to a real tree. `sync.sh` stops on the
   offending file rather than half-applying. Open it, find where Chromium
   moved the code, fix the context lines. This is routine fork maintenance.
2. **`install-build-deps.sh` needs sudo** and installs a lot of packages.
3. **`gclient sync` takes 1–2 hours** and looks stalled at times. It isn't.
4. **The first `gn gen` fails if API keys are malformed** — empty strings are
   fine, missing quotes are not.

---

## 6. The honest alternative

If your laptop is under 16 GB of RAM or 150 GB free, renting is cheaper than
suffering:

- A 16-core / 64 GB / 500 GB cloud VM runs about $0.50–1.50/hour. A first build
  is one or two hours of that, and incremental builds are minutes.
- Build once in the cloud, then develop locally against that output only if
  you're on the same OS — otherwise just keep working in the VM.

This matters most for the **first** build. Once the tree is built, day-to-day
agent work is small incremental rebuilds that any laptop handles fine.
