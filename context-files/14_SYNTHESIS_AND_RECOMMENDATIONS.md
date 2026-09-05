> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> **Do not execute Phase 1–3 from this file.** The live backlog is `/workspace/lu-status/project-bearing.md` §6.
>
> **Critical falsehoods in this file:** “Highest-impact thing to do today” (EntityManager index) and several other Phase-1 items **already landed on this stack**. `GameMessages.cpp` is not 6,445 lines. Migrations are not 0–23. Docs 10/11 were never placeholders. SLikeNet is not a small drop-in. C++23 / gcc:13 / sanitizer presets / MessageHandlerRegistry / inventory handler extract are already here (not on `main`). Characterization tests are started, not done.

# DarkflameServer — Synthesis & Recommendations

**Executive Investigation Report**  
**Analysis Date**: 2026-04-03  
**Investigator**: Claude AI Agent (claude-sonnet-4-6)  
**Scope**: Full codebase — read-only analysis, no modifications  
**Repository**: DarkflameUniverse/DarkflameServer (C++20, CMake, ~1,330 source files)

---

## 1. Executive Summary

### What Is This Codebase?

DarkflameServer is a **mature, community-maintained C++ private server emulator** for the now-defunct LEGO Universe MMORPG (2010–2012). The project emulates the full multi-server stack: authentication, chat, master coordination, and world simulation — all reproducing a real commercial MMORPG experience for a dedicated player community.

**Scale**: ~1,330 source files (667 .cpp / 663 .h, excluding thirdparty). The largest subdirectory is `dScripts/` with 613 files implementing zone-specific and object-specific C++ gameplay scripts. Core game logic in `dGame/` spans 341 files.

**Maturity**: This is not a toy project. It has 24 database migrations, CMake presets for Linux/Windows/macOS, a CI/CD pipeline (`.github/workflows/`), a functioning Docker deployment, and a well-partitioned multi-server architecture. The codebase is firmly in "production" state for its intended use.

**Purpose and Constraints**: The codebase is architecturally faithful to the original game's design — including RakNet UDP networking, client-side physics with server validation, Flash AMF3 serialization, and a read-only CDClient SQLite database (`CDServer.sqlite`) that holds all game content definitions. Adding new content requires editing that SQLite database; adding entirely new zones is currently **impossible** due to the proprietary `.luz`/`.lvl` binary format (no editor exists).

---

### Top 5 Code Quality / Architecture Findings

| # | Finding | Severity | File Reference |
|---|---------|----------|---------------|
| 1 | **GameMessages.cpp is a 6,445-line monolith** containing 101+ handler functions with no logical separation. Adding new message types requires editing this single massive file, creates extreme compilation times, and makes tracing message flows nearly impossible. | CRITICAL | `dGame/dGameMessages/GameMessages.cpp` |
| 2 | **GetEntitiesByComponent() called 29+ times** performs O(n) linear scans of ALL entities in a zone on every call, including in hot equip/kill code paths. No component-indexed lookup exists. | HIGH | `dGame/EntityManager.cpp:308-320` |
| 3 | **InventoryComponent.cpp is 1,901 lines** mixing inventory management, item validation, buff application, persistence, and skill logic. The `EquipItem()` function triggers full-zone entity scans for specific magic LOT values (6416, 8092). | HIGH | `dGame/dComponents/InventoryComponent.cpp` |
| 4 | **51 TODO/FIXME comments in dGame/** indicate incomplete implementations including unimplemented precondition checks (`Preconditions.cpp:155-189`), deferred safety changes (`PetComponent.cpp:50`), and unresolved logic issues (`QuickBuildComponent.cpp:436`). | MEDIUM | Multiple files in `dGame/` |
| 5 | **Character state serialized as XML blob** stored in a single `charxml.xml_data` TEXT column. All mission progress, inventory, skills, and levels are serialized into one XML string — making partial updates, querying player data, or performing analytics impossible without loading the entire character. | MEDIUM | `dDatabase/GameDatabase/ITables/ICharacterTable.h`, `dGame/dEntity/Character.cpp` |

---

### Top 5 Performance Optimization Priorities

| Priority | Optimization | Effort | Expected Impact |
|----------|-------------|--------|----------------|
| 1 | **Component-indexed entity lookup** — Add `std::unordered_map<eReplicaComponentType, std::vector<Entity*>>` to EntityManager, maintained on component add/remove. Eliminates O(n) scans called 29+ times per event. | SMALL | HIGH: Direct lookup replaces full-zone iteration for every component query |
| 2 | **Entity component array-based storage** — Replace `unordered_map<eReplicaComponentType, Component*>` in Entity with a flat `Component* m_ComponentArray[MAX_COMPONENT_TYPE]`, eliminating hash computation and dynamic_cast overhead on every component access. | MEDIUM | HIGH: GetComponent<T>() becomes O(1) array index + static_cast; called every frame per entity |
| 3 | **SkillComponent::Update() multimap allocation** — Current code creates a new `std::multimap` every frame for every entity with skills, then move-assigns the whole container. Replace with erase-remove idiom on the existing container. | SMALL | MEDIUM: Eliminates per-frame heap allocation at 60fps for every skilled entity |
| 4 | **CDClient LIKE query in CheckItemSet()** — `SELECT setID FROM ItemSets WHERE itemIDs LIKE ?` is unindexable and runs on every item equip. Cache results in a `static std::unordered_map<LOT, std::vector<uint32_t>>`. | SMALL | MEDIUM: Converts repeated unindexable SQL to O(1) in-memory lookup |
| 5 | **Async database I/O** — All MariaDB queries block the game thread (potentially 10–100ms per query). A dedicated database thread with a work queue and connection pool would prevent frame stalls during character saves, leaderboard updates, and property queries. | LARGE | HIGH: Eliminates frame spikes; throughput improvement ~5–10x for concurrent queries |

---

### Top 5 Refactoring Priorities

| Priority | Refactoring | Effort | Benefit |
|----------|-------------|--------|---------|
| 1 | **Split GameMessages.cpp into subsystem handlers** — Create `IMessageHandler` interface and `MessageDispatcher`, then extract handlers into `InventoryMessageHandler`, `CombatMessageHandler`, `PropertyMessageHandler`, `MissionMessageHandler`, and `MovementMessageHandler`. | LARGE (6–8 weeks) | Modularity, testability, reduced compile times, Open/Closed principle |
| 2 | **Replace Entity component unordered_map with flat array** — Swap `unordered_map<eReplicaComponentType, Component*>` for `Component* m_ComponentArray[MAX_COMPONENT_TYPE]`. Enables static_cast instead of dynamic_cast and eliminates hash overhead. | MEDIUM (1–2 weeks) | +234 bytes/entity but deterministic O(1) lookup and cache locality |
| 3 | **Extract InventoryComponent into focused classes** — Split into `InventoryManager` (slots/items), `ItemEquipmentHandler` (equip/unequip), and `ItemValidator` (precondition checks). Remove magic LOT special-cases from generic equipment path. | LARGE (3–4 weeks) | Easier testing, clearer responsibility boundaries, removes full-zone scans from equip path |
| 4 | **Normalize ItemSets database schema** — Replace `WHERE itemIDs LIKE '%LOT%'` LIKE-matching on a text column with a proper junction table `ItemSetMembers(setID, lot)` with an index on `lot`. | MEDIUM (requires DB migration) | Eliminates unindexable query; enables proper relational modeling |
| 5 | **Introduce group-indexed entity lookup** — Add `std::unordered_map<std::string, std::vector<Entity*>> m_EntitiesByGroup` to EntityManager. Current `GetEntitiesInGroup()` performs O(n×m) nested string comparison scan. | SMALL (2–3 days) | Eliminates nested loop; used by PropertyManagementComponent on every group query |

---

### Extensibility Maturity Score: **6 / 10**

**Justification:**

The component-based entity system (`Entity` + `Component` subclasses) is genuinely extensible — adding a new game component requires creating a class in `dGame/dComponents/`, registering it in `eReplicaComponentType`, and populating `CDComponentsRegistryTable` entries. No inheritance hierarchies need modification.

Content (items, missions, loot tables, skills) is almost entirely data-driven through the CDClient SQLite database, making addition of new content achievable through SQL alone for most common content types.

However, several factors cap the score:
- **Zone placement is frozen** — no editor exists for `.luz`/`.lvl` binary formats; new zones cannot be added without community tooling that does not yet exist
- **GameMessages.cpp monolith** means adding a new client→server message type requires editing a 6,445-line file with no safety guardrails
- **No scripting hot-reload** — all `dScripts/` are compiled C++; any behavioral change requires a full server recompile and restart
- **Character XML blob** means adding new persistent character state requires careful XML parsing additions; no schema-driven approach
- **~10% of CDClient table lookups are cached** — repeated queries to the same tables are common, creating latent fragility under high entity counts

Points awarded for: clean ECS pattern, database-driven content, dual-backend DB abstraction, migration system, well-organized CMake.

Points deducted for: LUZ/LVL lock-in, monolithic message handler, compiled-only scripts, missing spatial index, XML character state.

---

## 2. Priority Matrix

### Quick Wins — High Impact, Low Effort (Do These First)

These provide meaningful improvements with a week or less of development:

1. **Component-indexed EntityManager lookup** (`SMALL` effort) — Add `m_EntitiesByComponent` and `m_EntitiesByGroup` maps. File: `dGame/EntityManager.h/cpp`. Eliminates the most commonly called O(n) scans.

2. **SkillComponent multimap per-frame allocation** (`SMALL` effort) — Replace `std::multimap<> keep{}` rebuild in `SkillComponent::Update()` with erase-remove idiom. File: `dGame/dComponents/SkillComponent.cpp:145-196`.

3. **ItemSet lookup caching** (`SMALL` effort) — Static `unordered_map<LOT, vector<uint32_t>>` in `InventoryComponent::CheckItemSet()`. File: `dGame/dComponents/InventoryComponent.cpp:1138-1169`.

4. **SLikeNet upgrade** (`SMALL` effort, ~2 weeks) — Drop-in RakNet replacement with active maintenance and security patches. Files: `thirdparty/raknet/`, `CMakeLists.txt` include paths. See doc 09.

5. **Docker + build system quick-fixes** (`SMALL` effort, 1–2 days each):
   - Add `.dockerignore` to reduce Docker build context
   - Update Dockerfile from `gcc:12` to `gcc:13` (enables C++23 features)
   - Add `sanitizer` and `coverage` CMake presets to `CMakePresets.json`

6. **String serialization bulk writes** (`TRIVIAL`) — Replace character-by-character BitStream writes in `GameMessages.cpp` with `bitStream.Write(str.data(), str.size())`. Files: `GameMessages.cpp:1014-1021` and 50+ similar patterns.

7. **Fix misspelled variable** (`TRIVIAL`) — `arbitaryInventorySize` → `arbitraryInventorySize`. File: `InventoryComponent.cpp:1198`.

---

### Strategic Investments — High Impact, High Effort (Plan and Fund These)

These are architecturally significant changes requiring weeks of sustained effort:

1. **Async database I/O layer** (`LARGE`, 4–6 weeks) — Create `DatabaseThread` with work queue and `ConnectionPool` (10 connections). Game thread submits queries non-blocking; results handled via callback. Critical for frame stability under load. Files: new `dDatabase/DatabaseThread.h`, `dDatabase/ConnectionPool.h`.

2. **GameMessages.cpp decomposition** (`LARGE`, 6–8 weeks) — Create `IMessageHandler` interface, `MessageDispatcher`, and per-subsystem handler classes. Migrate ~101 handler functions across 7+ subsystem files. Files: new `dGame/dGameMessages/MessageHandlers/` directory.

3. **InventoryComponent refactoring** (`LARGE`, 3–4 weeks) — Split 1,901-line class into `InventoryManager`, `ItemEquipmentHandler`, `ItemValidator`. Remove magic LOT values from generic code paths. File: `dGame/dComponents/InventoryComponent.cpp`.

4. **Spatial partitioning for proximity queries** (`LARGE`) — `GetEntitiesByProximity()` performs O(n) distance calculations across all entities. A spatial grid or quadtree reduces this to O(k) for radius k. Needed for skill execution, loot drops, and AI targeting at scale. File: `dGame/EntityManager.cpp:332-340`.

5. **Entity component flat array storage** (`MEDIUM`, 1–2 weeks) — Replace `unordered_map<eReplicaComponentType, Component*>` with `Component* m_ComponentArray[MAX_COMPONENT_TYPE]` (currently ~140 component types). Eliminates hash + dynamic_cast overhead on every component access. File: `dGame/Entity.h:390`, `Entity.cpp`.

---

### Nice-to-Haves — Low Impact, Low Effort (When Time Permits)

- Replace LOG() macro calls with `std::print` (C++23, type-safe, cleaner API)
- Replace `std::map` with `std::flat_map` in CDClient table classes (cache-friendly reads at startup)
- Replace `std::vector m_ItemSetsChecked` with `std::unordered_set<LOT>` for O(1) membership test
- Add missing `const` qualifiers to getter methods across components
- Audit and clean up unused `#include` directives (50+ includes per .cpp file is common)
- Add docker-compose health checks for ChatServer and WorldServer services
- Document the 51 TODO/FIXME comments as GitHub issues and triage them

---

### Avoid — High Effort, Low Impact (Don't Do These)

- **Replace RakNet with GameNetworkingSockets** — Protocol incompatibility breaks all existing clients. Requires rewriting all 200+ message types and the entire replication layer. `VERY_LARGE` effort with zero gameplay improvement. SLikeNet achieves the same security/maintenance goals with `SMALL` effort.
- **Replace C++ scripts with Lua scripting** — The codebase already replaced the original Lua with compiled C++. Re-introducing a scripting language would add interpreter overhead, complicate debugging, and split the codebase between two languages — with no real benefit since compile+restart cycles are already fast.
- **Add a full SQL ORM (sqlpp11)** — Would require adding a dependency, generating schema code, and migrating hundreds of manual SQL strings. Current parameterized query approach is sufficient and safer than an ORM for this access pattern.
- **Containerizing with distroless base** — Very low operational benefit for a game server; adds complexity to debugging and log inspection without measurable improvement.

---

## 3. Phased Implementation Roadmap

### Phase 1 — Month 1: Stabilization & Quick Wins

**Goal**: Eliminate the most impactful performance footguns with minimal risk.

| Task | File | Effort | Done When |
|------|------|--------|-----------|
| Add `m_EntitiesByComponent` index to EntityManager | `dGame/EntityManager.h/cpp:308-340` | 2–3 days | All 29+ GetEntitiesByComponent() calls return in O(1) |
| Add `m_EntitiesByGroup` index to EntityManager | `dGame/EntityManager.cpp:295-306` | 1 day | GetEntitiesInGroup() returns direct from map |
| Fix SkillComponent per-frame multimap allocation | `dGame/dComponents/SkillComponent.cpp:145-196` | 1 day | No heap allocation in Update() hot path |
| Cache ItemSet SQL lookups | `dGame/dComponents/InventoryComponent.cpp:1138` | 1 day | CheckItemSet() reads from static map after first call |
| Fix bulk BitStream writes | `dGame/dGameMessages/GameMessages.cpp:1014+` | 2 days | No character-by-character loops remain |
| Upgrade to SLikeNet | `thirdparty/raknet/`, `CMakeLists.txt` | 1 week | Server compiles with SLikeNet, passes integration tests |
| Update Dockerfile to gcc:13 + add sanitizer presets | `Dockerfile`, `CMakePresets.json` | 2 days | `cmake --preset asan` builds and runs tests cleanly |
| Add `.dockerignore` | repo root | 1 hour | Docker build context < 50MB |
| Audit + create GitHub issues for 51 TODO/FIXME items | All `dGame/` files | 2 days | Every TODO has a tracking issue |

---

### Phase 2 — Months 2–3: Strategic Refactoring

**Goal**: Address the two largest architectural liabilities — the message handler monolith and the InventoryComponent — while setting up async database foundation.

| Task | File | Effort | Done When |
|------|------|--------|-----------|
| Create IMessageHandler + MessageDispatcher | New: `dGame/dGameMessages/MessageHandlers/` | 1 week | Interface exists; dispatcher tested with unit tests |
| Migrate InventoryMessageHandler | Extract from `GameMessages.cpp:1-6445` | 1.5 weeks | 10–15 inventory handlers in own class; old wrappers route to new |
| Migrate MissionMessageHandler | Extract from `GameMessages.cpp` | 1.5 weeks | 8–12 mission handlers extracted and tested |
| Migrate CombatMessageHandler | Extract from `GameMessages.cpp` | 1 week | 6–10 combat handlers extracted |
| Replace Entity component unordered_map with flat array | `dGame/Entity.h:390`, `Entity.cpp` | 1 week | GetComponent<T>() uses static_cast + array index |
| DatabaseThread skeleton + connection pool | New: `dDatabase/DatabaseThread.h/cpp` | 1 week | Thread running; CDClient queries going async |
| Upgrade C++ standard to C++23 in CMakeLists | `CMakeLists.txt:15` | 2 days | Server compiles cleanly with `set(CMAKE_CXX_STANDARD 23)` |
| Adopt std::print for logging | `dCommon/Logger.h/cpp` + callsites | 1 week | LOG() macro replaced; type-safe format strings throughout |

---

### Phase 3 — Month 4+: Major Improvements

**Goal**: Complete the remaining strategic investments and begin modernization upgrades from doc 09.

| Task | File | Effort | Done When |
|------|------|--------|-----------|
| Migrate hot DB paths to async (top 20 queries by frequency) | `dWorldServer/WorldServer.cpp:490`, components | 2 weeks | Character saves, property queries, leaderboard writes are non-blocking |
| Complete GameMessages.cpp decomposition (remaining subsystems) | `dGame/dGameMessages/` | 2–3 weeks | <500 lines remain in original file; all handlers in subsystem classes |
| Split InventoryComponent into 3 classes | `dGame/dComponents/InventoryComponent.cpp` | 3 weeks | InventoryManager, ItemEquipmentHandler, ItemValidator in separate files |
| Normalize ItemSets DB schema + migration | `migrations/dlu/sqlite/`, `CDItemSetsTable` | 1 week | Junction table replaces LIKE text query; migration tested on both backends |
| Implement spatial grid for proximity queries | `dGame/EntityManager.h/cpp:332-340` | 2–3 weeks | GetEntitiesByProximity() queries grid cells, not all entities |
| Adopt std::flat_map for CDClient tables | `dDatabase/CDClientDatabase/CDClientTables/*.h` | 1 week | All CDTable<> types use flat_map; startup benchmark shows improvement |
| Add sanitizer CI step | `.github/workflows/` | 2 days | ASAN + UBSAN run on every PR |
| std::expected error handling in GameMessages | New parser functions | Ongoing | New message parsers return expected<void, Error>; errors logged with context |

---

## 4. Answers to Key Questions

### 1. How do I add 10 new items without breaking the game?

Pure SQL operation — no C++ required for standard items.

**Steps** (from doc 12, Part 3A):
1. Open `resServer/CDServer.sqlite` with `sqlite3` or DB Browser for SQLite
2. For each item, insert into three tables in order:
   ```sql
   INSERT INTO Objects (id, name, type) VALUES (12345, 'MySword', 'weapon');
   INSERT INTO ItemComponent (id, equipLocation, baseValue, rarity, itemType, stackSize) VALUES (12345, 'RightHand', 500, 3, 2, 1);
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (12345, 2, 12345);
   ```
3. Use LOT IDs in the 12000–12999 range to avoid conflicts with original content
4. Restart the WorldServer (CDClient is loaded once at startup)
5. Verify in-game: `/gmadditem 12345 1` (requires GM level 8)

**Biggest risk**: Choosing a LOT ID already in use. Always query `SELECT id FROM Objects WHERE id = 12345` before inserting. If adding loot table entries, ensure `LootMatrixIndex` and `LootTableIndex` values are also fresh.

---

### 2. How do I add a new mission type?

Standard missions are also pure SQL. Custom scripted missions require a C++ script.

**For standard missions** (doc 12, Part 3B):
```sql
INSERT INTO Missions (id, defined_type, offer_objectID, reward_currency, isMission) VALUES (10001, 'mission', 5000, 1000, 1);
INSERT INTO MissionTasks (id, taskType, target, targetValue, uid) VALUES (1000100, 1, 5001, 3, 1);  -- Kill 3 of LOT 5001
INSERT INTO MissionNPCComponent (id, missionID, offersMission, acceptsMission) VALUES (5000, 10001, 1, 1);
```

**For custom scripted behavior** (e.g., conditional logic, cutscenes): Create `dScripts/CustomMissionScript.cpp` inheriting `CppScripts::Script`, implement `OnMissionDialogueOK()`, and register in `dScripts/CppScripts.cpp`. Then set `scriptID` in the `CDScriptComponentTable` entry for the NPC.

Key task type values: `1` = Defeat, `2` = Collect, `3` = Deliver, `4` = Use skill, `5` = Visit location, `6` = Interact. See `dGame/dMission/MissionTask.h` and `dCommon/dEnums/eMissionTaskType.h` for the full enum.

---

### 3. How do I add a new character with unique abilities?

"Character" in this codebase means a player-controlled entity. "New character type" is not a supported concept — all players share the same character class. What you can add is **a new set of skills/behaviors** and **a custom script**.

**For a new NPC with unique abilities**:
1. Create `CDObjects` entry with a new LOT
2. Create `CDComponentsRegistry` entries mapping the LOT to components: `BaseCombatAIComponent`, `DestroyableComponent`, `SkillComponent`
3. Create `CDSkillBehavior`, `CDBehaviorTemplate`, and `CDBehaviorParameter` entries for each unique ability
4. Create a C++ script in `dScripts/` inheriting `CppScripts::Script` with custom logic
5. Link via `CDScriptComponentTable`

For unique behaviors not representable by the ~50 existing behavior templates (e.g., teleporting players, spawning entities), you must create a new `Behavior` subclass in `dGame/dBehaviors/`, implement `Calculate()` and `Execute()`, and register it in the behavior factory.

**Key files**: `dGame/dBehaviors/Behavior.h`, `dScripts/CppScripts.h`, `CDSkillBehaviorTable.h`.

---

### 4. How do I add a new zone?

This is the hardest content addition and currently **only partially possible**.

**What you can do**:
1. Add a `CDZoneTable` entry with a new `zoneID` (defines metadata: name, caps, physics config)
2. Create a C++ zone script in `dScripts/zone/` for scripted behavior
3. Spawn objects dynamically from the script (workaround for zone placement)

**What you cannot currently do**:
- Create or edit `.luz` (zone layout) or `.lvl` (level/collision) files — these are proprietary binary formats with no known editor
- Define static object placement, trigger volumes, or terrain collision
- Create entirely new visual environments (all terrain is client-side from original game assets)

**Practical approach**: Reuse an existing zone by creating an instance of it (via CDZoneTable), then modify NPC/object presence dynamically through scripts. The Racing zone (`RacingControlComponent`) and instanced activity zones (`CDActivitiesTable`) both follow this pattern.

**Future**: A LUZ/LVL writer MCP would unlock this. Doc 13 §3.1 and §5.7 rate this at 80-150 hours given the existing parser in `dZoneManager/` as a spec — large, but tractable for someone willing to do the reverse-engineering. No such tool exists as of 2026; it is the highest-impact missing capability in the MCP stack for this project.

---

### 5. What is the biggest risk when modifying this codebase?

**The XML character serialization system.**

All player state — inventory, mission progress, skills, levels, property ownership — is stored in a single `charxml.xml_data` TEXT blob. Changes to serialization format (e.g., adding new fields to `MissionComponent::UpdateXml()`) can silently corrupt or discard data for characters saved before the change.

There is no schema versioning for the XML format. A character saved before a migration-required change may fail to load, lose data, or cause null pointer dereferences when code expects fields that don't exist in the old XML.

**Mitigation before any serialization change**: Make a backup of the `charxml` table. Add backward-compatible default values in `LoadFromXml()` for any new fields. Test with a character that predates the change.

Secondary risk: **null pointer dereferences in DestroyableComponent** (doc 07, C.3). The death/kill path performs database operations with direct character pointer dereferences and limited null checks. Any modification to combat or kill handling should add explicit null guards.

---

### 6. What is the single highest-impact improvement I could make today?

**Add component-indexed lookup to EntityManager** — specifically, add `m_EntitiesByComponent` and `m_EntitiesByGroup` maps and maintain them on `AddComponent`/`RemoveComponent`.

**Why**: `GetEntitiesByComponent()` is called 29+ times across the codebase (in GameMessages, InventoryComponent, DestroyableComponent), including in hot paths triggered on every item equip and every entity death. In zones with 500+ entities, each call iterates the entire entity map. This single change converts the most-called query from O(n) to O(1) lookup with a result vector, eliminates repeated vector allocations, and has zero backward-compatibility impact.

**File**: `dGame/EntityManager.h` (add two maps), `dGame/EntityManager.cpp` (update 3 methods: `CreateEntity`, `DestroyEntity`, and the `GetEntitiesByComponent`/`GetEntitiesInGroup` implementations).

**Effort**: 2–3 days including testing.

---

### 7. What external systems (MCP) could most accelerate development?

This question changed shape since the original 2026-04 analysis. Generic SQLite, Blender, and image-gen MCP servers now exist as community/reference implementations — the practical question is "which to connect" rather than "which to build." Doc 13 has the full breakdown; the ranking below is the executive view.

Ranked by impact-to-setup-effort for this project:

1. **SQLite MCP** (`HIGHEST`, ~10 min to install): Claude executes SQL directly against `resServer/CDServer.sqlite` — schema inspection, ID collision checks, multi-table inserts in transactions, post-insert verification. Closes the entire content-creation loop for items, missions, loot, skills, NPCs, pets, and quick-builds. Use any maintained SQLite MCP server. **Install this first.**

2. **Blender MCP** (`HIGH`, ~30-60 min setup): Claude writes `bpy` scripts that the MCP executes inside Blender, including the NIF exporter add-on. Unlocks 3D model creation for items, NPCs, props, quick-builds. **NIF caveat applies** — LU uses an early NetImmerse dialect; exported files may need a NifSkope post-step until LU-specific NIF tooling matures (see `13_CLAUDE_VS_MCP.md` §2.2).

3. **Image-gen + texconv MCP** (`HIGH`, ~20 min setup): Pairs with Blender. Claude prompts an image generator for diffuse/normal/roughness maps, the texconv wrapper converts to DDS at the right compression (BC3/BC5). Standalone use (re-skinning existing items) is also high-value.

4. **Audio-gen MCP** (`MEDIUM`, ~20 min setup): TTS for voice lines on custom missions; music/SFX gen for ambient and one-shots; ffmpeg/oggenc wrapper to OGG Vorbis. Not on the critical path — most content can ship reusing existing audio — but it removes a real friction point for voice-acted custom quests.

5. **draw.io MCP** (`MEDIUM`, often pre-connected): For diagrams in refactor proposals and onboarding docs. Helps communicate the GameMessages.cpp decomposition and EntityManager indexing changes to other contributors.

6. **GitHub MCP** or `gh` CLI (`LOW-MEDIUM`, ~5 min): For PR/issue workflows around the Phase-1 plan. `gh` covers most of this without an MCP.

7. **LUZ/LVL Writer MCP** (`VERY HIGH IF BUILT`, 80-150 hours): Does not yet exist. The single highest-impact missing capability for this codebase — would unlock new-zone authoring, currently impossible. The parser in `dZoneManager/Zone.cpp` + `Level.cpp` is the most complete open-source spec to build against.

8. **Game Client Testing MCP** (`FUTURE`, 50+ hours): Automated in-game verification. Still gated on the closed-source LU client; could be approached via input emulation + screen scraping, but brittle.

**Immediate recommendation**: Connect SQLite MCP first (10 minutes, unlocks ~80% of content-creation friction). Add Blender + image-gen MCPs when your first content task needs custom assets. Anyone with capacity for a community-scale contribution should consider the LUZ/LVL writer — it's the one thing that would make this server fully content-extensible.

---

### 8. What is the current test coverage situation?

**Current state**: Tests exist in `tests/` directory using a `TestSQL` mock database backend (`dDatabase/GameDatabase/TestSQL/`). The database abstraction layer was designed with testability in mind — `GameDatabase` is an abstract interface, and `TestSQL` provides an in-memory test double.

**What is tested**: Unit tests for database table interfaces, some component logic, and serialization utilities. The test framework is CMake-integrated (`add_subdirectory(tests)` in root `CMakeLists.txt`).

**Critical gaps**:
- **No integration tests** for the game loop itself (entity creation, component update, serialization)
- **No tests for GameMessages.cpp handlers** (101+ handlers with zero test coverage)
- **No behavior tree tests** (70+ behavior types in `dGame/dBehaviors/`)
- **No character XML round-trip tests** (the highest-risk serialization path)
- **No network protocol tests** (packet serialization/deserialization)

**Pre-refactoring gap that must be filled**: Before splitting `GameMessages.cpp`, write characterization tests for at least the 20 most-called handlers (inventory equip, skill cast, mission dialogue, property placement). Without these, refactoring the monolith risks silent behavioral regressions.

**Test coverage estimate**: < 15% of game logic paths. Database layer is the best-covered area.

---

## 5. Testing Strategy

### Current State Assessment

The project has the infrastructure for testing (`tests/` directory, `TestSQL` mock, CMake integration) but the test suite covers primarily the database abstraction layer. The core game simulation — entity lifecycle, component interactions, skill execution, mission progression, serialization — is largely untested.

### Critical Test Gaps (by risk)

| Gap | Risk Level | Recommended Test Type |
|-----|-----------|----------------------|
| Character XML round-trip (save → load) | CRITICAL | Integration test: serialize character, reload from XML, assert state equality |
| DestroyableComponent kill path null safety | HIGH | Unit test: kill entity with no Character* attached; assert no crash |
| MissionComponent state transitions | HIGH | Unit test: accept → progress → complete → claim reward state machine |
| InventoryComponent equip/unequip | HIGH | Unit test: equip item, verify component state; unequip, verify state restored |
| GameMessages handler parsing | MEDIUM | Fuzz test: malformed BitStream inputs; assert no crash or assertion failure |
| CDClient table loading | MEDIUM | Unit test: LoadValuesFromDatabase() with known-good and edge-case SQLite data |
| Behavior tree execution | MEDIUM | Unit test: execute HealBehavior, BasicAttackBehavior with mock BehaviorContext |
| Zone entity spawning | MEDIUM | Integration test: load a zone, assert expected entity count and component presence |

### Pre-Refactoring Regression Test Recommendations

Before undertaking any Phase 1 or Phase 2 refactoring, establish a **characterization test baseline**:

1. **EntityManager query tests** — Assert that `GetEntitiesByComponent(type)` and `GetEntitiesInGroup(group)` return the same results before and after the indexed-lookup refactoring. Use a zone with 100+ entities.

2. **InventoryComponent equip tests** — Cover the EquipItem code path for the magic LOT values (6416 for rockets, 8092 for cars) before refactoring to verify proximity check behavior is preserved.

3. **GameMessages handler smoke tests** — Before decomposing the monolith, write a test that calls each of the 101+ handlers with a minimal valid BitStream and asserts the entity state changes correctly. This forms the regression suite that validates the subsystem handler migration.

4. **Character XML round-trip** — Before any MissionComponent or InventoryComponent refactoring, assert that `UpdateXml()` → `LoadFromXml()` is an identity operation for a variety of character states.

### CI/CD Recommendations

From doc 02 and 09:

- **Add ASAN/UBSAN sanitizer preset** to `CMakePresets.json` and run on every PR in CI (1–2 days effort, catches memory safety issues before production)
- **Add coverage preset** (`--coverage` flags) and upload to a coverage service (Codecov or similar) to track progress toward coverage targets
- **Require tests for new components** — enforce via code review policy: any new `Component` subclass must have unit tests for `Update()`, `Serialize()`, `LoadFromXml()`, and `UpdateXml()`
- **Database migration test** — add a CI step that applies all migrations from `migrations/dlu/sqlite/` against an empty database and asserts the final schema is correct
- **Separate slow tests** — use CTest labels to separate fast unit tests (< 1 second) from integration tests requiring database setup; run fast tests on every commit, full suite on PR merge

---

## 6. Constraints & Open Questions

### What We Know Confidently

- The CDClient SQLite database (`resServer/CDServer.sqlite`) is the authoritative source for all game content definitions; it is read-only at runtime and fully loaded into memory at startup
- Items, missions, loot tables, and skills can be added purely through SQL inserts into the CDClient database — no C++ compilation required
- The component-based entity system (`Entity` + `Component` subclasses + `eReplicaComponentType`) is the correct extension point for new game mechanics
- The `migrations/` system handles schema evolution correctly for both MySQL and SQLite backends
- The codebase targets C++20 (GCC 12+) and is build-verified on Linux, Windows (MSVC), and macOS
- Character state persists as XML every 10 minutes (`WorldServer.cpp:490-497`) with no event-driven save; crashes between saves lose up to 10 minutes of progress

### What Requires Original LEGO Universe Assets to Verify

- **Zone files** (`.luz`/`.lvl`): Whether zone modifications are possible requires examining actual LUZ binary format; no documentation exists outside reverse-engineering community notes
- **FDB binary format**: The `FdbToSqlite.cpp` converter is the authoritative implementation of the FDB format, but verifying edge cases requires sample FDB files from the original client
- **NIF model format**: 3D model integration is client-side; server references model paths by string. The Blender MCP can author new NIFs (see doc 13 §2.2), but verifying that custom NIF files load correctly in the LU client still requires a running client. The NIF dialect caveat in doc 13 §2.2 also applies — LU's early NetImmerse variant may need a NifSkope post-step.
- **CDServer.sqlite schema completeness**: The analysis assumes the CDClient tables documented are complete; additional tables used by client features may not appear in server-side code

### What Requires a Running Game Client to Test

- Any in-game visual or physics change
- Verifying that new items appear correctly equipped on characters
- Testing mission dialogue rendering
- Confirming loot drop visual feedback
- Validating zone transitions and portal behavior
- Any animation or audio changes

### Team Skill Assumptions

This analysis assumes:
- Familiarity with C++17/20 (templates, smart pointers, ranges)
- Ability to compile and run DarkflameServer locally (requires original LEGO Universe client files)
- Access to a running MariaDB/MySQL instance or SQLite for development
- Familiarity with CMake build system
- No experience with the LEGO Universe proprietary binary formats (LUZ, LVL, FDB) is assumed; these are undocumented and require dedicated study

---

## 7. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| **Character XML corruption during serialization refactoring** | MEDIUM | CRITICAL | Back up charxml table before any serialization change; add version field to XML; write round-trip regression tests first |
| **Null pointer dereference in kill/death path** | HIGH (existing bug) | HIGH | Add null checks to `DestroyableComponent.cpp` character pointer dereferences (doc 07, C.3); fuzz-test kill path |
| **Async database introducing race conditions** | MEDIUM (new risk on adoption) | HIGH | Introduce DatabaseThread with explicit serialization guarantees; add thread-sanitizer CI step; migrate one table at a time with extensive testing |
| **GameMessages.cpp refactoring introducing silent message-routing bugs** | HIGH | HIGH | Write characterization tests for all 101 handlers before beginning decomposition; keep adapter wrappers during transition; add logging for unhandled messages |
| **SLikeNet upgrade breaking protocol compatibility with clients** | LOW | HIGH | SLikeNet is ~95% drop-in compatible; test on a private network with a real LEGO Universe client before deploying |
| **LOT ID collision when adding new content** | HIGH (operationally) | MEDIUM | Always query existence before insert; maintain a community LOT registry document; consider a LOT range reservation policy (e.g., 12000–12999 for custom content) |
| **Zone file format reverse-engineering effort wasted** | LOW | LOW | No current plans; file under "Future" not "Active"; only invest if community produces reproducible format documentation |
| **C++23 compiler support gaps on target deployment OS** | MEDIUM | LOW | Upgrade to GCC 13 in Dockerfile first; verify all features used are supported before setting `CMAKE_CXX_STANDARD 23`; keep C++20 fallback in CMakePresets.json |
| **ItemSet LIKE query causing equip lag at scale** | HIGH (active issue) | MEDIUM | Cache fix is `SMALL` effort; should be done in Phase 1; quantify by profiling equip time with 100+ item set entries |
| **EntityManager component scan causing frame spikes at high entity count** | HIGH (active issue) | HIGH | This is the top Phase 1 priority; implement indexed lookup before deploying to high-population servers |
| **10-minute character save interval causing data loss on crash** | HIGH | MEDIUM | Consider reducing save interval to 2–3 minutes; add `/force-save` automation hook on graceful disconnect; this is config-level change, not code |

---

## Appendix: Quick Reference

### Codebase Statistics
- Total source files (excluding thirdparty): **1,330**
- Largest single file: `GameMessages.cpp` at **6,445 lines** (CRITICAL quality issue)
- Component types defined: **45+** in `eReplicaComponentType` enum
- CDClient tables: **43 table classes** (100+ actual tables in SQLite)
- Database migration count: **24 migrations** (0–23)
- TODO/FIXME count in dGame/: **51**
- GetEntitiesByComponent() call sites: **29+**

### Most Important Files for Contributors
- `dGame/Entity.h` — Foundation of the entire game object model
- `dGame/EntityManager.h/cpp` — Entity lifecycle and lookup (performance critical)
- `dGame/dGameMessages/GameMessages.cpp` — All client↔server message handling (needs splitting)
- `dGame/dComponents/InventoryComponent.cpp` — Item equip/unequip (needs refactoring)
- `dScripts/CppScripts.h` — Base class for all scripted behaviors
- `dDatabase/CDClientDatabase/CDClientTables/` — All content definition loaders
- `dWorldServer/WorldServer.cpp` — Game loop and zone initialization entry point
- `migrations/dlu/sqlite/0_initial.sql` — Complete game database schema

### Content Addition Summary
| Type | Method | Effort | Code Required? | Claude+MCP Drives It End-to-End? |
|------|--------|--------|---------------|---------------------------------|
| Items | SQL INSERT to CDClient (+ NIF/DDS if custom) | Very Low | No | YES with SQLite + Blender + image-gen MCPs |
| Missions | SQL INSERT to CDClient | Low | No (unless custom logic) | YES with SQLite MCP (+ audio-gen for VO) |
| Loot tables | SQL INSERT to CDClient | Low | No | YES with SQLite MCP |
| Skills (simple) | SQL INSERT to CDClient | Low | No | YES with SQLite MCP |
| Skills (complex) | SQL + C++ Behavior subclass | Medium | Yes | YES — Claude writes the subclass; SQLite MCP wires the DB |
| NPCs | SQL INSERT + optional C++ script | Medium | Maybe | YES for dynamic spawn; NO for static placement (LUZ) |
| Zones (metadata only) | SQL INSERT to CDZoneTable | Low | Yes (zone script) | YES — metadata + script |
| Zones (new geometry / object placement) | Requires LUZ/LVL writer | N/A today | N/A | **NO** — no MCP exists; see doc 13 §3.1 and §5.7. Only unsolved content task. |
