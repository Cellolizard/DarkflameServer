# context-files status

Verified 2026-09-05 against `origin/main` (`f3a5add0`) and this tip
(`docs/refresh-context-files` tracking `origin/refactor/inventory-handlers`).
April dump analyzed 2026-04-03, committed `aa2de8e8` (2026-05-19).

Living files: [`00_INDEX.md`](00_INDEX.md) and this STATUS. Docs 01–14 are retired snapshots.

## Verified — still true

On **both** `origin/main` and this tip unless noted.

- Four processes: Master, Auth, Chat, World. World is one process per zone/instance.
- Target client is live LU 1.10.64. Assets are operator-supplied (`res/`, optional `versions/` packs). Server does not ship the client.
- `dScripts/` is compiled C++ (`CppScripts.cpp/h`). **Zero** `.lua` files in-tree on main or this tip.
- CDClient catalog: client FDB converted to SQLite at Master boot (`dCommon/FdbToSqlite.*`). 43 CDClient table classes.
- Player DB: MySQL **and** SQLite backends. Character persistence is still an XML blob (`charxml`).
- Migrations on both trees: `migrations/dlu/mysql/` **0–26**, `migrations/dlu/sqlite/` **0–9**, `migrations/cdserver/` **0–7**.
- Physics is custom grid + sphere/box (`dPhysics/`). Not Havok/Bullet. Navmesh is Recast (`resources/navmeshes.zip`).
- Networking is vendored RakNet **3.25** (`thirdparty/raknet/`). `GameMessageHandler.cpp` includes `RakPeer.h` / `RakNetworkFactory.h`.
- No LUZ/LVL writer. New zone *geometry* and static object placement are blocked. CDClient SQL + C++ scripts + behaviors are not.
- `GetEntitiesByProximity` is still a full-zone distance scan (`EntityManager.cpp`) on both trees.
- `CMakeVariables.txt` = 3.0.0 vs `versions.txt` = 3.1. README clone/GHCR URLs are upstream DLU. Compose image is `ghcr.io/darkflameuniverse/darkflameserver:latest`.
- CI matrix uses `macos-15-intel` but a step is still gated on `macos-13`. CMake 4 is papered over in root `CMakeLists.txt`.
- Ghosting: `#1920` plus `#1947` “temp fixes so I can continue being on break.” Do not treat GM invis as finished.
- Tests on `main` are still thin (~20 `.cpp`). No combat-AI / inventory / mission / EntityManager / physics-grid tests on `main`.

## Obsolete — do not quote

False in the April dump, or rot that will mislead an agent.

| Claim | Reality |
|---|---|
| `dScripts/` is a Lua system / Lua VM | C++ `CppScripts`. Lua only on stale `scripting-lua` remote. |
| Docs 10 and 11 are placeholders | Both are full documents (~61 KB / ~40 KB). |
| `GameMessages.cpp` is 6,445 lines | `main` 6469; this tip 6287 after inventory extract. |
| Database migrations 0–23 / “24 migrations” | MySQL 0–26 (27 files). SQLite player 0–9. |
| C++20 is the stack standard | `main` is C++20; **this tip is C++23**. |
| Doc 07 “C++ 14-17” | False even in April. |
| SLikeNet is “~95% drop-in, include-path only” | Research claim. In-tree RakNet is customized 3.25. Treat as a study, not a plan. |
| April file:line as ground truth | `EntityManager.cpp:308` is now the **indexed** lookup on this tip, not the linear scan the dump pasted. Doc 07 still contains `/Users/mitchell/Documents/repos/DarkflameServer/...`. |
| INDEX “top 3” as unstarted | Indexed lookup, test scaffolding, and several other Phase-1 items exist on this stack. SLikeNet was never a 2-week include-path change. |
| Investigator target `DarkflameUniverse/DarkflameServer` as this fork’s current work | Product `main` tracks upstream. LU Dev work is the 18-commit stack on this fork. |
| Doc 13 “what Claude can do with MCP X” | Bound to a different agent/tooling session. See **Needs Astra**. |
| `GetEntitiesByComponent` “29+” call sites | ~40 production call sites on this tip (plus tests). |
| dGame TODO count 51 as current | 51 on 2026-05-25 triage; **48** on this tip after two fix-now items. |
| “No centralized dispatcher” (doc 08) | This tip has `MessageHandlerRegistry`; leftover switch still exists. |
| Doc 14 Phase 1–3 as the live roadmap | Use `/workspace/lu-status/project-bearing.md` §6. |

## Stack already landed — this tip only, **not** on `main`

18 commits, `aa2de8e8` … `4caf120a`. Later branches contain earlier ones.

| Commit | What |
|---|---|
| `aa2de8e8` | This `context-files/` dump (now retired except INDEX + STATUS) |
| `af3b7d74` + `8752164e` | Test scaffolding: EntityManager/lifecycle, Inventory, Mission, Buff, Character, QuickBuild, BaseCombatAI, CDClient tables, Config, BitStream. 20 → 33 test `.cpp` files. |
| `a51a202d` | EntityManager O(1) `byComponent` / `byGroup` / `byLOT` |
| `ac48923d` | `SkillComponent::Update` no longer rebuilds a per-frame multimap |
| `8d47c218` | Process-wide LOT→setIDs cache in `CheckItemSet` (LIKE still used on first miss) |
| `2c7cc781` | Bulk BitStream string writes (`GameMessages`, `ChatPackets`) |
| `27a2019b` | ASan + coverage CMake presets, Dockerfile **gcc:13**, gtest discovery at ctest time |
| `31e4aa74` + `c8c4b5bc` + `771d1799` | Dead `PropertyTeleportBehavior` field; `g_TestedMissions` comment; `docs/dgame-todo-triage.md` |
| `a12c79d3` | `CMAKE_CXX_STANDARD 23` |
| `533f0fcb` | Flat `Component*` array on `Entity` (replaces `unordered_map`) |
| `b1a610ee` + `b32a13c1` + `0198d4df` | `MessageHandlerRegistry` + dispatch routed through it + registry tests |
| `8035b747` + `4caf120a` | Equip / Unequip / Move / Remove inventory handlers extracted off the legacy switch |

Still **not** done on this tip:

- Rest of GameMessages (combat / property / mission / movement) — ~108-case leftover switch
- `InventoryComponent` 3-class split
- Spatial index for proximity
- Async DB / connection pool
- ItemSets schema normalization (cache is a process-wide workaround, not a junction table)
- SLikeNet (correctly not done)
- LUZ/LVL writer
- Characterization tests that would actually lock EquipItem / mission progression / remaining handlers

## Needs Astra

Work that should not be done from the April dump as-is.

1. **Restate or delete doc 13.** Claude+Blender/image-gen/SQLite MCP session notes are not an Astra/Codex/Grok-Build playbook. LUZ/LVL-is-unsolved is the one durable content fact; keep that, drop the rest or rewrite for the current tool stack.
2. **Do not execute doc 14 Phase 1.** Those “quick wins” are already on this stack or were never quick (SLikeNet). Next code work is the bearing backlog: rebase/evaluate the stack vs `main`, characterization tests, then further handler split.
3. **Do not treat file:line in 01–14 as review comments.** Re-read the current file.
4. **Doc 10/11 content recipes** (LOT inserts, FK map) are the parts worth a future rewrite, not a banner. Schema/column names should be re-checked against `dDatabase/CDClientDatabase/CDClientTables/` before any SQL is generated.
5. **Triage line numbers** in `docs/dgame-todo-triage.md` already drift (e.g. GameMessages reputation TODO was `:5769`, now `:5611`). Decisions still stand if surrounding code is unchanged; citations do not.

## How to use this directory

- New agent: read INDEX + this file. Open 01–14 only for historical orientation.
- Product questions: `origin/main`.
- “Did we already do X?”: the stack table above, then `git log origin/main..HEAD`.
- Backlog: `/workspace/lu-status/project-bearing.md` §6, plus `/workspace/lu-status/backlog-addendum-env.md` (full-server run env).
