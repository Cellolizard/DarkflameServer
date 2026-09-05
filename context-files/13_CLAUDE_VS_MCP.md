> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> **Needs Astra:** this file is bound to a Claude + MCP session (Blender, image-gen, SQLite MCP, “what Claude can do”). Do not follow it as an Astra/Codex/Grok-Build playbook.
>
> **Durable fact to keep:** there is still no LUZ/LVL writer; new zone geometry / static placement is blocked. CDClient SQL + C++ scripts + behaviors remain the content ceiling that is actually reachable. The rest of this file should be restated for the current tool stack or deleted.

# Claude Code vs External Tools: Capabilities & MCP Opportunities

This document defines exactly what Claude can accomplish for DarkflameServer development. It distinguishes three categories:

1. **What Claude can do natively** (in any session, with just file/shell tools).
2. **What Claude can do when an MCP server is connected** — most asset/database work falls here. The relevant MCP servers exist today; the constraint is "is this server connected to the current session" rather than "does this capability exist."
3. **What still requires a human or unsolved tooling** — currently limited to the LUZ/LVL zone formats and to anything requiring the closed-source LEGO Universe client.

> **Session caveat.** "Claude can do X over MCP" means: *given an MCP session with the relevant server connected*, Claude is the orchestrator. Whether a given chat instance can call Blender, SQLite, image-gen, etc. depends on which MCP servers the user has installed and connected. The plans below assume the user can connect the named MCP servers; if a server is not connected in a given session, the same task falls back to either Claude-generated artifacts the user runs externally or to manual work.

---

## SECTION 1: What Claude Can Do Natively (No MCP Needed)

These are tasks Claude handles with built-in tools (file edits, shell commands, web fetch). The confidence ratings reflect Claude's training and the nature of the task — not MCP availability.

### 1. SQL/FDB Content Generation — **HIGH**

Claude understands SQL syntax (SQLite dialect specifically), the CDClient schema, entity relationships, and population patterns. Given example rows and a design brief, Claude generates production-ready INSERT/UPDATE statements.

```sql
-- Item definition
INSERT INTO Objects (id, name, type, description, interactionDistance)
VALUES (12345, 'CustomSword', 'weapon', 'A powerful new sword', 5.0);

INSERT INTO ItemComponent (id, equipLocation, baseValue, rarity, itemType,
    isTwoHanded, SellMultiplier)
VALUES (12345, 'RightHand', 1000, 5, 2, 1, 1.5);

INSERT INTO ComponentsRegistry (id, component_type, component_id)
VALUES (12345, 2, 12345);

-- Mission definition
INSERT INTO Missions (id, defined_type, defined_subtype, offer_objectID,
    target_objectID, reward_currency, LegoScore, isMission, time_limit)
VALUES (10001, 'mission', 'storyline', 5000, 5001, 5000, 100, 1, 3600);

INSERT INTO MissionTasks (id, taskType, target, targetGroup, targetValue, uid)
VALUES (1000100, 1, 5001, 'Enemy', 5, 1);

-- Loot table
INSERT INTO LootMatrix (lootMatrixIndex, lootTableIndex, percentChance)
VALUES (6001, 2001, 75);

INSERT INTO LootTable (itemid, LootTableIndex, sortPriority)
VALUES (100, 2001, 1);
```

**Limitations without MCP**:
- Claude cannot see the existing database schema unless it's provided (or unless a SQLite MCP is connected — see Section 2.1).
- Claude cannot guarantee a fresh LOT ID without querying the DB.
- Claude cannot validate the values against in-game balance.

### 2. C++ Script Generation — **HIGH for boilerplate, MEDIUM for complex logic**

Claude generates script class structure, event handlers, component manipulation, mission/behavior state machines, and serialization patterns.

```cpp
#pragma once
#include "CppScripts.h"

class ZoneTrapScript : public CppScripts::Script {
private:
    uint32_t m_TrapEntityID;
    bool m_IsActivated;

public:
    void OnStartup(Entity* self) override {
        m_TrapEntityID = self->GetObjectID();
        m_IsActivated = false;
    }

    void OnCollisionPhantom(Entity* self, Entity* target) override {
        if (!m_IsActivated && target->IsPlayer()) {
            m_IsActivated = true;
            target->GetComponent<DestructibleComponent>()->Damage(
                25, self->GetObjectID(), true, false
            );
            Game::entityManager->ScheduleForDeletion(self->GetObjectID(), 5.0f);
        }
    }
};
```

Best when Claude has access to the repo (it does — via the file tools) so it can read existing component APIs, eRakNetMessageType enums, and example scripts in `dScripts/`.

### 3. Configuration Files — **HIGH**

INI, JSON, YAML, XML — Claude generates syntactically correct config files based on requirements. See `resServer/*.ini` for the project's INI dialect.

### 4. XML Data Files — **HIGH**

Mission dialogue XML, behavior parameter trees, and similar structured data are well within Claude's wheelhouse.

### 5. Documentation — **HIGH**

API documentation, architecture diagrams (Markdown / Mermaid / PlantUML), changelogs, content-addition guides. The draw.io MCP — see Section 2.5 — extends this to interactive diagrams.

### 6. Code Analysis & Refactoring — **HIGH**

Identify code smells, propose refactorings, generate behavior-preserving rewrites, suggest design patterns, analyze performance characteristics. This is what powered the rest of this research directory.

---

## SECTION 2: Tasks That Require an MCP Server

Each item here used to live in a "Claude can't do this" section. With the right MCP server connected, Claude is the orchestrator — it generates the script/recipe, the MCP runs it, results come back, and Claude verifies and iterates.

For each entry below: what the MCP does, the workflow, and **what the user has to do once** to connect the server.

### 2.1 SQLite MCP — Direct CDClient editing

**Server**: `mcp-server-sqlite` (Anthropic reference implementation) or any equivalent SQLite MCP.

**Capability**: Execute SQL directly against `resServer/CDServer.sqlite`. Claude can:
- Inspect schema (`.schema Objects`, `PRAGMA table_info(...)`).
- Query for ID collisions before inserting (`SELECT id FROM Objects WHERE id = ?`).
- Run multi-statement transactions to add an item + its components + a vendor entry atomically.
- Verify results with a follow-up SELECT.

**Workflow** — adding a new item end-to-end in a single conversation:

```
Claude → query schema for Objects, ItemComponent, ComponentsRegistry
Claude → query for next free LOT in the 12000-12999 range
Claude → BEGIN; INSERT three rows; COMMIT
Claude → SELECT to confirm
Claude → tell the user to restart the WorldServer
```

This closes the manual "generate SQL → user copies → user runs → user reports back" loop into a single iteration. **Highest leverage MCP for this project.**

**Setup**: install the MCP server, add a stanza to the user's MCP config pointing at `resServer/CDServer.sqlite` (read-write or read-only depending on workflow). One-time, ~10 minutes.

**FDB**: still no MCP for the binary FDB format, but it doesn't matter — DarkflameServer auto-converts FDB → SQLite at startup and the server only reads SQLite. There is no need to write FDB.

### 2.2 Blender MCP — 3D model creation (NIF)

**Server**: `blender-mcp` (commonly the Ahuja/Patel community server) or `BlenderMCP` — both expose Blender's Python API to Claude over MCP.

**Capability**: Claude writes Blender Python (`bpy`) scripts that the MCP executes inside a running Blender instance. Claude can:
- Create primitives, modify meshes, set up materials, position/rotate/scale, weight-paint, rig, UV-unwrap.
- Execute Blender's NIF exporter add-on (if installed) to produce `.nif` files for `res/meshes/`.
- Take a viewport screenshot back into the conversation for visual verification before export.
- Iterate: "make the blade ~30% longer," "thicken the cross-guard," "add a slight curve to the tip."

**Workflow** — adding a new sword model:

```
User: "make me a longsword for LOT 12345"
Claude → bpy script: create cylinder for hilt, scaled cube for blade, beveled
         cross-guard, parent and join, UV-unwrap, assign metal/leather materials
Claude → render preview, send screenshot back
Claude → iterate based on user feedback ("longer blade", "darker pommel")
Claude → run NIF exporter add-on → res/meshes/weapons/sword_custom_12345.nif
Claude → (with SQLite MCP) update RenderComponent.render_asset to point at the new path
```

**Setup, one-time**:
1. Install Blender 3.x or 4.x.
2. Install a NIF exporter add-on. The community options as of 2026 are the `blender_nif_plugin` fork for Bethesda-style NIFs (LEGO Universe uses a related but distinct NetImmerse variant — see "NIF caveat" below).
3. Install the Blender MCP server and point it at your Blender executable.

**NIF caveat** — read this before assuming MCP-driven NIF authoring works end-to-end. LEGO Universe's NIF dialect is an early/custom NetImmerse variant. The mainstream `blender_nif_plugin` targets Bethesda's NIF dialect; exported files may need a post-processing pass through NifSkope (also scriptable) or a community converter to match the LU format the client expects. Treat NIF export as "Claude can drive Blender to produce a NIF binary that's 90% of the way there; verifying it loads in the LU client may require a NifSkope round-trip." If the community-maintained NIF tooling for LU evolves, this caveat becomes obsolete.

**Server-side test**: DarkflameServer uses NIFs primarily for collision and render-asset references; it does not parse the full NIF binary on the server side (`06_DATA_PERSISTENCE.md` line 339: "Parser: Not fully implemented in DarkflameServer (mostly client-side)"). So you can land a custom NIF, register it in CDClient via SQLite MCP, and the server happily references the path — visual correctness gets verified in a running client.

### 2.3 Image-gen + texconv MCP — Texture creation (DDS)

**Servers**: an image-generation MCP (most users wire up either an OpenAI-image MCP or a local Stable Diffusion MCP) plus a small wrapper around `texconv` (Microsoft) or `compressonatorcli` / `nvtt` on non-Windows.

**Capability**: Claude designs a texture in prose ("weathered iron, blue tint, normal-mapped pitting"), the image-gen MCP produces a PNG/PSD, the texconv MCP converts to DDS with the correct compression format (typically DXT5/BC3 for diffuse + alpha, BC5 for normals), and the result lands in `res/textures/`.

**Workflow** — adding a custom item texture:

```
Claude → image-gen MCP: "1024x1024 PBR-style diffuse, weathered steel sword
         blade, blue chromatic accent, seamless on Y axis"
Claude → receive PNG
Claude → texconv MCP: convert to DDS (BC3, mipmaps generated, sRGB flag set
         for diffuse)
Claude → place at res/textures/weapons/sword_blue_12345.dds
Claude → (with SQLite MCP) update the material reference
```

**Setup, one-time**: image-gen MCP API key (OpenAI/Anthropic/Stability) + install texconv (Windows) or an equivalent on macOS/Linux (`compressonatorcli` works cross-platform).

**Practical scope**: this works well for diffuse, normal, and roughness textures on items, props, and simple environment surfaces. It does **not** replace a 3D-experienced texture artist for character skins or anything requiring tight stylistic consistency with original LU art — but it's perfectly viable for filling out item variety, vendor stock, and prop dressings.

### 2.4 Audio-gen MCP — Sound and music (.ogg)

**Servers**: TTS MCPs (ElevenLabs MCP, OpenAI TTS MCP) for voice; music-gen MCPs (Suno-style MCPs or local AudioCraft wrappers) for ambient/sting music; plus a small `ffmpeg`/`oggenc` wrapper for format conversion.

**Capability**: Claude writes prompts/scripts; the MCP produces audio; ffmpeg converts to OGG Vorbis at the bitrate/sample rate the LU client expects (44.1 kHz stereo for music, mono for one-shots typical). Output lands in `res/sounds/`.

**Use cases this unlocks**:
- NPC dialogue voice lines for custom missions.
- Generic SFX (UI clicks, item pickup chimes, ambient zone audio) for new content.
- Stinger music for custom activities.

**Limits**: matching the existing LU audio aesthetic (heavily Lego-themed, kid-friendly, often comedic) requires careful prompting; expect manual review.

**Setup**: provider API key + ffmpeg installed.

### 2.5 draw.io MCP — Architecture and diagram authoring

**Server**: `claude.ai draw_io` (already available in this session). Creates interactive draw.io diagrams from Mermaid or draw.io XML.

**Capability**: Diagram the architecture sections of these research docs, generate sequence diagrams for the entity-component lifecycle, draw the GameMessages.cpp dependency graph as part of the refactoring proposal in doc 08, etc. Useful for communicating proposed refactors to other contributors.

### 2.6 GitHub / git MCP — PR and issue workflows

**Server**: GitHub MCP (official) or `gh` CLI via shell.

**Capability**: file issues for the 51 TODO/FIXME items mentioned in doc 14, open draft PRs for the Phase-1 quick wins, query the issue tracker for "is anyone working on X." Useful for the operational side of executing the Phase-1 plan from doc 14 — not for content creation directly.

---

## SECTION 3: What Is Still Not Feasible

### 3.1 LUZ / LVL zone files

**Status as of 2026**: no MCP-friendly toolchain exists. LUZ (zone layout, spawner placement, paths) and LVL (level/collision/environment data) are proprietary LEGO Universe binary formats.

**What's true**:
- The `dZoneManager/` code in this repo **reads** LUZ and LVL — that parser is the most complete open-source reference for these formats (see `06_DATA_PERSISTENCE.md:268-326`).
- No project has produced a writer/editor with feature parity. The reverse direction (write LVL chunks: FileInfo, SceneEnvironment, SceneObjectData, SceneParticleData) is what would need to be built.

**Could this become an MCP?** Plausibly — and this is the single most impactful future MCP for DarkflameServer specifically. Estimated effort: 80-150 hours to write a Python or C++ LUZ/LVL writer using the existing reader as a spec, then wrap it as an MCP. Anyone working on this would be doing reverse engineering against the parser in `dZoneManager/Zone.cpp` and `Level.cpp`.

**Workarounds today** (no zone editor needed):
- Spawn objects dynamically at runtime via C++ scripts in `dScripts/`.
- Reuse existing zones with new mission/NPC/loot overlays.
- Modify NPC presence by editing `MissionNPCComponent` or `CDObjectsTable` entries — zone metadata in `CDZoneTable` is editable too.

### 3.2 LEGO Universe client testing

The LU client is closed-source. There is no public API for headless testing, screenshot diffing, or automation. Any "did this look right in-game" check still requires a human running the client.

A future MCP that drove the client via input emulation + screen-scraping would be feasible but high-effort (50+ hours) and brittle.

### 3.3 LXFML brick puzzle files (taming minigame)

Used in pet-taming minigames (see `10_CONTENT_ADDITION_GUIDE.md` section 6). LXFML is LEGO Digital Designer's XML format. No MCP exists, but it is **XML** — Claude can hand-author it natively if you can describe the brick set, and the format is documented enough that a thin MCP wrapper around an LXFML validator would be tractable. Treat as "natively doable with effort" rather than "needs MCP."

---

## SECTION 4: Recommendation Matrix

| Content Type | Step | Native | MCP Required? | Which MCP | Notes |
|---|---|---|---|---|---|
| **Item** | Define schema | YES | — | — | SQL generation |
| **Item** | Create in CDClient | partial | recommended | SQLite | Native = generate SQL; MCP = execute & verify |
| **Item** | Create 3D model | NO | YES | Blender | NIF export, see §2.2 caveat |
| **Item** | Create texture | NO | YES | image-gen + texconv | DDS pipeline |
| **Item** | Test in-game | NO | — | — | Still needs a human + client |
| **Mission** | Define tasks | YES | — | — | SQL structure |
| **Mission** | Create in CDClient | partial | recommended | SQLite | Pure database |
| **Mission** | Custom logic | YES | — | — | Generate C++ script |
| **Mission** | Voice lines | NO | YES | audio-gen | TTS to OGG |
| **NPC** | Define object | partial | recommended | SQLite | SQL across 4-7 tables |
| **NPC** | Offer mission | partial | recommended | SQLite | Link via CDMissionNPCComponent |
| **NPC** | Custom model | NO | YES | Blender | 3D asset |
| **NPC** | Spawn in zone (dynamic) | YES | — | — | C++ script or `/spawn` |
| **NPC** | Place statically | NO | NO (still infeasible) | — | Requires LUZ editor; none exists |
| **Skill** | Simple skill | partial | recommended | SQLite | DB-driven |
| **Skill** | Complex behavior | YES | — | — | C++ Behavior subclass |
| **Loot** | Design table | YES | — | — | SQL planning |
| **Loot** | Create in CDClient | partial | recommended | SQLite | Multi-table inserts |
| **Zone** | Define metadata | partial | recommended | SQLite | CDZoneTable entry only |
| **Zone** | Object placement | NO | NO MCP exists | — | LUZ writer is unsolved |
| **Zone** | Custom script | YES | — | — | C++ zone controller |
| **Zone** | Physics/collision | NO | NO MCP exists | — | LVL writer is unsolved |
| **Pet** | Definition | partial | recommended | SQLite | PetComponent + TamingBuildPuzzle |
| **Pet** | Brick puzzle (LXFML) | YES | — | — | XML — Claude can author |
| **Pet** | Pet model | NO | YES | Blender | NIF + animations |
| **Quick Build** | DB entry | partial | recommended | SQLite | Standard component pattern |
| **Quick Build** | Models (broken + built) | NO | YES | Blender | Two NIFs |

"Partial" = Claude can generate the artifact; the MCP closes the loop by executing and verifying.

---

## SECTION 5: Highest-Leverage MCP Connections

Ranked by impact per setup effort for this specific project, assuming the MCP servers exist and the user is willing to install them.

### 1. SQLite MCP — **HIGHEST**

**Why first**: CDClient editing is the bottleneck for ~80% of content work. Without SQLite MCP, every content task is a copy-paste-restart-verify loop with the user. With it, Claude does the entire INSERT/verify cycle inline.

**Effort**: ~10 minutes to install + configure. Use any maintained SQLite MCP server.

**Risk**: low. Operate against a copy of `CDServer.sqlite` during early sessions; the FDB→SQLite converter regenerates the canonical SQLite from the original FDB on server startup, so mistakes are recoverable.

### 2. Blender MCP — **HIGH** (with NIF caveat)

**Why second**: unlocks the entire 3D-asset half of content creation. Items, NPCs, props, quick-build models all become Claude-driven.

**Effort**: ~30-60 minutes: install Blender, install the NIF exporter add-on, install + configure the Blender MCP. Read the NIF caveat in §2.2 before assuming end-to-end NIF authoring is solved.

**Risk**: medium. NIF dialect mismatch may require a NifSkope post-step until the LU-specific NIF tooling matures. The Blender side itself is solid and battle-tested.

### 3. Image-gen + texconv MCP — **HIGH**

**Why third**: pairs with Blender. A textured model is dramatically more usable than a flat-shaded one. Standalone use (re-skinning existing items with new diffuse maps) is also high-value.

**Effort**: ~20 minutes setup if the image-gen provider is already in your stack.

**Risk**: low for diffuse/normal/roughness; medium for matching LU's specific art direction without prompt engineering.

### 4. Audio-gen MCP — **MEDIUM**

**Why fourth**: nice-to-have, not load-bearing. Voice lines for custom missions and SFX for new objects are real wins, but most content can ship reusing existing audio.

**Effort**: ~20 minutes setup + provider key.

### 5. draw.io MCP — **MEDIUM** (already connected in this session)

**Why**: helps communicate refactor proposals from docs 07/08/14 to other contributors. Not on the content-creation critical path.

### 6. GitHub MCP — **LOW-MEDIUM**

**Why**: improves the *operational* side of the Phase-1 plan (issue tracking, PR creation) without affecting any content creation pipeline. `gh` CLI via shell already covers most of this.

### 7. LUZ/LVL Writer MCP — **VERY HIGH if built**

**Why**: the only thing standing between this project and full new-zone capability. Doesn't exist yet. Building it is a community-scale project (80-150 hours) that uses the existing parser in `dZoneManager/` as a spec.

---

## SECTION 6: Workflow Examples

The point of this section is to make concrete what "Claude + MCP" looks like as a single coherent workflow.

### Workflow A: "Add a new sword end-to-end"

Assuming SQLite, Blender, and image-gen+texconv MCPs are connected:

```
User: "Create a Frostbite Longsword, level 30, ice damage,
       drops from Frostlords"

Claude (SQLite MCP):
  - SELECT max(id) FROM Objects WHERE id BETWEEN 12000 AND 12999
  - Reserve LOT 12042
  - SELECT existing ice-element items to match damage/value scaling

Claude (Blender MCP):
  - bpy script: longsword silhouette, slight curve on tip,
    cross-guard with ice motif, jeweled pommel
  - Render preview, send screenshot

User: "blade should be slimmer, less fantasy more clean"

Claude (Blender MCP):
  - Adjust geometry, re-render, send screenshot
  - On approval, run NIF exporter → res/meshes/weapons/sword_frostbite_12042.nif

Claude (image-gen + texconv MCP):
  - Generate 1024² diffuse: clean steel blade, frosted blue accents
  - Generate matching normal map
  - texconv → res/textures/weapons/sword_frostbite_12042.dds

Claude (SQLite MCP):
  - BEGIN
  - INSERT Objects, ItemComponent, ComponentsRegistry, RenderComponent
  - INSERT LootMatrix entry linking to Frostlord destructible
  - COMMIT
  - SELECT to confirm

Claude → "Restart WorldServer and /spawn 12042 to test."
```

Time end-to-end: ~10 minutes interactive, vs hours of manual context switching previously.

### Workflow B: "Add a new mission with voice-acted NPC"

Assuming SQLite + audio-gen MCPs are connected:

```
Claude (SQLite MCP):
  - Reserve mission ID 10042 (range check via SELECT)
  - INSERT into Missions, MissionTasks (3 tasks), MissionNPCComponent
  - Link to existing NPC LOT 5034 (or create a new one with NPC workflow)

Claude (audio-gen MCP, TTS):
  - Generate offer voice line: "Adventurer! Frostlords have besieged..."
  - Generate completion line: "You've saved us all..."
  - ffmpeg → 44.1kHz mono OGG → res/sounds/missions/10042_*.ogg

Claude (SQLite MCP):
  - INSERT audio event references into mission dialogue table
```

### Workflow C: "Add a new zone" — partial only

```
Claude (SQLite MCP):
  - INSERT CDZoneTable entry with new zoneID

Claude (file edit):
  - Author a zone script in dScripts/zone/MyZone.cpp
  - Register in CppScripts dispatch

User: "But how do I get objects placed in the zone?"

Claude: "The LUZ/LVL writer is the unsolved gap. Either:
        a) Reuse an existing zone's LUZ and dynamically spawn from script, or
        b) Wait for a community LUZ writer MCP (~80-150h to build)."
```

This is the only major content workflow where MCP doesn't cover the critical step.

---

## SECTION 7: Setup Roadmap for a New Contributor

If you are setting up Claude-assisted DarkflameServer development today, install MCPs in this order. Stop at any tier; later tiers only matter if you're doing the relevant content work.

### Tier 1 — Required (any content work)

1. **SQLite MCP** pointed at `resServer/CDServer.sqlite`. ~10 min.

### Tier 2 — Strongly recommended (any asset work)

2. **Blender MCP** with NIF exporter add-on. ~30-60 min, plus the NIF caveat in §2.2.
3. **Image-gen MCP** (your preferred provider) + **texconv** wrapper. ~20 min.

### Tier 3 — Optional (specialized content)

4. **Audio-gen MCP** for voice and SFX. ~20 min.
5. **draw.io MCP** for diagrams in docs/refactor proposals. Already available in some Claude environments.

### Tier 4 — Operations

6. **GitHub MCP** or `gh` CLI for PR/issue workflows.

### Not yet available

7. **LUZ/LVL Writer MCP** — does not exist. Building it would unlock new-zone creation; effort estimate 80-150 hours against `dZoneManager/` as a spec.

---

## CONCLUSION

**What changed since the original analysis:**

The 2026-04 version of this document treated 3D-modeling, texture creation, audio, and direct DB access as "external tools Claude cannot drive." That framing is now obsolete for everything except LUZ/LVL zone files. With the right MCP servers connected:

- Items, missions, NPCs, loot, skills, pets, and quick-builds can be added end-to-end inside a Claude session.
- 3D models, textures, and audio for those entities can be generated and committed in the same session.
- The only routine content task that still requires manual tooling is **static object placement in zones** — gated on a LUZ/LVL writer that nobody has built yet.

**What still needs a human:**

- Visual verification in a running LU client (no client automation MCP exists).
- Stylistic art direction calls — Claude can produce on-spec assets, but matching the original LU look reliably requires a human reviewer.
- LUZ/LVL zone authoring, until a writer MCP is built.

**Immediate action for new content contributors**: connect the SQLite MCP first. It alone closes ~80% of the content-creation friction. Add Blender and image-gen MCPs when you start needing custom assets.

**Cross-references**:
- `12_DEVELOPMENT_WORKFLOW.md` — practical step-by-step workflows, updated to assume MCP availability.
- `10_CONTENT_ADDITION_GUIDE.md` — per-content-type recipes; asset-creation steps now point here.
- `14_SYNTHESIS_AND_RECOMMENDATIONS.md` §4Q7 — MCP-ranked recommendation roll-up.
