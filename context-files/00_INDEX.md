# context-files — living map

**Not product documentation.** This directory is an April 2026 Claude research dump
(`aa2de8e8`, 2026-05-19) that was never updated as the LU Dev stack implemented its
own recommendations. It should stay out of the long-term product tree.

| File | Role |
|---|---|
| **This INDEX** | Current navigation. Start here. |
| **[STATUS.md](STATUS.md)** | Verified / obsolete / already landed on this stack / needs Astra |
| **01–14** | **Retired snapshots.** Read only with STATUS.md open. File:line citations are 2026-04-03. |

## Two trees — do not mix

| Tree | Ref | Meaning |
|---|---|---|
| Product | `origin/main` @ `f3a5add0` (2026-05-18) | Upstream Darkflame Universe. C++20, gcc:12, `GameMessages.cpp` 6469 lines, linear EntityManager scans, 20 test `.cpp` files. **No `context-files/`.** |
| LU Dev stack (this branch) | tip of `origin/refactor/inventory-handlers` | 18 commits ahead of main: tests, perf, C++23, `MessageHandlerRegistry`, inventory handler extract. Contains this dump. |

Scripts are **C++** (`dScripts/CppScripts.*`). There are **zero** `.lua` files in-tree. Lua exists only on the 2023 `scripting-lua` remote, 829 behind main.

## Architecture (verified on both trees)

Four processes spawned by Master: Auth, Chat, World (one per zone/instance), Master.
Client is live LU 1.10.64. Operators supply `res/` (packed or unpacked); the server does not ship assets.
CDClient catalog is FDB → SQLite (`CDServer.sqlite`). Player DB is MySQL **or** SQLite; character state is an XML blob.
Physics is a custom 2-shape grid (`dPhysics/`), not Havok/Bullet. Net is vendored RakNet **3.25**.
New *geometry* zones need a LUZ/LVL writer that does not exist. SQL + C++ scripts + behaviors **are** possible.

## Stats (spot-checked 2026-09-05)

| Metric | `origin/main` | this tip |
|---|---|---|
| `GameMessages.cpp` | 6469 | 6287 (inventory handlers extracted) |
| `InventoryComponent.cpp` | 1901 | 1911 |
| C++ standard | 20 | 23 |
| Dockerfile | gcc:12 | gcc:13 |
| MySQL migrations | 0–26 | 0–26 |
| SQLite player migrations | 0–9 | 0–9 |
| Test `.cpp` files | 20 | 33 |
| `GetEntitiesByComponent` | O(n) full-zone scan | O(1) index (`m_EntitiesByComponent`) |
| Dispatch | 4-entry map + ~112-case switch | registry first, then leftover switch (~108 cases) |
| dGame TODO/FIXME | 51 | 48 (triage “fix now” items landed) |
| First-party `.cpp`+`.h` (excl. thirdparty) | ~1330 (April dump) | 1375 |

`CMakeVariables.txt` still says **3.0.0**; `versions.txt` already describes **3.1**.

## Retired dump (01–14)

The April INDEX “top 3” is retired. Those items are either done on this stack or were never a weekend job:

1. **EntityManager indexed lookup** — landed (`a51a202d`). Still O(n) on `main`. Proximity scans are still O(n) on **both**.
2. **SLikeNet “~95% drop-in, include-path only”** — research claim, not a plan. RakNet 3.25 is vendored and heavily used (`dServer/`, `dNet/`, `GameMessageHandler.cpp` includes `RakPeer.h` / `RakNetworkFactory.h`). Needs a real diff study.
3. **Characterization tests before refactor** — scaffolding landed; GameMessages / inventory / mission coverage is still thin. Do not treat tests as done.

Docs 10 and 11 are **full documents**, not placeholders. Doc 13 is a Claude+MCP session note; restatement is an Astra job.

| # | File | Keep as |
|---|---|---|
| 01 | [Repository Map](01_REPOSITORY_MAP.md) | Retired. Lua claims are false. |
| 02 | [Technology Stack](02_TECHNOLOGY_STACK.md) | Retired. Entire Lua VM section is false. |
| 03 | [Code Organization](03_CODE_ORGANIZATION.md) | Retired. Module sketch still roughly right; file counts drifted. |
| 04 | [Content Systems](04_CONTENT_SYSTEMS.md) | Retired. LOT/CDClient load path still useful as orientation. |
| 05 | [Gameplay Mechanics](05_GAMEPLAY_MECHANICS.md) | Retired. Loop / component sketch; hot-path claims need STATUS.md. |
| 06 | [Data Persistence](06_DATA_PERSISTENCE.md) | Retired. Dual DB + XML blob still true; migration counts drifted. |
| 07 | [Optimization Opportunities](07_OPTIMIZATION_OPPORTUNITIES.md) | Retired. Several “do this” items already landed on this stack. |
| 08 | [Refactoring Strategy](08_REFACTORING_STRATEGY.md) | Retired. Registry + inventory extract already started. |
| 09 | [Replatforming Analysis](09_REPLATFORMING_ANALYSIS.md) | Retired. C++23 and gcc:13 already on this stack; SLikeNet is not a drop-in. |
| 10 | [Content Addition Guide](10_CONTENT_ADDITION_GUIDE.md) | Retired **full** guide (never a placeholder). SQL/LOT steps still useful. |
| 11 | [Content Dependencies](11_CONTENT_DEPENDENCIES.md) | Retired **full** FK map (never a placeholder). |
| 12 | [Development Workflow](12_DEVELOPMENT_WORKFLOW.md) | Retired. Build/test steps need a current pass. |
| 13 | [Claude vs MCP](13_CLAUDE_VS_MCP.md) | Retired. Claude-session bound. **Needs Astra.** |
| 14 | [Synthesis & Recommendations](14_SYNTHESIS_AND_RECOMMENDATIONS.md) | Retired. Phase 1–3 is not the live backlog. |

## Content addition (still true)

| Type | Possible today? |
|---|---|
| Items, missions, loot, simple skills | Yes — CDClient SQL |
| Complex skills / new behaviors | Yes — C++ `Behavior` subclass |
| NPCs (dynamic spawn) | Yes — SQL + optional `CppScripts` |
| Zone metadata + zone script | Yes — SQL + `dScripts/zone/` |
| Zone geometry / static placement | **No** — no LUZ/LVL writer |

## Next work

Canonical backlog is `/workspace/lu-status/project-bearing.md` §6, not doc 14’s Phase 1–3. This refresh is bearing task #1.
Related in-tree: [`docs/dgame-todo-triage.md`](../docs/dgame-todo-triage.md) (2026-05-25 snapshot; header notes drift).
