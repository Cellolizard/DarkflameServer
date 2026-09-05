> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> File:line citations and toolchain notes are frozen at analysis time (committed `aa2de8e8`, 2026-05-19).
>
> **Rot in this file:** This tip builds as **C++23** with gcc:13 Dockerfile and extra ASan/coverage presets; `main` is still C++20 / gcc:12. Test tree on this tip is 33 `.cpp` files, not the April set. README CMake 3.25–3.31 vs CMake 4 workaround in `CMakeLists.txt` is a live CI/docs mismatch. Operator compose still pulls upstream GHCR.

# DarkflameServer Development Workflow: Adding Game Content

This document provides practical, step-by-step workflows for adding different content types to DarkflameServer. It assumes a development environment is set up with CMake, a C++ compiler, and access to the CDClient and game databases.

---

## PART 1: FDB File Format & CDClient Database

### The FDB Binary Format

**FDB (LEGO Universe Binary Database)** is a custom binary format that wraps SQLite-like data structures. Located in the original game client, it must be converted to SQLite for use in DarkflameServer.

**Binary Format Structure** (from `dCommon/FdbToSqlite.cpp`):

```
FDB File Layout:
├─ Header
│  └─ int32_t: Number of tables
├─ Tables (repeating)
│  ├─ Table Pointer (int32_t offset)
│  ├─ Column Header
│  │  ├─ int32_t: Number of columns
│  │  ├─ String: Table name
│  │  └─ Columns (repeating)
│  │     ├─ int32_t: Column data type (eSqliteDataType enum)
│  │     └─ String: Column name (Latin-1 encoded)
│  └─ Row Data
│     ├─ int32_t: Number of allocated rows (power of 2)
│     └─ Rows (repeating)
│        ├─ int32_t: Row pointer (or -1 if empty)
│        └─ Row Info
│           ├─ int32_t: Number of columns
│           └─ Values (repeating)
│              ├─ int32_t: Data type
│              └─ Value data (type-specific)
```

**Supported Data Types** (enum `eSqliteDataType`):
- `NONE` (0) - NULL value
- `INT32` (1) - 32-bit integer
- `REAL` (2) - Float
- `TEXT_4` (3) - 4-byte string reference
- `INT_BOOL` (4) - Boolean as int
- `INT64` (5) - 64-bit integer
- `TEXT_8` (6) - 8-byte string reference

**Encoding Details**:
- All strings are **Latin-1 encoded** in FDB, converted to UTF-8 on read (see `FdbToSqlite.cpp:75`)
- Pointers are absolute offsets within the file
- Rows are allocated in power-of-2 blocks for efficiency
- Multiple row segments can be linked (each row can have a "next" pointer)

### FDB to SQLite Conversion

**Conversion Process** (in `dCommon/FdbToSqlite.cpp`):

DarkflameServer includes a built-in converter that:
1. Reads the FDB binary format using `FdbToSqlite::Convert`
2. Creates SQLite tables with matching schema
3. Inserts all row data as SQL INSERT statements
4. Writes to `CDServer.sqlite` in the resource directory

**Key Method**: `FdbToSqlite::Convert::ConvertDatabase(AssetStream& buffer)`

```cpp
// Called at WorldServer startup (WorldServer.cpp:185-197)
FdbToSqlite::Convert converter(binaryPath);
converter.ConvertDatabase(cdClientBuffer);
// Output: resServer/CDServer.sqlite
```

**Practical Workflow for Editing CDClient**:

1. **Get the FDB file** from the original LEGO Universe client (`CDClient.fdb`)
2. **Run DarkflameServer** — it automatically converts FDB to `CDServer.sqlite`
3. **Edit the SQLite database** using one of:
   - **SQLite MCP (recommended for Claude-driven sessions)** — Claude executes SQL directly and verifies with SELECTs; see `13_CLAUDE_VS_MCP.md` §2.1
   - **sqlite3 CLI**: `sqlite3 resServer/CDServer.sqlite`
   - **GUI editors**: DB Browser for SQLite, DBeaver, or VS Code extension
4. **Restart the server** to reload the updated CDClient
   - The server reads from SQLite, not FDB
   - No reverse conversion needed (server doesn't write to CDClient)

**Community Tools & References**:

While the project doesn't distribute external editors, the community uses:
- **DB Browser for SQLite** (free, cross-platform): Direct SQLite editing with GUI
- **lcdr/lego-universe-fdb-tools** (community project, not included): FDB conversion utilities (if you need to convert FDB independently)
- **SQLite command-line tools**: Included with most Linux distros, available for all platforms

The built-in converter (`FdbToSqlite.cpp`) is the authoritative tool for this project.

---

## PART 2: Resource Directory Structure

### Directory Layout

```
DarkflameServer/
├── resServer/                    # All server resources
│   ├── CDServer.sqlite           # CDClient database (auto-generated from FDB)
│   ├── navmeshes.zip            # Pre-built navigation meshes for all zones
│   ├── worldconfig.ini          # World server configuration
│   ├── sharedconfig.ini         # Shared configuration
│   └── (other .ini files)
└── (client files in separate folder, referenced by config)
    └── res/                     # Game resources from original client
        ├── meshes/              # NIF 3D models
        │   └── (zone-specific folders)
        │       └── *.nif        # NetImmerse/Gamebryo format models
        ├── textures/            # DDS texture files
        │   └── (zone-specific folders)
        │       └── *.dds
        ├── zones/               # Zone definition files
        │   ├── zone_1/          # Zone ID 1
        │   │   ├── zone_1.luz   # Zone layout (binary proprietary format)
        │   │   ├── zone_1.lvl   # Level data (binary proprietary format)
        │   │   └── zone_1_assets/ # Zone-specific assets
        │   └── (other zones)
        ├── audio/               # Audio files
        │   └── (zone-specific or global)
        └── locale/              # Localized text files
```

### File Format Details

| Format | Purpose | Authored Via | Claude+MCP Path |
|--------|---------|--------------|-----------------|
| `.nif` | 3D model (NetImmerse/Gamebryo) | Blender + NIF exporter | **Blender MCP** — Claude writes `bpy` scripts, MCP executes & exports. See `13_CLAUDE_VS_MCP.md` §2.2 (NIF dialect caveat applies) |
| `.dds` | Texture (DirectDraw Surface) | Image-gen → texconv | **image-gen + texconv MCP** — Claude prompts the image gen, the wrapper converts to BC3/BC5. See `13_CLAUDE_VS_MCP.md` §2.3 |
| `.luz` | Zone layout/object placement | (no editor exists) | None — see `13_CLAUDE_VS_MCP.md` §3.1; spawn dynamically from C++ scripts as workaround |
| `.lvl` | Level collision/physics | (no editor exists) | None — same as `.luz` |
| `.sqlite` | CDClient game data | `sqlite3` or **SQLite MCP** | **SQLite MCP** — Claude runs SQL directly. See `13_CLAUDE_VS_MCP.md` §2.1 |
| `.ogg` | Audio (Vorbis) | TTS / audio-gen → ffmpeg | **audio-gen MCP** — see `13_CLAUDE_VS_MCP.md` §2.4 |
| `.lxfml` | LEGO Digital Designer brick puzzle XML | hand-authored | Native — Claude writes the XML directly |

### Asset Path Resolution

**How the server finds assets**:

1. **Config specifies client location** (`resources/sharedconfig.ini`):
   ```ini
   client_location=../LEGO Universe/
   ```

2. **AssetManager resolves paths** (`dCommon/dClient/AssetManager.cpp`):
   - Searches in configured client folder
   - Loads from packed asset files (.zip) or loose files
   - Caches loaded assets in memory

3. **Zone loading** (`dZoneManager/Zone.cpp`):
   - Reads `.luz` file for object placement (positions, rotations, LOTs)
   - Loads `.lvl` for physics collision
   - Loads NIF models by LOT reference
   - Applies textures via DDS references

**Key Point**: You typically don't need to copy or manage resource files—they're referenced by the server and loaded from the original client installation.

---

## PART 3: Per-Content-Type Workflow

### A. ITEMS

#### Files to Modify

**Required**:
- `CDObjects` table - Object LOT and metadata
- `CDComponentsRegistry` table - Maps LOT to component IDs
- `CDItemComponent` table - Item properties (damage, rarity, etc.)

**Optional** (depending on item type):
- `CDInventoryComponent` - If it's a container
- `CDItemSets` / `CDItemSetSkills` - If part of a set
- `CDLootTable` / `CDLootMatrix` - If obtainable as loot

#### FDB Editing Process

1. Open the SQLite database: `sqlite3 resServer/CDServer.sqlite`
2. Generate IDs for your new LOT (typically use gaps in existing LOT ranges)
3. Execute INSERT statements (see examples below)
4. Restart server—it auto-reloads the CDClient

**Can Claude generate the SQL?** **YES, HIGH confidence.**

Example SQL for adding a new sword:

```sql
-- 1. Create object entry (LOT 12345 = new sword)
INSERT INTO Objects (id, name, type, description, nametag) 
VALUES (12345, 'NewAwesomeSword', 'weapon', '', 0);

-- 2. Create item component (component ID 12345, same as LOT for simplicity)
INSERT INTO ItemComponent (id, equipLocation, baseValue, isKitPiece, rarity, itemType, itemInfo, 
    inLootTable, inVendor, isUnique, isBOP, isBOE, reqFlagID, stackSize, color1, decal, 
    equipEffects, isTwoHanded, commendationLOT, commendationCost, forgeType, SellMultiplier)
VALUES (12345, 'RightHand', 500, 0, 5, 2, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1.0);

-- 3. Register component (component ID = component type ID + LOT)
INSERT INTO ComponentsRegistry (id, component_type, component_id)
VALUES (12345, 2, 12345);  -- component_type 2 = ItemComponent
```

#### Assets Needed

**Reusable**: Yes, items can share existing NIF models and DDS textures.

**Path Examples**:
- Model: `res/meshes/weapons/sword_generic.nif`
- Texture: `res/textures/weapons/sword_blue.dds`

When creating an item, you reference existing LOTs if possible. To use a custom model, the Claude+MCP path is:
1. With **Blender MCP** connected, ask Claude to script the mesh in Blender and run the NIF exporter — output lands in `res/meshes/zone_X/` or the global meshes folder. (NIF dialect caveat in `13_CLAUDE_VS_MCP.md` §2.2: LU's NetImmerse variant may need a NifSkope post-step.)
2. With **image-gen + texconv MCP** connected, generate and place the diffuse/normal DDS in `res/textures/`.
3. With **SQLite MCP** connected, Claude updates the Object/RenderComponent entry to reference the new model.
4. If those MCPs aren't connected in your session, the same steps fall back to manual Blender + texconv work; Claude can still generate the `bpy` script and the SQL for you to run.

#### Code Changes Required

**Minimal** (for simple items): None. Pure CDClient SQL is sufficient.

**Complex items** (e.g., rockets with configurable modules):
- May need C++ code to handle custom item behavior
- Rocket behavior is handled in `dGame/dInventory/Item.cpp` (configuration parsing)
- Pet items require `PetComponent` entry

#### Complete Workflow Checklist

1. **Design**: Decide LOT ID (use range 12000-12999 for custom items)
2. **Create CDObjects entry** with LOT ID and name
3. **Create ItemComponent entry** with properties (baseValue, rarity, equipLocation, etc.)
4. **Register component** in ComponentsRegistry
5. **Add to loot/vendor** (optional): Insert into CDLootTable or update CDVendorComponent
6. **Test in-game**: Use `/spawn 12345` (GM level 8) to spawn item
7. **Verify**: Equip, check properties, confirm in inventory

#### Test Verification

In-game as GM (gmlevel 8):
```
/spawn 12345          # Spawn the item
/gmadditem 12345 1    # Add to inventory
/pos                  # Print your position
/testmap 1100 0 0     # Go to starter zone
```

Then visually verify the item appears, equips correctly, shows proper stats.

---

### B. MISSIONS

#### Files to Modify

**Required**:
- `CDMissions` - Mission metadata
- `CDMissionTasks` - Individual tasks
- `CDMissionNPCComponent` - NPC offering/accepting missions
- `CDObjects` - NPC LOT (if new NPC)
- `CDMissionEmailTable` - Optional email sent on mission start

**Optional**:
- `CDLootMatrix` / `CDLootTable` - If rewards include randomized loot
- `CDRewards` - Additional reward configuration

#### FDB Editing

```sql
-- 1. Create mission (ID 10001 = custom quest)
INSERT INTO Missions (id, defined_type, defined_subtype, UISortOrder, offer_objectID, 
    target_objectID, reward_currency, LegoScore, reward_reputation, isChoiceReward, 
    reward_item1, reward_item1_count, isMission, time_limit, prereqMissionID, cooldownTime)
VALUES (10001, 'mission', 'storyline', 100, 5000, 5001, 1000, 50, 0, 0, 2000, 1, 1, 0, '', 0);

-- 2. Create task 1 (format: mission_id * 100 + task_num = 1000100)
INSERT INTO MissionTasks (id, taskType, target, targetGroup, targetValue, taskParam1, uid)
VALUES (1000100, 1, 5001, 'Enemy', 3, '', 1);  -- Kill 3 of object 5001

-- 3. Task 2 (1000101)
INSERT INTO MissionTasks (id, taskType, target, targetGroup, targetValue, taskParam1, uid)
VALUES (1000101, 2, 6000, 'Obj', 1, '', 2);   -- Interact with object 6000

-- 4. Link NPC as mission offerer
INSERT INTO MissionNPCComponent (id, missionID, offersMission, acceptsMission)
VALUES (5000, 10001, 1, 0);  -- NPC 5000 offers mission 10001
```

**Mission Task Types**:
- `1` - Defeat X enemies
- `2` - Collect X items
- `3` - Deliver X items
- `4` - Use skill X times
- `5` - Visit location
- `6` - Interact with object
- `7` - Complete activity
- `8` - Earn currency
- `9` - Build/craft

#### Assets Needed

**None** (unless custom NPC visuals). Mission logic is purely database-driven.

#### Code Changes Required

**None** for standard missions. C++ script only needed if:
- Mission has custom scripted behavior (special cutscene, conditional logic)
- Would create as `dScripts/SomeQuest.cpp` inheriting from `CppScripts::Script`

#### Can Claude Generate SQL?

**YES, MEDIUM confidence.** Claude can generate structure, but requires knowledge of:
- Valid taskType values
- Existing LOT IDs to reference
- Prerequisite mission chains

#### Complete Workflow Checklist

1. **Design mission**: Objective, NPC, tasks, rewards
2. **Assign mission ID** (use 10000+ for custom)
3. **Create Missions entry** with metadata
4. **Create tasks** in MissionTasks (one row per task)
5. **Link NPC** in MissionNPCComponent
6. **Set rewards** (items, currency, reputation)
7. **Test**: Use `/addmission 10001` (GM level 8)
8. **Verify**: Check mission appears, tasks update, rewards grant

---

### C. NPCs (New NPC in Existing Zone)

#### Files to Modify

**Required**:
- `CDObjects` - NPC object definition
- `CDComponentsRegistry` - NPC components
- `CDMovementAIComponent` - NPC AI movement (optional)
- `CDMissionNPCComponent` - If offering/accepting missions (optional)
- `CDVendorComponent` - If selling items (optional)
- Zone `.luz` file - **CANNOT BE EDITED** (proprietary binary format)

#### Workaround for Zone Placement

**LUZ File Problem**: The `.luz` zone file specifies which objects are placed where. It's a proprietary binary format that cannot be edited without the original LEGO Universe tools (unavailable).

**Solutions**:
1. **Use an existing NPC LOT** - Replace the mission/vendor of an existing NPC
2. **Spawn dynamically** - Use a C++ script or `/spawn` command to create NPC at runtime
3. **Wait for community tools** - Future zone editors may emerge (not available as of 2025)

**For Development**: You can spawn NPCs via `/spawn <lot>` and test their behavior without modifying `.luz`.

#### NPC Definition Example

```sql
-- Create NPC object
INSERT INTO Objects (id, name, type, description, interactionDistance)
VALUES (5000, 'CustomNPC', 'npc', 'A new NPC', 20.0);

-- Create NPC component (typically component_id = LOT)
INSERT INTO ComponentsRegistry (id, component_type, component_id)
VALUES (5000, 9, 5000);  -- component_type 9 = NPCComponent
```

#### Assets Needed

**NIF model** (optional): For custom NPC appearance, you'd need a model in `res/meshes/npc_custom.nif`.

Most NPCs reuse existing models. The Claude+MCP path for custom NPC appearance:
1. **Blender MCP** drives Blender to model, rig, and export the NIF.
2. **image-gen + texconv MCP** produces the DDS texture set.
3. **SQLite MCP** updates the `Objects` / `RenderComponent` rows to reference the new asset.

See `13_CLAUDE_VS_MCP.md` §2.2-2.3 for the full workflow and the NIF dialect caveat.

#### Code Changes Required

**For dynamic spawning**: Optional C++ script to spawn NPC at specific location/behavior.

Example (dScripts/CustomNPCSpawner.cpp):

```cpp
class CustomNPCSpawner : public CppScripts::Script {
public:
    void OnStartup(Entity* self) override {
        // Spawn NPC at position
        Entity* npc = Game::entityManager->CreateEntity(5000, 0, "MyZone");
        npc->SetPos({100.0f, 50.0f, 100.0f});
        npc->Initialize();
    }
};
```

#### Complete Workflow Checklist

1. **Design NPC**: Appearance, dialogue, behavior
2. **Create Objects entry**
3. **Create components** (Movement AI, Vendor, MissionNPC, etc.)
4. **Placement options**:
   - Option A: Use `/spawn <lot>` for testing/development
   - Option B: Create spawner script in zone
   - Option C: Wait for LUZ editor tools
5. **Test**: Verify NPC appears, interacts, offers/accepts missions
6. **Voice/Dialogue**: Create dialogue XML (handled by dScripts)

---

### D. SKILLS/BEHAVIORS

#### Files to Modify

**Required**:
- `CDSkillBehavior` - Maps skill ID to behavior
- `CDBehaviorTemplate` - Behavior tree structure
- `CDBehaviorParameter` - Behavior parameters
- `CDBehaviorParameterTable` - For custom parameters

**Optional**:
- `CDObjectSkills` - Object-specific skills (destructibles, vehicles)
- C++ implementation in `dGame/dBehavior/Behavior.h`

#### Behavior System Overview

DarkflameServer uses a **behavior tree** system where behaviors can be:

1. **Database-driven** - Defined purely in CDClient (damage, healing, buffs)
2. **Code-driven** - Require C++ `Behavior` subclass for complex logic

**Database-Driven Behaviors** (pure CDClient):
- Direct damage
- Healing
- Buffs/debuffs
- Stuns/knockback
- Projectiles

**Code-Driven Behaviors** (require C++):
- Custom game mechanics
- Conditional logic
- Multi-stage behaviors
- Physics-based effects

#### Can Behaviors Be Added Purely via Database?

**Mostly YES, but not always.**

Simple behaviors (damage, heal, buff) can be added via SQL. Complex behaviors require a C++ class inheriting from `Behavior`.

#### Example: Adding a Simple Damage Skill

```sql
-- 1. Create skill (ID 3001)
INSERT INTO SkillBehavior (skillID, behaviorID)
VALUES (3001, 4001);  -- Skill 3001 uses behavior 4001

-- 2. Create behavior (just damage)
INSERT INTO BehaviorTemplate (behaviorID, templateID, effectID)
VALUES (4001, 0, 0);

-- 3. Set behavior parameters (damage amount)
INSERT INTO BehaviorParameter (behaviorID, parameterID, value)
VALUES (4001, 1, 25.5);  -- 25.5 damage
```

#### Code Changes Required

**Simple behaviors**: No C++ needed.

**Complex behaviors**: Create `dGame/dBehavior/Behavior.h` subclass.

Example skeleton:

```cpp
class CustomBehavior : public Behavior {
public:
    void Calculate(BehaviorContext* context) override {
        // Custom calculation logic
    }
    void Execute(BehaviorContext* context) override {
        // Apply behavior effects
    }
};
```

#### Complete Workflow Checklist

1. **Design behavior**: Mechanical effect, parameters
2. **Check if database behavior types exist** - If yes, use those
3. **Create SkillBehavior entry** (maps skill ID to behavior)
4. **Create BehaviorTemplate entry**
5. **Add parameters** in BehaviorParameter
6. **If complex logic**: Write C++ Behavior subclass
7. **Register in CppScripts** (dScripts/CppScripts.cpp)
8. **Test**: Use `/castskill <skill_id>` (GM level 9)

---

### E. LOOT TABLES

#### Files to Modify

**Required**:
- `CDLootMatrix` - High-level loot definition
- `CDLootTable` - Individual item drops
- `CDRarityTable` - Rarity modifiers

**For NPCs**:
- Update NPC's `CDDestructibleComponent` to reference loot matrix

#### Loot System Architecture

```
NPC defeated
  ↓
CDDestructibleComponent.lootMatrixIndex
  ↓
CDLootMatrix (lookup by index) → lists loot tables to roll
  ↓
CDLootTable (for each table) → list of items with drop %
  ↓
Roll RNG, grant item(s) to player
```

#### Example: Add Loot to an Enemy

```sql
-- 1. Create loot matrix (ID 5001)
INSERT INTO LootMatrix (lootMatrixIndex, lootTableIndex, percentChance, minToDrop, maxToDrop, flagID)
VALUES (5001, 1001, 100, 1, 1, 0);  -- 100% drop 1 of table 1001
VALUES (5001, 1002, 50, 0, 1, 0);   -- 50% drop 1 of table 1002

-- 2. Create loot table 1001 (common drops)
INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
VALUES (100, 1001, 0, 1);  -- Item 100 with 1% weight
VALUES (101, 1001, 0, 1);  -- Item 101 with 1% weight

-- 3. Create loot table 1002 (rare drops)
INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
VALUES (500, 1002, 0, 5);  -- Rare item 500

-- 4. Link to NPC
UPDATE DestructibleComponent SET lootMatrixIndex = 5001 WHERE id = 4000;
```

#### Can Claude Generate SQL?

**YES, HIGH confidence** - Loot tables are purely data-driven.

#### Complete Workflow Checklist

1. **Design loot**: Which items, drop rates
2. **Create loot matrix** with percentages
3. **Create loot tables** for each item group
4. **Link to NPC/object** via DestructibleComponent
5. **Balance**: Adjust percentChance and weights
6. **Test**: Defeat NPC repeatedly, verify loot distribution
7. **Use `/rollloot <matrix> <item> <amount>`** (GM level 9) to simulate rolls

---

## PART 4: Development Environment & Testing

### Beyond Building the Server

**Required**:
1. C++ compiler (g++11+, MSVC, or clang)
2. CMake 3.25+ (build system)
3. MariaDB or SQLite (game database)
4. Text editor or IDE (VS Code, CLion, Visual Studio)
5. **LEGO Universe game client** - For testing changes in-game

**Optional (manual tooling — superseded by MCP when available)**:
- DB Browser for SQLite — for CDClient editing (alternative: `sqlite3` CLI). Replaced by **SQLite MCP** for Claude-driven sessions.
- Blender + NIF exporter — for 3D model creation. Replaced by **Blender MCP** for Claude-driven sessions.
- Image editor (GIMP/Photoshop/Paint.NET) + texconv — for DDS textures. Replaced by **image-gen + texconv MCP** for Claude-driven sessions.
- Audio DAW or TTS provider for OGG audio. Replaced by **audio-gen MCP** for Claude-driven sessions.
- Git — version control (no MCP substitute; use `gh` CLI or GitHub MCP for PR/issue workflows).

See `13_CLAUDE_VS_MCP.md` §7 for the MCP setup roadmap.

### GM Commands for Testing

The server includes comprehensive `/` commands for development (GM level 8+). Key ones:

```
# Spawn & inventory
/spawn <lot>                  # Spawn object by LOT ID
/gmadditem <lot> (count)      # Add item to inventory
/setcurrency <amount>         # Set coins

# Game state
/testmap <zone> force         # Teleport to zone (force = no checks)
/pos                          # Print current position
/setlevel <level>             # Set character level

# Content testing
/addmission <mission_id>      # Accept mission
/completemission <mission_id> # Complete mission
/resetmission <mission_id>    # Reset mission state
/castskill <skill_id>         # Cast skill
/buff <buff_id> <seconds>     # Apply buff

# Server operations
/reloadconfig                 # Reload INI config without restart
/force-save                   # Save character to database
```

See `/docs/Commands.md` for complete list (100+ commands).

### Reloading Content Without Full Restart

**Partial reload** (via `/reloadconfig`):
- Reloads INI configuration files
- Does NOT reload CDClient database (would require full server restart)

**Full CDClient reload**:
- Requires restarting the World Server process
- Server auto-converts FDB to SQLite on startup

**Game state reload**:
- Use `/force-save` to sync player data
- Use `/resetmission` to reset mission state
- Use `/setflag` to toggle player flags

### Testing Workflow

1. **Edit CDClient** (SQLite)
2. **Restart server** (World Server)
3. **Log in with test character**
4. **Use GM commands** to verify changes
5. **Check logs** for errors

**Example session**:

```bash
# Terminal 1: Start server
./MasterServer
./WorldServer

# Terminal 2: Check logs
tail -f MasterServer.log
tail -f WorldServer.log

# In-game client
/gmlevel 8              # Become GM
/spawn 12345            # Test new item
/addmission 10001       # Test new mission
/pos                    # Verify position updates
```

### Database Access During Development

**SQLite (development)**:
```bash
sqlite3 resServer/CDServer.sqlite
sqlite> SELECT * FROM Objects WHERE id = 12345;
sqlite> UPDATE ItemComponent SET baseValue = 1000 WHERE id = 12345;
sqlite> .quit
```

**MariaDB (production)**:
```bash
mysql -u user -p -D darkflame
mysql> SELECT * FROM characters LIMIT 5;
mysql> UPDATE characters SET level = 20 WHERE character_id = 1;
```

---

## Summary: Quick Reference

| Content Type | Main Table | SQL Editable? | Assets Needed | Code Needed? | Claude-Drivable End-to-End? |
|---|---|---|---|---|---|
| **Item** | CDItemComponent | YES | NIF/DDS (optional) | NO (usually) | YES, with SQLite + Blender + image-gen MCPs |
| **Mission** | CDMissions | YES | None (audio optional) | NO (usually) | YES, with SQLite MCP (+ audio-gen for voice lines) |
| **NPC** | CDObjects | YES | NIF (optional) | Maybe | YES for dynamic spawn; NO for static zone placement (LUZ) |
| **Skill** | CDSkillBehavior | Partially | None | Maybe | YES, with SQLite MCP |
| **Loot** | CDLootTable | YES | None | NO | YES, with SQLite MCP |
| **Zone** | CDZoneTable | YES | LUZ/LVL (no MCP exists) | YES (script) | Partial — metadata + script yes; static placement still needs human |

**Highest Leverage Wins**:
1. Add items → Highest ROI, pure SQL, instant testing
2. Add missions → Pure SQL, tests content chains
3. Modify existing NPCs → Change vendor/quest offerings, no `.luz` needed
4. Add loot tables → Affects gameplay balance, pure SQL

