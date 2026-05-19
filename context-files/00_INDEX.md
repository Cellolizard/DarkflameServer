# DarkflameServer — Complete Codebase Investigation

> Comprehensive analysis for optimization, refactoring, and content extension

## Quick Navigation

| # | Document | Purpose |
|---|----------|---------|
| 01 | [Repository Map](01_REPOSITORY_MAP.md) | Directory structure, entry points, file counts |
| 02 | [Technology Stack](02_TECHNOLOGY_STACK.md) | Languages, frameworks, all dependencies |
| 03 | [Code Organization](03_CODE_ORGANIZATION.md) | Module breakdown, design patterns |
| 04 | [Content Systems](04_CONTENT_SYSTEMS.md) | How game content is defined and loaded |
| 05 | [Gameplay Mechanics](05_GAMEPLAY_MECHANICS.md) | Game loop, ECS, networking, combat |
| 06 | [Data Persistence](06_DATA_PERSISTENCE.md) | Save/load, CDClient DB, file formats |
| 07 | [Optimization Opportunities](07_OPTIMIZATION_OPPORTUNITIES.md) | Performance improvements with locations |
| 08 | [Refactoring Strategy](08_REFACTORING_STRATEGY.md) | Architectural improvements |
| 09 | [Replatforming Analysis](09_REPLATFORMING_ANALYSIS.md) | Dependency/version upgrades |
| 10 | [Content Addition Guide](10_CONTENT_ADDITION_GUIDE.md) | Step-by-step: add items, missions, zones, etc. |
| 11 | [Content Dependencies](11_CONTENT_DEPENDENCIES.md) | How content types reference each other |
| 12 | [Development Workflow](12_DEVELOPMENT_WORKFLOW.md) | Practical tooling and workflows |
| 13 | [Claude vs MCP](13_CLAUDE_VS_MCP.md) | Claude-native capabilities, MCP-backed workflows (SQLite, Blender, image-gen, audio), and what still has no tool |
| 14 | [Synthesis & Recommendations](14_SYNTHESIS_AND_RECOMMENDATIONS.md) | Executive summary and roadmap |

---

## Key Statistics

| Metric | Value | Source |
|--------|-------|--------|
| Total source files (excl. thirdparty) | **1,330** (.cpp + .h) | Doc 01 |
| dGame/ (core game logic) | **341 files** | Doc 01, 03 |
| dScripts/ (C++ gameplay scripts) | **613 files** | Doc 01, 03 |
| dDatabase/ (DB abstraction layer) | **168 files** | Doc 01, 03 |
| dCommon/ (shared utilities) | **124 files** | Doc 01, 03 |
| Component types (eReplicaComponentType) | **45+** | Doc 03, 05 |
| CDClient table classes | **43** | Doc 03, 06 |
| Database migrations | **24** (0–23) | Doc 06 |
| Largest single file | `GameMessages.cpp` — **6,445 lines** | Doc 07, 08 |
| TODO/FIXME count in dGame/ | **51** | Doc 07 |
| GetEntitiesByComponent() call sites | **29+** (O(n) each) | Doc 07 |
| Third-party libraries (vendored) | **11** | Doc 01, 02 |
| Supported platforms | Linux, Windows (MSVC), macOS | Doc 01, 02 |
| C++ standard | **C++20** (C99 for thirdparty) | Doc 02, 09 |
| Server processes | 4 (Master, World, Chat, Auth) | Doc 05 |

---

## Top 3 Action Items

These are the three most impactful things to do first, drawn from the Phase 1 roadmap in Doc 14:

### 1. Add Component-Indexed Lookup to EntityManager
**File**: `dGame/EntityManager.h/cpp` (lines 295–340)  
**Effort**: 2–3 days  
**Why first**: `GetEntitiesByComponent()` is called 29+ times across the codebase, each performing O(n) full-zone scans. Adding `m_EntitiesByComponent` and `m_EntitiesByGroup` maps (maintained on component add/remove) converts all these to O(1) direct lookups. This is the single highest-impact change in the codebase — it affects hot paths in item equip, entity death, and property management on every game tick.

### 2. Upgrade RakNet to SLikeNet
**File**: `thirdparty/raknet/`, `CMakeLists.txt` include paths  
**Effort**: ~2 weeks  
**Why second**: RakNet 3.25 is unmaintained (last official release 2013), has known security vulnerabilities, and is a proprietary fork that cannot receive patches. SLikeNet is an actively maintained, ~95% drop-in compatible replacement by the original contributor. The migration requires only `#include` path changes. This is the most important risk-reduction action — security and stability without behavioral change.

### 3. Write Regression Tests Before Any Refactoring
**File**: `tests/` directory (new test files)  
**Effort**: 1–2 weeks  
**Why third**: The two largest planned refactors (splitting `GameMessages.cpp` and `InventoryComponent`) risk introducing silent regressions because there are currently no tests for these code paths. Before beginning either refactor, write characterization tests for the 20 most-called message handlers and the full `EquipItem()` code path (including the magic LOT 6416/8092 special cases). These tests become the regression suite that validates the refactoring is behavior-preserving.

---

## Architecture at a Glance

```
Client (LEGO Universe.exe)
    ↕ RakNet UDP (proprietary LEGO protocol)
    ↓
┌─────────────────────────────────────────┐
│             MasterServer                │
│  (instance management, auth routing)    │
└───┬─────────────┬──────────────────────┘
    │             │
    ↓             ↓
AuthServer    ChatServer
(login)       (social, teams)
                  │
                  ↓
         WorldServer (one per zone/instance)
         ┌────────────────────────┐
         │  Game Loop @ 60 FPS    │
         │  EntityManager         │
         │  Component Updates     │
         │  Physics (dpWorld)     │
         │  Packet Processing     │
         └────────────────────────┘
                  │
         ┌────────┴────────┐
         ↓                 ↓
   CDServer.sqlite    GameDatabase
   (read-only,        (MySQL/SQLite,
    game content)      player state)
```

---

## Content Addition Quick Reference

| Content Type | SQL Only? | Assets Needed | Code Required? | Claude+MCP End-to-End? | Difficulty |
|---|---|---|---|---|---|
| Items | YES | NIF/DDS optional | No | YES (SQLite + Blender + image-gen MCPs) | Low |
| Missions | YES | Audio optional | No (usually) | YES (SQLite + optional audio-gen MCP) | Low |
| Loot tables | YES | None | No | YES (SQLite MCP) | Low |
| Skills (simple) | YES | None | No | YES (SQLite MCP) | Low |
| NPCs (dynamic spawn) | YES | NIF/DDS optional | Maybe | YES (all asset MCPs apply) | Medium |
| NPCs (static placement) | — | LUZ edit | — | NO — needs LUZ writer | Not possible today |
| Skills (complex) | Partially | None | Yes (Behavior subclass) | YES (Claude writes the subclass) | Medium |
| Pets | YES | NIF + LXFML + DDS | Maybe | YES (Blender + image-gen + native LXFML) | Medium |
| Quick Builds | YES | NIF (broken + built) | Optional | YES (Blender MCP) | Medium |
| Zones (metadata + script) | YES | None | Yes (zone script) | YES | Hard |
| Zones (geometry / object placement) | NO | LUZ/LVL (no MCP) | N/A | **NO** — the only unsolved content task; see doc 13 §3.1 | Not possible today |

**Required reading for Claude-driven content work**: [Claude vs MCP](13_CLAUDE_VS_MCP.md) — see §7 for the MCP install order.

**Key entry point for SQL content edits**: `resServer/CDServer.sqlite`  
**Key entry point for C++ behavior**: `dScripts/CppScripts.h` (Script base class)  
**Key entry point for new components**: `dGame/dComponents/` + `dCommon/dEnums/eReplicaComponentType.h`

---

## Investigation Metadata

- **Repository**: DarkflameUniverse/DarkflameServer
- **Analysis Date**: 2026-04-03
- **Phases Completed**: All 6 (14 documents)
- **Investigator**: Claude AI Agent (claude-sonnet-4-6)
- **Scope**: Full codebase — read-only analysis, no modifications
- **Note**: Documents 10 and 11 (Content Addition Guide and Content Dependencies) are placeholders in the navigation table above; the substantive content for content addition workflows is in docs 12 and 13.
