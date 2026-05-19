# Content Addition Guide

This guide provides step-by-step instructions for adding each major content type to DarkflameServer. All field names are drawn directly from the CDClient header files in `dDatabase/CDClientDatabase/CDClientTables/`.

---

## Core Concept: The LOT System

Every in-game object has a **LOT** (Lego Object Template) — a `uint32_t` identifier that is the root key for almost all content data. The lookup chain is:

```
LOT (uint32_t)
  └─ CDObjects (id = LOT)          -- name, type, interactionDistance
       └─ CDComponentsRegistry     -- (LOT, component_type) → component_id
            └─ component table     -- looked up by component_id
```

**CDComponentsRegistry** has three fields:
- `id` — the LOT of the object
- `component_type` — value from `eReplicaComponentType` enum
- `component_id` — the row ID in the corresponding component table (0 = non-networked or the ID is literally 0)

Key `eReplicaComponentType` integer values used in CDClient:
| Value | Enum Name | Component Table |
|-------|-----------|-----------------|
| 1 | CONTROLLABLE_PHYSICS | (physics asset) |
| 2 | RENDER | (render asset) |
| 3 | SIMPLE_PHYSICS | CDPhysicsComponent |
| 7 | DESTROYABLE | CDDestructibleComponent |
| 9 | SKILL | CDSkillBehavior (via CDObjectSkills) |
| 11 | ITEM | CDItemComponent |
| 12 | REBUILD / QUICK_BUILD | CDRebuildComponent |
| 16 | VENDOR | CDVendorComponent |
| 17 | INVENTORY | CDInventoryComponent |
| 26 | PET | CDPetComponent |
| 31 | MOVEMENT_AI | CDMovementAIComponent |
| 55 | QUICK_BUILD | CDRebuildComponent |
| 60 | MISSION_OFFER | CDMissionNPCComponent |
| 73 | BASE_COMBAT_AI | (CDBaseCombatAI, loaded from CDObjectSkills) |

> Note: The `eReplicaComponentType` enum in `dCommon/dEnums/eReplicaComponentType.h` uses sequential integers starting at 1. The actual integer stored in CDComponentsRegistry corresponds to the enum value — not a named constant. Always verify against the enum file.

---

## 1. Items

### Data Model

**Table: `Objects`** (`CDObjectsTable.h` — `struct CDObjects`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | The LOT |
| `name` | `std::string` | Internal name, not shown to players |
| `type` | `std::string` | Object type string (e.g. `"Loot"`, `"Equipment"`) |
| `interactionDistance` | `float` | Distance at which player can interact |

**Table: `ComponentsRegistry`** (`CDComponentsRegistryTable.h` — `struct CDComponentsRegistry`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | LOT |
| `component_type` | `eReplicaComponentType` | Type of component (11 = ITEM, 2 = RENDER, etc.) |
| `component_id` | `uint32_t` | Row ID in the component's own table |

**Table: `ItemComponent`** (`CDItemComponentTable.h` — `struct CDItemComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID (matches `component_id` in registry) |
| `equipLocation` | `std::string` | Slot: `"hat"`, `"neck"`, `"chest"`, `"legs"`, `"left_hand"`, `"right_hand"`, etc. |
| `baseValue` | `uint32_t` | Coin value for selling |
| `isKitPiece` | `bool` | Part of a gear kit |
| `rarity` | `uint32_t` | Rarity tier (1=common, higher=rarer) |
| `itemType` | `uint32_t` | See `eItemType` enum |
| `itemInfo` | `int64_t` | Packed info (varies by type) |
| `inLootTable` | `bool` | Can appear in loot drops |
| `inVendor` | `bool` | Can be sold by vendors |
| `isUnique` | `bool` | Only one allowed in inventory |
| `isBOP` | `bool` | Bind on Pickup |
| `isBOE` | `bool` | Bind on Equip |
| `reqFlagID` | `uint32_t` | Player must have this flag set |
| `reqAchievementID` | `uint32_t` | Must have completed this achievement |
| `stackSize` | `uint32_t` | Max stack size (0 or 1 = not stackable) |
| `color1` | `uint32_t` | Custom color |
| `decal` | `uint32_t` | Decal ID |
| `reqPrecondition` | `std::string` | Precondition expression string |
| `animationFlag` | `uint32_t` | Animation flags |
| `isTwoHanded` | `bool` | Two-handed weapon |
| `subItems` | `std::string` | Comma-separated LOTs for sub-items |
| `commendationLOT` | `uint32_t` | LOT of commendation currency |
| `commendationCost` | `uint32_t` | Commendation cost |
| `currencyCosts` | `std::string` | Crafting cost specification |
| `forgeType` | `uint32_t` | Forge/crafting type |
| `SellMultiplier` | `float` | Sell price multiplier |

**`eItemType` values** (`dCommon/dEnums/eItemType.h`):
```
UNKNOWN=-1, BRICK=1, HAT=2, HAIR=3, NECK=4, LEFT_HAND=5, RIGHT_HAND=6,
LEGS=7, LEFT_TRINKET=8, RIGHT_TRINKET=9, BEHAVIOR=10, PROPERTY=11, MODEL=12,
COLLECTIBLE=13, CONSUMABLE=14, CHEST=15, EGG=16, PET_FOOD=17, QUEST_OBJECT=18,
PET_INVENTORY_ITEM=19, PACKAGE=20, LOOT_MODEL=21, VEHICLE=22, LUP_MODEL=23, MOUNT=24
```

### Definition Method
CDClient SQLite database (`CDClient.fdb` / `CDClient.sqlite`). All data is defined in DB rows — no C++ changes needed for a standard item.

### Step-by-Step Addition Process

1. **Choose a LOT.** Pick an unused `uint32_t` ID. Existing LEGO Universe items use IDs up to roughly 14000. Custom items should use a high range (e.g. 30000+) to avoid conflicts.

2. **Insert into `Objects`:**
   ```sql
   INSERT INTO Objects (id, name, placeable, type, description, localize, npcTemplateID,
       displayName, interactionDistance, nametag, _internalNotes, locStatus, gate_version, HQ_valid)
   VALUES (30001, 'MyCustomHat', 0, 'Loot', 'A custom hat', 1, NULL,
       'My Custom Hat', 3.0, 0, '', 0, '', 0);
   ```

3. **Create an `ItemComponent` row:**
   ```sql
   INSERT INTO ItemComponent (id, equipLocation, baseValue, isKitPiece, rarity, itemType,
       itemInfo, inLootTable, inVendor, isUnique, isBOP, isBOE, reqFlagID, reqSpecialtyID,
       reqSpecRank, reqAchievementID, stackSize, color1, decal, offsetGroupID, buildTypes,
       reqPrecondition, animationFlag, equipEffects, readyForQA, itemRating, isTwoHanded,
       minNumRequired, delResIndex, currencyLOT, altCurrencyCost, subItems, noEquipAnimation,
       commendationLOT, commendationCost, currencyCosts, locStatus, forgeType, SellMultiplier)
   VALUES (5001, 'hat', 100, 0, 2, 2,
       0, 1, 1, 0, 0, 0, 0, 0,
       0, 0, 1, 0, 0, 0, 0,
       '', 0, 0, 1, 0, 0,
       0, 0, 0, 0, '', 0,
       0, 0, '', 0, 0, 1.0);
   ```
   Note: `id` here (5001) is your new `component_id`, not the LOT.

4. **Register components in `ComponentsRegistry`:**
   ```sql
   -- Render component (type 2) - component_id 0 means use default render
   INSERT INTO ComponentsRegistry (id, component_type, component_id)
   VALUES (30001, 2, 0);

   -- Item component (type 11)
   INSERT INTO ComponentsRegistry (id, component_type, component_id)
   VALUES (30001, 11, 5001);
   ```

5. **Add a render asset.** The render component references a NIF file by path. For hats, the NIF typically lives in `res/meshes/`. The render component_id points to a `RenderComponent` table row which specifies the asset path. **MCP path**: with Blender MCP + image-gen+texconv MCP connected, Claude can model the hat in Blender, export the NIF, generate matching DDS textures, and insert the `RenderComponent` row in a single session — see `13_CLAUDE_VS_MCP.md` §6 Workflow A. (NIF dialect caveat in §2.2 applies.)

6. **For loot drops:** Set `inLootTable = 1` on the ItemComponent, then add a row to `LootTable` pointing at this LOT (see section 5).

7. **For vendor sales:** Set `inVendor = 1`, then add the LOT to the vendor's loot matrix (see section 8).

### Code Integration Points
- No C++ changes are needed for basic items.
- If a consumable item needs a new skill effect: add the skill (section 4) and set `itemType = 14` (CONSUMABLE), with the skill referenced via `CDObjectSkills`.
- Package items (`itemType = 20`) need a `PackageComponent` row in CDPackageComponent that points to a `LootMatrixIndex`.

### Asset Requirements
- **NIF model:** `res/meshes/` — referenced by RenderComponent table. *Creation: Blender MCP (`13_CLAUDE_VS_MCP.md` §2.2) or manual Blender + NIF exporter.*
- **DDS textures:** `res/textures/` — embedded in NIF or referenced separately. *Creation: image-gen + texconv MCP (`13_CLAUDE_VS_MCP.md` §2.3) or manual texconv.*
- **Icon:** Optional, referenced by `missionIconID` or item icon fields. *Same DDS pipeline as above.*
- **Sounds:** Optional, referenced by audio event fields in ItemComponent. *Creation: audio-gen MCP (`13_CLAUDE_VS_MCP.md` §2.4) or manual DAW/TTS.*

### Testing Checklist
- [ ] LOT appears correctly in CDObjects
- [ ] ComponentsRegistry has entries for both RENDER (2) and ITEM (11) component types
- [ ] ItemComponent row exists with matching `id` = component_id from registry
- [ ] Item drops correctly from a loot table (if `inLootTable = 1`)
- [ ] Item appears in vendor inventory (if `inVendor = 1`)
- [ ] `equipLocation` string is valid and item equips to the correct slot
- [ ] `stackSize` is correct (1 for equipment, higher for consumables/bricks)

### Worked Example

Adding a custom hat with LOT 30001, component_id 5001, rarity 2 (uncommon), sold by vendor, drops from loot:

```sql
-- Step 1: Objects table
INSERT INTO Objects (id, name, type, interactionDistance)
VALUES (30001, 'custom_pirate_hat', 'Loot', 3.0);

-- Step 2: ItemComponent table
INSERT INTO ItemComponent (id, equipLocation, baseValue, rarity, itemType,
    inLootTable, inVendor, stackSize, SellMultiplier)
VALUES (5001, 'hat', 500, 2, 2, 1, 1, 1, 1.0);

-- Step 3: ComponentsRegistry
INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30001, 2, 0);
INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30001, 11, 5001);
```

### Constraints & Validation Rules
- `id` in `ComponentsRegistry` must match a row in `Objects`
- `component_id` in registry must match `id` in the target component table
- `equipLocation` must be one of the recognized strings (`hat`, `neck`, `chest`, `legs`, `left_hand`, `right_hand`, `left_trinket`, `right_trinket`)
- `itemType` must be a valid `eItemType` integer
- LOT 0 is reserved (null/invalid)

### Common Pitfalls
- Forgetting to add the RENDER component (type 2) to ComponentsRegistry — item will be invisible
- Using a `component_id` that already exists in the ItemComponent table for a different item
- Setting `inLootTable = 1` but not adding the LOT to any LootTable row — item never drops
- `stackSize = 0` behaves like `stackSize = 1` in most contexts but may cause unexpected behavior

---

## 2. Missions & Tasks

### Data Model

**Table: `Missions`** (`CDMissionsTable.h` — `struct CDMissions`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `int32_t` | Mission ID |
| `defined_type` | `std::string` | Category (e.g. `"Story"`, `"Achievement"`) |
| `defined_subtype` | `std::string` | Sub-category |
| `UISortOrder` | `int32_t` | Display order in mission journal |
| `offer_objectID` | `int32_t` | LOT of NPC that offers this mission |
| `target_objectID` | `int32_t` | LOT of NPC that accepts completion |
| `reward_currency` | `int64_t` | Coins awarded |
| `LegoScore` | `int32_t` | LEGO Universe Score (U-Score) awarded |
| `reward_reputation` | `int64_t` | Reputation points awarded |
| `isChoiceReward` | `bool` | Player picks one of the reward items |
| `reward_item1..4` | `int32_t` | LOTs of up to 4 reward items |
| `reward_item1..4_count` | `int32_t` | Counts for each reward item |
| `reward_emote..4` | `int32_t` | Emote IDs rewarded |
| `reward_maximagination` | `int32_t` | Max imagination increase |
| `reward_maxhealth` | `int32_t` | Max health increase |
| `reward_maxinventory` | `int32_t` | Inventory slot increase |
| `repeatable` | `bool` | Can be done multiple times (daily) |
| `reward_currency_repeatable` | `int64_t` | Currency for repeat completions |
| `reward_item1..4_repeatable` | `int32_t` | Repeat reward items |
| `reward_item1..4_repeat_count` | `int32_t` | Repeat reward item counts |
| `time_limit` | `int32_t` | Time limit in seconds (0 = no limit) |
| `isMission` | `bool` | `true` = mission, `false` = achievement |
| `missionIconID` | `int32_t` | Icon ID for UI display |
| `prereqMissionID` | `std::string` | Pipe-separated prerequisite mission IDs |
| `cooldownTime` | `int64_t` | Cooldown seconds for repeatable missions |
| `reward_bankinventory` | `int32_t` | Bank slot increase |

**Table: `MissionTasks`** (`CDMissionTasksTable.h` — `struct CDMissionTasks`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Mission ID this task belongs to |
| `taskType` | `uint32_t` | Task type (see enum below) |
| `target` | `uint32_t` | Primary target (meaning depends on type) |
| `targetGroup` | `std::string` | Comma-separated list of LOTs/IDs |
| `targetValue` | `int32_t` | How many times to complete the task |
| `taskParam1` | `std::string` | Extra parameter (meaning depends on type) |
| `uid` | `uint32_t` | Unique task ID within the mission |

**`eMissionTaskType` values** (`dCommon/dEnums/eMissionTaskType.h`):
| Value | Name | `target` meaning | `targetValue` meaning |
|-------|------|------------------|-----------------------|
| 0 | SMASH | LOT to smash | Count |
| 1 | SCRIPT | Script event string | Count |
| 2 | ACTIVITY | Activity ID | Count |
| 3 | COLLECTION | Collectible ID | Count |
| 4 | TALK_TO_NPC | NPC LOT | Count |
| 5 | EMOTE | Emote ID | Count |
| 8 | USE_ITEM | Item LOT | Count |
| 9 | USE_SKILL | Skill ID | Count |
| 10 | GATHER | Item LOT to collect | Count |
| 11 | EXPLORE | Zone ID | Count |
| 13 | DELIVERY | Item LOT to deliver | Count |
| 14 | PERFORM_ACTIVITY | Activity ID | Count |
| 15 | INTERACT | NPC LOT | Count |
| 22 | PET_TAMING | Pet LOT | Count |
| 23 | RACING | Race result type | Count |
| 24 | PLAYER_FLAG | Flag ID | Count |
| 25 | PLACE_MODEL | Model LOT | Count |
| 30 | TIME_PLAYED | Time in seconds | Threshold |
| 31 | DONATION | Item LOT | Count |

**Mission states** (`eMissionState` enum in `dCommon/dEnums/eMissionState.h`):
```
UNKNOWN=-1, REWARDING=0, AVAILABLE=1, ACTIVE=2, READY_TO_COMPLETE=4,
COMPLETE=8, COMPLETE_AVAILABLE=9, COMPLETE_ACTIVE=10,
COMPLETE_READY_TO_COMPLETE=12, FAILED=16
```

### Definition Method
CDClient database. No C++ changes required for standard task types. Lua scripts may interact with missions via `MissionComponent::Progress()`.

### Step-by-Step Addition Process

1. **Plan the mission chain.** Decide: is this a story mission (`isMission = 1`) or an achievement (`isMission = 0`)? Achievements are auto-accepted and tracked silently.

2. **Insert the mission into `Missions`:**
   ```sql
   INSERT INTO Missions (id, defined_type, defined_subtype, UISortOrder,
       offer_objectID, target_objectID, reward_currency, LegoScore,
       reward_reputation, isChoiceReward, reward_item1, reward_item1_count,
       reward_item2, reward_item2_count, reward_item3, reward_item3_count,
       reward_item4, reward_item4_count, reward_emote, reward_emote2,
       reward_emote3, reward_emote4, reward_maximagination, reward_maxhealth,
       reward_maxinventory, reward_maxmodel, reward_maxwidget, reward_maxwallet,
       repeatable, reward_currency_repeatable, reward_item1_repeatable,
       reward_item1_repeat_count, reward_item2_repeatable, reward_item2_repeat_count,
       reward_item3_repeatable, reward_item3_repeat_count, reward_item4_repeatable,
       reward_item4_repeat_count, time_limit, isMission, missionIconID,
       prereqMissionID, localize, inMOTD, cooldownTime, isRandom, randomPool,
       UIPrereqID, reward_bankinventory)
   VALUES (2001, 'Story', 'Main', 100,
       1234, 1234, 500, 10,
       0, 0, 30001, 1,
       0, 0, 0, 0,
       0, 0, 0, 0,
       0, 0, 0, 0,
       0, 0, 0, 0,
       0, 0, 0,
       0, 0, 0,
       0, 0, 0,
       0, 0, 1, 0,
       '', 1, 0, 0, 0, '',
       0, 0);
   ```

3. **Insert tasks into `MissionTasks`:**
   ```sql
   -- Task 1: Smash 5 Stromlings (LOT 6231)
   INSERT INTO MissionTasks (id, locStatus, taskType, target, targetGroup,
       targetValue, taskParam1, uid)
   VALUES (2001, 0, 0, 6231, '',
       5, '', 1);

   -- Task 2: Talk to NPC (LOT 1234)
   INSERT INTO MissionTasks (id, locStatus, taskType, target, targetGroup,
       targetValue, taskParam1, uid)
   VALUES (2001, 0, 4, 1234, '',
       1, '', 2);
   ```

4. **Link the offering NPC.** Add a row to `MissionNPCComponent` (see NPC section), or set `offer_objectID` on the mission — but note that NPCs require `CDMissionNPCComponentTable` rows to offer/accept missions.

5. **Handle prerequisites.** Set `prereqMissionID` to a pipe-separated list: `"1900|2000"` means both mission 1900 and 2000 must be complete.

6. **For achievements** (`isMission = 0`): set `offer_objectID = 0`, `target_objectID = 0`. They trigger automatically when task conditions are met.

### Code Integration Points
- `MissionComponent::Progress(eMissionTaskType, value, ...)` — called by game systems when relevant actions occur
- `MissionComponent::AcceptMission(missionId)` — triggered by NPC interaction
- `MissionComponent::CompleteMission(missionId)` — called when all tasks done and player talks to target NPC
- For new task types beyond the existing enum: extend `eMissionTaskType` in `dCommon/dEnums/eMissionTaskType.h` and add a case in `MissionTask::IsComplete()` / the mission progression logic

### Asset Requirements
- Mission icons referenced by `missionIconID` should exist in the client assets
- Reward items must be valid LOTs in CDObjects

### Testing Checklist
- [ ] Mission row exists in `Missions` table with correct `id`
- [ ] At least one `MissionTasks` row exists with matching `id`
- [ ] All `taskType` values are valid `eMissionTaskType` integers
- [ ] `offer_objectID` LOT has a `MissionNPCComponent` row with `offersMission = 1`
- [ ] `target_objectID` LOT has a `MissionNPCComponent` row with `acceptsMission = 1`
- [ ] Prerequisite mission IDs in `prereqMissionID` are valid and completable
- [ ] Reward items (`reward_item1..4`) are valid LOTs
- [ ] `isMission` flag is correct (1 for story, 0 for achievement)

### Worked Example

Achievement for smashing 10 enemies:
```sql
INSERT INTO Missions (id, defined_type, defined_subtype, UISortOrder,
    offer_objectID, target_objectID, reward_currency, LegoScore,
    isMission, reward_maxinventory, prereqMissionID, localize, repeatable,
    reward_currency_repeatable, time_limit, cooldownTime, reward_bankinventory)
VALUES (9001, 'Achievement', 'Combat', 500,
    0, 0, 100, 25,
    0, 0, '', 1, 0,
    0, 0, 0, 0);

INSERT INTO MissionTasks (id, taskType, target, targetValue, uid)
VALUES (9001, 0, 0, 10, 1);
-- target=0 means ANY enemy for SMASH type; use specific LOT to restrict
```

### Constraints & Validation Rules
- Each `MissionTasks.uid` must be unique within the same mission
- `prereqMissionID` format: single ID or pipe-separated IDs (`"1000|1001"`)
- `targetValue` must be >= 1
- Reward items must exist in CDObjects/CDItemComponent or be 0

### Common Pitfalls
- Tasks with `taskType = 3` (COLLECTION) require the object to have a `CollectibleComponent` in its registry
- Setting `offer_objectID` without also adding a `MissionNPCComponent` row — NPC will not offer the mission
- `isMission = 0` achievements with `offer_objectID != 0` will behave unexpectedly
- Forgetting to set `localize = 1` means the mission name won't appear in the client UI

---

## 3. NPCs

### Data Model

NPCs are defined by combining several tables:

**`Objects`** — base definition (see Items section for fields)

**`DestructibleComponent`** (`CDDestructibleComponentTable.h` — `struct CDDestructibleComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `faction` | `int32_t` | Faction ID (-1 = no faction) |
| `factionList` | `std::string` | Semicolon-separated faction list |
| `life` | `int32_t` | Max HP |
| `imagination` | `uint32_t` | Max imagination |
| `LootMatrixIndex` | `int32_t` | Loot matrix for drops |
| `CurrencyIndex` | `int32_t` | Currency index for coin drops |
| `level` | `uint32_t` | Level of the entity |
| `armor` | `float` | Armor points |
| `death_behavior` | `uint32_t` | Behavior ID on death |
| `isnpc` | `bool` | True if NPC, false if enemy |
| `attack_priority` | `uint32_t` | AI priority |
| `isSmashable` | `bool` | Can be smashed |
| `difficultyLevel` | `int32_t` | Difficulty rating |

**`MissionNPCComponent`** (`CDMissionNPCComponentTable.h` — `struct CDMissionNPCComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `missionID` | `uint32_t` | Mission ID this NPC is linked to |
| `offersMission` | `bool` | NPC gives this mission |
| `acceptsMission` | `bool` | NPC accepts turn-in for this mission |
| `gate_version` | `std::string` | Feature gate |

**`ScriptComponent`** (`CDScriptComponentTable.h` — `struct CDScriptComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `script_name` | `std::string` | Server-side Lua script path |
| `client_script_name` | `std::string` | Client-side script path |

**`MovementAIComponent`** (`CDMovementAIComponentTable.h`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `MovementType` | `std::string` | `"Ground"`, `"Air"`, etc. |
| `WanderChance` | `float` | 0.0–1.0 probability of wandering |
| `WanderDelayMin` | `float` | Min seconds between wanders |
| `WanderDelayMax` | `float` | Max seconds between wanders |
| `WanderSpeed` | `float` | Movement speed while wandering |
| `WanderRadius` | `float` | Radius of wander area |
| `attachedPath` | `std::string` | Named path from LUZ file |

**`InventoryComponent`** (`CDInventoryComponentTable.h`) — items an NPC starts with or uses
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `itemid` | `uint32_t` | LOT of the item |
| `count` | `uint32_t` | Count |
| `equip` | `bool` | Whether to equip on spawn |

### NPC vs Enemy vs Vendor Distinction

| Role | `isnpc` | Has `VENDOR` (16) | Has `MISSION_OFFER` (60) | Has `BASE_COMBAT_AI` (73) |
|------|---------|-------------------|---------------------------|---------------------------|
| Story NPC | true | false | true/false | false |
| Vendor NPC | true | true | false | false |
| Enemy | false | false | false | true |
| Quest enemy | false | false | false | true |

### Step-by-Step Addition Process

1. **Insert into `Objects`.**

2. **Insert a `DestructibleComponent` row** (even for friendly NPCs — set `life` appropriately, `isnpc = 1`).

3. **Register in `ComponentsRegistry`:**
   - Type 2 (RENDER): visual asset
   - Type 3 (SIMPLE_PHYSICS) or 1 (CONTROLLABLE_PHYSICS): movement physics
   - Type 7 (DESTROYABLE): links to DestructibleComponent
   - Type 31 (MOVEMENT_AI): if NPC wanders
   - Type 60 (MISSION_OFFER): if NPC offers/accepts missions
   - Type 16 (VENDOR): if NPC is a vendor
   - Type 5 (SCRIPT): if NPC has server-side behavior script

4. **For quest-giver NPCs:** Add rows to `MissionNPCComponent` for each mission the NPC is involved in.

5. **Place in world:** Spawners in LUZ files reference the LOT. The spawner is defined in the `.luz` level file and references the LOT. The server reads this via `Zone` → `Level` → spawn objects.

6. **For enemies:** Also add `ObjectSkills` rows (see Behaviors section) and a `BaseCombatAIComponent` entry (type 73).

### Code Integration Points
- NPC dialogue is handled by Lua scripts referenced in `ScriptComponent`
- `MissionOfferComponent` reads from `CDMissionNPCComponentTable` to know which missions to display
- `BaseCombatAIComponent` reads `CDObjectSkillsTable` to get enemy skills

### Asset Requirements
- NIF model for visual representation — *Blender MCP can drive `bpy` to model, rig, and run the NIF exporter; see `13_CLAUDE_VS_MCP.md` §2.2.*
- Animation files for idle/walk/attack animations — *Authored in Blender alongside the mesh; same MCP applies.*
- Collision mesh for physics — *Often reuses the render mesh or a simplified primitive; can be generated in the same Blender session.*

### Testing Checklist
- [ ] LOT exists in `Objects` with correct `type`
- [ ] `DestructibleComponent` row exists with appropriate `life` and `isnpc` flag
- [ ] All `ComponentsRegistry` entries are present
- [ ] Mission-related NPCs have `MissionNPCComponent` rows
- [ ] Spawner in LUZ/level file references correct LOT
- [ ] NPC appears at correct position in world
- [ ] Wander radius and speed are reasonable
- [ ] For enemies: skills and AI behavior configured

### Worked Example

Simple quest-giver NPC with LOT 30010:
```sql
INSERT INTO Objects (id, name, type, interactionDistance) VALUES (30010, 'MyQuestGiver', 'NPC', 4.0);
INSERT INTO DestructibleComponent (id, faction, life, imagination, LootMatrixIndex,
    CurrencyIndex, level, armor, isnpc, isSmashable)
VALUES (6001, -1, 100, 0, 0, 0, 1, 0, 1, 0);
-- ComponentsRegistry entries:
INSERT INTO ComponentsRegistry VALUES (30010, 2, 0);   -- RENDER
INSERT INTO ComponentsRegistry VALUES (30010, 3, 0);   -- SIMPLE_PHYSICS
INSERT INTO ComponentsRegistry VALUES (30010, 7, 6001); -- DESTROYABLE → DestructibleComponent
INSERT INTO ComponentsRegistry VALUES (30010, 60, 6002); -- MISSION_OFFER
-- MissionNPCComponent:
INSERT INTO MissionNPCComponent (id, missionID, offersMission, acceptsMission, gate_version)
VALUES (6002, 2001, 1, 1, '');
```

### Constraints & Validation Rules
- `faction` in DestructibleComponent: 1 = Assembly, 2 = Paradox, 3 = Venture League, 4 = Sentinel. Negative = no faction
- `LootMatrixIndex` must be 0 or reference a valid `LootMatrix` index
- Every NPC needs at least a RENDER and PHYSICS component

### Common Pitfalls
- Missing DESTROYABLE (type 7) component — NPC cannot be targeted or interacted with correctly
- Setting `isnpc = 0` on a quest NPC — AI treats it as an enemy
- Spawner in LUZ file references wrong LOT or LOT doesn't exist in CDObjects

---

## 4. Enemy Behaviors & Skills

### Data Model

**`ObjectSkills`** (`CDObjectSkillsTable.h` — `struct CDObjectSkills`)
| Column | Type | Notes |
|--------|------|-------|
| `objectTemplate` | `uint32_t` | LOT of the object |
| `skillID` | `uint32_t` | Skill ID to assign |
| `castOnType` | `uint32_t` | When to cast (0=default, 1=on hit, etc.) |
| `AICombatWeight` | `uint32_t` | AI preference weight for this skill |

**`SkillBehavior`** (`CDSkillBehaviorTable.h` — `struct CDSkillBehavior`)
| Column | Type | Notes |
|--------|------|-------|
| `skillID` | `uint32_t` | Skill ID |
| `behaviorID` | `uint32_t` | Root behavior ID |
| `imaginationcost` | `uint32_t` | Imagination cost to cast |
| `cooldowngroup` | `uint32_t` | Cooldown group ID |
| `cooldown` | `float` | Cooldown time in seconds |

**`BehaviorTemplate`** (`CDBehaviorTemplateTable.h` — `struct CDBehaviorTemplate`)
| Column | Type | Notes |
|--------|------|-------|
| `behaviorID` | `uint32_t` | Behavior node ID |
| `templateID` | `uint32_t` | Template type (maps to `BehaviorTemplate` enum) |
| `effectID` | `uint32_t` | Visual effect ID |
| `effectHandle` | `std::string` | Effect handle string |

**`BehaviorParameter`** (`CDBehaviorParameterTable.h`) — key/value pairs
| Column | Type | Notes |
|--------|------|-------|
| `behaviorID` | `uint32_t` | Behavior node ID |
| `parameterID` | `std::string` | Parameter name (e.g. `"damage"`, `"radius"`, `"action"`) |
| `value` | `float` | Parameter value |

### Behavior Tree Structure

Skills form a tree of behavior nodes:
```
SkillBehavior.skillID → SkillBehavior.behaviorID (root node)
    BehaviorTemplate.behaviorID → templateID (which C++ class to use)
    BehaviorParameter (behaviorID, "action") → child behaviorID
    BehaviorParameter (behaviorID, "on_success") → child behaviorID
    BehaviorParameter (behaviorID, "damage") → 15.0 (float param)
```

### `BehaviorTemplate` enum values (from `dGame/dBehaviors/BehaviorTemplate.h`):
| Value | Name | C++ Class | Data-Driven? |
|-------|------|-----------|--------------|
| 1 | BASIC_ATTACK | `BasicAttackBehavior` | Mostly (damage in params) |
| 2 | TAC_ARC | `TacArcBehavior` | Yes (radius, angle params) |
| 3 | AND | `AndBehavior` | Yes (chains sub-behaviors) |
| 4 | PROJECTILE_ATTACK | `ProjectileAttackBehavior` | Yes |
| 5 | HEAL | `HealBehavior` | Yes (`health` param) |
| 6 | MOVEMENT_SWITCH | `MovementSwitchBehavior` | Yes |
| 7 | AREA_OF_EFFECT | `AreaOfEffectBehavior` | Yes (`radius`, `max_targets`) |
| 11 | OVER_TIME | `OverTimeBehavior` | Yes |
| 12 | IMAGINATION | `ImaginationBehavior` | Yes |
| 14 | STUN | `StunBehavior` | Yes |
| 16 | KNOCKBACK | `KnockbackBehavior` | Yes |
| 22 | SPEED | `SpeedBehavior` | Yes |
| 25 | LOOT_BUFF | `LootBuffBehavior` | Yes |
| 27 | SPAWN_OBJECT | `SpawnBehavior` | Yes |
| 29 | SWITCH | `SwitchBehavior` | Yes |
| 30 | BUFF | `BuffBehavior` | Yes |
| 31 | JETPACK | `JetPackBehavior` | Mostly |
| 43 | INTERRUPT | `InterruptBehavior` | Yes |
| 48 | NPC_COMBAT_SKILL | `NpcCombatSkillBehavior` | Yes |
| 55 | BLOCK | `BlockBehavior` | Yes |
| 58 | TAUNT | `TauntBehavior` | Yes |
| 60 | SPAWN_QUICKBUILD | `SpawnQuickbuildBehavior` | Yes |

**Fully data-driven** (no C++ changes): HEAL, AND, OVER_TIME, AREA_OF_EFFECT, STUN, KNOCKBACK, SPEED, LOOT_BUFF, BUFF, TAUNT, BLOCK, SWITCH, SWITCH_MULTIPLE, TAC_ARC, PROJECTILE_ATTACK.

**Require C++ for new functionality**: any template not in `BehaviorTemplate.h`, or new behavior types with entirely new game mechanics.

### Step-by-Step Addition Process

1. **Design the behavior tree.** Draw out the node hierarchy: which node is the root, what are its children (via BehaviorParameter entries), and what are the leaf parameters.

2. **Insert into `SkillBehavior`:**
   ```sql
   INSERT INTO SkillBehavior (skillID, locStatus, behaviorID, imaginationcost,
       cooldowngroup, cooldown)
   VALUES (9001, 0, 20001, 3, 1, 5.0);
   ```

3. **Insert `BehaviorTemplate` rows** for each node in the tree:
   ```sql
   -- Root: AND node
   INSERT INTO BehaviorTemplate (behaviorID, templateID, effectID, effectHandle)
   VALUES (20001, 3, 0, '');
   -- Child: BasicAttack node
   INSERT INTO BehaviorTemplate (behaviorID, templateID, effectID, effectHandle)
   VALUES (20002, 1, 0, '');
   ```

4. **Insert `BehaviorParameter` rows** to connect nodes and set values:
   ```sql
   -- AND node action points to BasicAttack
   INSERT INTO BehaviorParameter (behaviorID, parameterID, value)
   VALUES (20001, 'action 0', 20002);
   -- BasicAttack damage
   INSERT INTO BehaviorParameter (behaviorID, parameterID, value)
   VALUES (20002, 'damage', 10.0);
   INSERT INTO BehaviorParameter (behaviorID, parameterID, value)
   VALUES (20002, 'min damage', 8.0);
   ```

5. **Link skill to object via `ObjectSkills`:**
   ```sql
   INSERT INTO ObjectSkills (objectTemplate, skillID, castOnType, AICombatWeight)
   VALUES (30010, 9001, 0, 50);
   ```

6. **For new behavior types** (requiring C++):
   - Add value to `BehaviorTemplate` enum in `dGame/dBehaviors/BehaviorTemplate.h`
   - Create `MyBehavior.h` / `MyBehavior.cpp` in `dGame/dBehaviors/` inheriting from `Behavior`
   - Implement `Load()`, `Handle()`, `Calculate()` as needed
   - Register in `Behavior::CreateBehavior()` in `dGame/dBehaviors/Behavior.cpp`
   - Add to `CMakeLists.txt`

### Code Integration Points
- `Behavior::CreateBehavior(behaviorId)` in `Behavior.cpp` — factory method, dispatches to correct subclass
- `SkillComponent::CastSkill()` / `CalculateBehavior()` — entry point for skill execution
- `BaseCombatAIComponent` — selects skills from `AiSkillEntry` list built from `CDObjectSkillsTable`
- `BehaviorContext` — tracks state during behavior tree traversal

### Asset Requirements
- Visual effects referenced by `effectID` in BehaviorTemplate must exist in client assets
- Projectile LOTs for PROJECTILE_ATTACK must be valid items

### Testing Checklist
- [ ] `SkillBehavior` row exists with correct `skillID` → `behaviorID`
- [ ] All `BehaviorTemplate` nodes exist and have valid `templateID` values
- [ ] `BehaviorParameter` rows properly link parent nodes to children
- [ ] `ObjectSkills` row links LOT to skill
- [ ] Skill fires when triggered (use debug/GM commands to test)
- [ ] Imagination cost deducts correctly
- [ ] Cooldown prevents immediate re-cast

### Worked Example

Simple 10-damage melee attack skill for enemy LOT 30020:
```sql
INSERT INTO SkillBehavior (skillID, behaviorID, imaginationcost, cooldown)
VALUES (9010, 20010, 0, 3.0);

INSERT INTO BehaviorTemplate (behaviorID, templateID, effectID, effectHandle)
VALUES (20010, 1, 0, '');  -- templateID 1 = BASIC_ATTACK

INSERT INTO BehaviorParameter (behaviorID, parameterID, value) VALUES (20010, 'damage', 10.0);
INSERT INTO BehaviorParameter (behaviorID, parameterID, value) VALUES (20010, 'min damage', 8.0);
INSERT INTO BehaviorParameter (behaviorID, parameterID, value) VALUES (20010, 'on_success', 0);

INSERT INTO ObjectSkills (objectTemplate, skillID, castOnType, AICombatWeight)
VALUES (30020, 9010, 0, 100);
```

### Constraints & Validation Rules
- `BehaviorParameter.parameterID` is case-sensitive and must match what the C++ behavior class calls via `GetFloat()` / `GetAction()`
- Circular behavior references (A → B → A) will cause infinite loops
- Child behavior IDs referenced via parameters must have their own `BehaviorTemplate` rows

### Common Pitfalls
- Missing `BehaviorTemplate` row for a node — `Behavior::CreateBehavior()` returns `EmptyBehavior`
- Using float-valued behavior IDs in parameters (child links) — must be integer IDs cast to float
- Not adding `ObjectSkills` row — skill never gets assigned to the enemy
- `AICombatWeight = 0` — AI never selects this skill

---

## 5. Loot Tables

### Data Model

**`LootMatrix`** (`CDLootMatrixTable.h` — `struct CDLootMatrix`)
| Column | Type | Notes |
|--------|------|-------|
| `LootTableIndex` | `uint32_t` | Foreign key to a LootTable group |
| `RarityTableIndex` | `uint32_t` | Which rarity table to use for rolls |
| `percent` | `float` | Probability this matrix entry is used (0.0–1.0) |
| `minToDrop` | `uint32_t` | Minimum items dropped from this entry |
| `maxToDrop` | `uint32_t` | Maximum items dropped from this entry |
| `flagID` | `uint32_t` | Optional flag required to drop |

> A `LootMatrixIndex` maps to **multiple** `CDLootMatrix` rows (one per loot table in the matrix).

**`LootTable`** (`CDLootTableTable.h` — `struct CDLootTable`)
| Column | Type | Notes |
|--------|------|-------|
| `itemid` | `uint32_t` | LOT of the item to drop |
| `LootTableIndex` | `uint32_t` | Which loot table this belongs to |
| `MissionDrop` | `bool` | Only drops when player has active mission needing it |
| `sortPriority` | `uint32_t` | Display/selection priority |

**`RarityTable`** (`CDRarityTableTable.h` — `struct CDRarityTable`)
| Column | Type | Notes |
|--------|------|-------|
| `randmax` | `float` | Upper bound of random range for this rarity tier |
| `rarity` | `uint32_t` | Rarity value (matches `CDItemComponent.rarity`) |

The rarity roll works: generate random float 0–1, find first `RarityTable` entry where `randmax >= roll`, use that rarity tier to filter items from the LootTable.

### Loot Drop Chain
```
Enemy LOT
  └─ DestructibleComponent.LootMatrixIndex = 50
       └─ LootMatrix (where LootMatrixIndex = 50)
            ├─ Entry: LootTableIndex=100, RarityTableIndex=1, percent=0.5, min=1, max=2
            └─ Entry: LootTableIndex=101, RarityTableIndex=2, percent=0.25, min=1, max=1
                 └─ LootTable (where LootTableIndex=101)
                      ├─ itemid=30001 (LOT), sortPriority=1
                      └─ itemid=30002 (LOT), sortPriority=2
                           └─ CDObjects (id=30001) -- must exist
```

### Step-by-Step Addition Process

1. **Choose a new `LootTableIndex`** (unused integer, e.g. 1500).

2. **Add item rows to `LootTable`:**
   ```sql
   INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
   VALUES (30001, 1500, 0, 1);
   INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
   VALUES (30002, 1500, 0, 2);
   ```

3. **Choose or create a `LootMatrixIndex`** (unused integer, e.g. 800).

4. **Add rows to `LootMatrix`:**
   ```sql
   INSERT INTO LootMatrix (LootMatrixIndex, LootTableIndex, RarityTableIndex,
       percent, minToDrop, maxToDrop, flagID)
   VALUES (800, 1500, 1, 1.0, 1, 2, 0);
   ```

5. **Assign to enemy/container.** Set `LootMatrixIndex = 800` on the `DestructibleComponent` for the enemy LOT, or on the `PackageComponent` for a chest/package.

### Code Integration Points
- `DestroyableComponent` stores `m_LootMatrixID` (read from CDDestructibleComponent)
- Loot is rolled and dropped in the death handling code, which calls into the loot system using `LootMatrixIndex`

### Testing Checklist
- [ ] All `LootTable.itemid` values exist in `Objects` and `ItemComponent`
- [ ] `LootMatrix` rows exist for the `LootMatrixIndex`
- [ ] `RarityTableIndex` references a valid rarity table
- [ ] `percent` values across a matrix sum appropriately (can be < 1.0 for chance of no drop)
- [ ] `minToDrop <= maxToDrop`
- [ ] Enemy's `DestructibleComponent.LootMatrixIndex` is set

### Worked Example

Enemy that drops 1–2 items from a pool of 3, with 80% drop chance:
```sql
-- LootTable entries
INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority) VALUES (30001, 1500, 0, 1);
INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority) VALUES (30002, 1500, 0, 2);
INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority) VALUES (30003, 1500, 0, 3);

-- LootMatrix
INSERT INTO LootMatrix (LootMatrixIndex, LootTableIndex, RarityTableIndex, percent, minToDrop, maxToDrop, flagID)
VALUES (800, 1500, 1, 0.8, 1, 2, 0);

-- Assign to enemy
UPDATE DestructibleComponent SET LootMatrixIndex = 800 WHERE id = 6001;
```

### Constraints & Validation Rules
- `itemid` must reference a valid LOT in `Objects`
- `percent` is per-matrix-entry, not per-item
- `RarityTableIndex` must reference at least one row in `RarityTable`
- Multiple `LootMatrix` rows with the same `LootMatrixIndex` are all rolled independently

### Common Pitfalls
- Confusing `LootTableIndex` and `LootMatrixIndex` — they are different things
- Setting `MissionDrop = 1` on items that should always drop
- `percent = 0` effectively disables the loot table entry

---

## 6. Pets

### Data Model

**`PetComponent`** (`CDPetComponentTable.h` — `struct CDPetComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `walkSpeed` | `float` | Walking speed |
| `runSpeed` | `float` | Running speed |
| `sprintSpeed` | `float` | Sprint speed |
| `imaginationDrainRate` | `float` | Imagination drain per second while active |

Many fields are `UNUSED_COLUMN` (defined in DB but not used server-side):
- `minTameUpdateTime`, `maxTameUpdateTime`, `percentTameChance`, `tameability`
- `elementType`, `idleTimeMin`, `idleTimeMax`, `petForm`
- `AudioMetaEventSet`, `buffIDs`

**`TamingBuildPuzzle`** (`CDTamingBuildPuzzleTable.h` — `struct CDTamingBuildPuzzle`)
| Column | Type | Notes |
|--------|------|-------|
| `puzzleModelLot` | `LOT` | LOT of the model built during taming |
| `validPieces` | `std::string` | Path to .lxfml file with required bricks |
| `timeLimit` | `float` | Time limit for the build (seconds) |
| `numValidPieces` | `int32_t` | Number of bricks to place |
| `imaginationCost` | `int32_t` | Imagination cost to start taming |

### Component Type for Pets
Pets use `eReplicaComponentType::PET` (value 26) in `ComponentsRegistry`.

### Taming Mechanics
1. Player approaches wild pet, presses Use
2. `PetComponent::OnUse()` checks for `CDTamingBuildPuzzle` entry by LOT
3. Client receives `NotifyPetTamingMinigame` message, starts build minigame
4. Player builds model from `validPieces` within `timeLimit`
5. On success, `NotifyTamingBuildSuccess()` → `RequestSetPetName()` → pet added to inventory as `PET_INVENTORY_ITEM`
6. Pet can be spawned from inventory, follows owner and drains imagination at `imaginationDrainRate`

### Step-by-Step Addition Process

1. **Insert into `Objects`** with type `"Pet"`.

2. **Insert `PetComponent` row:**
   ```sql
   INSERT INTO PetComponent (id, walkSpeed, runSpeed, sprintSpeed, imaginationDrainRate)
   VALUES (7001, 3.0, 6.0, 10.0, 1.0);
   ```

3. **Insert `TamingBuildPuzzle` row** (keyed by LOT, not component ID):
   ```sql
   INSERT INTO TamingBuildPuzzle (puzzleModelLot, validPieces, timeLimit, numValidPieces, imaginationCost)
   VALUES (30030, 'res/BuildMechanics/validpieces/myPet.lxfml', 45.0, 6, 10);
   ```

4. **Register in `ComponentsRegistry`:**
   ```sql
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30030, 2, 0);     -- RENDER
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30030, 26, 7001); -- PET
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30030, 31, 7002); -- MOVEMENT_AI
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30030, 7, 7003);  -- DESTROYABLE
   ```

5. **Place in world** via spawner in LUZ file.

6. **Create the LXFML puzzle files** in `res/BuildMechanics/validpieces/`.

### Asset Requirements
- Pet NIF model — *Blender MCP (`13_CLAUDE_VS_MCP.md` §2.2).*
- Animation set (idle, walk, run, dig, play, etc.) — *Authored in the same Blender session.*
- `.lxfml` puzzle brick set for taming minigame — *XML format; Claude can author natively without an MCP.*
- Pet inventory item LOT (when tamed, stored as `PET_INVENTORY_ITEM` type) — *SQLite MCP for the inventory item row.*

### Testing Checklist
- [ ] `PetComponent` row exists with reasonable speed values
- [ ] `TamingBuildPuzzle` row references valid `.lxfml` files
- [ ] `ComponentsRegistry` has PET (26), MOVEMENT_AI (31), DESTROYABLE (7), RENDER (2)
- [ ] Pet appears in world via spawner
- [ ] Taming minigame launches on interaction
- [ ] After successful tame, pet appears in inventory

### Constraints & Validation Rules
- `CDTamingBuildPuzzleTable` is indexed by LOT (the pet's LOT), not by component ID
- `imaginationDrainRate` should be > 0 or pet never drains imagination
- `numValidPieces` must match the piece count in the `validPieces` LXFML file

### Common Pitfalls
- Using the component ID instead of the LOT as the key in `TamingBuildPuzzle`
- Missing `MOVEMENT_AI` component — pet won't move
- `validPieces` LXFML file path doesn't exist — taming crashes or fails silently

---

## 7. Zones/Levels

### Data Model

**`ZoneTable`** (`CDZoneTableTable.h` — `struct CDZoneTable`)
| Column | Type | Notes |
|--------|------|-------|
| `zoneID` | `uint32_t` | Zone ID (LWOMAPID) |
| `locStatus` | `uint32_t` | Localization status |
| `zoneName` | `std::string` | Internal name of the zone |
| `scriptID` | `uint32_t` | Zone-level script from ScriptsTable |
| `ghostdistance_min` | `float` | Minimum ghost/visibility distance |
| `ghostdistance` | `float` | Maximum ghost/visibility distance |
| `population_soft_cap` | `uint32_t` | Recommended max players |
| `population_hard_cap` | `uint32_t` | Absolute max players |
| `smashableMinDistance` | `float` | Min distance for smashable spawning |
| `smashableMaxDistance` | `float` | Max distance for smashable spawning |
| `serverPhysicsFramerate` | `std::string` | Physics update rate (e.g. `"30fps"`) |
| `zoneControlTemplate` | `uint32_t` | LOT of the zone control object |
| `widthInChunks` | `uint32_t` | World width in map chunks |
| `heightInChunks` | `uint32_t` | World height in map chunks |
| `petsAllowed` | `bool` | Pets allowed in this zone |
| `localize` | `bool` | Whether zone name should be localized |
| `fZoneWeight` | `float` | Zone weighting (used for load balancing?) |
| `PlayerLoseCoinsOnDeath` | `bool` | Coin loss on death |
| `disableSaveLoc` | `bool` | Prevents saving spawn location |
| `teamRadius` | `float` | Radius for team loot sharing |
| `mountsAllowed` | `bool` | Mounts allowed in this zone |

### LUZ File Relationship
The zone ID in `ZoneTable` must correspond to a `.luz` file on disk:
- Path: `res/maps/<zoneName>/` or as configured
- The LUZ file contains: scene references (`.lvl` files), spawner definitions with LOTs, path waypoints, and transition points
- The server parses LUZ files via `Zone` → `Level` classes at startup
- Spawners in LUZ files reference LOT IDs — those LOTs must exist in `CDObjects` + `CDComponentsRegistry`

### What Can Be Changed Without a New LUZ File
- `ZoneTable` metadata: `population_soft_cap`, `population_hard_cap`, `petsAllowed`, `mountsAllowed`, `PlayerLoseCoinsOnDeath`, `teamRadius`
- NPC/enemy stats via their component tables
- Loot tables for enemies in the zone
- Mission content

### What Requires a New or Modified LUZ File
- Adding new spawner positions
- Adding new objects to the scene
- Changing terrain, skybox, or level geometry
- Adding new paths/waypoints

### Step-by-Step Addition Process

1. **Create the LUZ and LVL files.** This is the one content task that has **no MCP path** and no public editor. The LUZ binary format references scene files; the LVL chunk format encodes objects/environment/particles (see `06_DATA_PERSISTENCE.md` §"LUZ Zone Files"). Practical workarounds:
   - **Reuse an existing zone's LUZ** and dynamically spawn objects from a zone script in `dScripts/zone/`. This is how the Racing and instanced-activity zones work today.
   - **Build a LUZ/LVL writer MCP** as a future contribution — the parser in `dZoneManager/Zone.cpp` and `Level.cpp` is the most complete open-source spec for these formats. Estimated effort 80-150 hours (see `13_CLAUDE_VS_MCP.md` §3.1, §5.7).

2. **Place LUZ file** in `res/maps/<zoneID>/`. The server expects the file at a path derived from the zone ID.

3. **Insert into `ZoneTable`:**
   ```sql
   INSERT INTO ZoneTable (zoneID, locStatus, zoneName, scriptID,
       ghostdistance_min, ghostdistance, population_soft_cap, population_hard_cap,
       smashableMinDistance, smashableMaxDistance, serverPhysicsFramerate,
       zoneControlTemplate, widthInChunks, heightInChunks,
       petsAllowed, localize, fZoneWeight, PlayerLoseCoinsOnDeath,
       disableSaveLoc, teamRadius, mountsAllowed)
   VALUES (1800, 0, 'MyCustomZone', 0,
       100.0, 500.0, 40, 60,
       30.0, 100.0, '30fps',
       2365, 64, 64,
       1, 1, 1.0, 1,
       0, 50.0, 1);
   ```

4. **Register the zone** in the master server's zone list so players can be sent there. This may require server configuration changes depending on how zone routing is handled.

5. **Add `PropertyTemplate` if the zone is a property zone** (see section 10).

### Code Integration Points
- `dZoneManager::LoadZone()` reads `CDZoneTableTable` to get zone metadata
- `Zone::Initialize()` loads the LUZ binary file from disk
- `WorldServer.cpp` accepts `-zone <id>` argument to launch a specific zone

### Asset Requirements
- `.luz` binary zone file — *no MCP path; see step 1 above.*
- `.lvl` scene files referenced by the LUZ — *no MCP path; same.*
- All asset NIFs referenced by scene objects — *Blender MCP can create the meshes themselves; the gap is placing them, which is the LUZ/LVL problem.*
- Terrain heightmap and texture assets — *image-gen + texconv MCP for textures; heightmap authoring requires Blender or dedicated terrain tools (drivable through Blender MCP for simple cases).*

### Testing Checklist
- [ ] `ZoneTable` row exists with correct `zoneID`
- [ ] LUZ file exists at the expected path on disk
- [ ] Server launches with `-zone <zoneID>` without crashes
- [ ] All spawner LOTs in the LUZ exist in `CDObjects`
- [ ] `zoneControlTemplate` LOT is valid
- [ ] Population caps are reasonable
- [ ] Physics framerate string is valid

### Common Pitfalls
- Zone ID in `ZoneTable` doesn't match the directory/file name expected by the server
- Spawner LOTs in LUZ file referencing LOTs not in CDObjects — server logs errors
- `zoneControlTemplate` LOT doesn't exist — zone control object fails to spawn

---

## 8. Vendors/Shops

### Data Model

**`VendorComponent`** (`CDVendorComponentTable.h` — `struct CDVendorComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `buyScalar` | `float` | Price multiplier for buying from vendor |
| `sellScalar` | `float` | Price multiplier for selling to vendor |
| `refreshTimeSeconds` | `float` | How often inventory refreshes (0 = never) |
| `LootMatrixIndex` | `uint32_t` | Matrix that defines what vendor sells |

The vendor's inventory is the **same loot matrix system** as enemy drops. Items in the loot matrix with `inVendor = 1` on their `ItemComponent` are available for purchase.

### Runtime: `VendorComponent` (C++ class, `dGame/dComponents/VendorComponent.h`)
- `m_BuyScalar`, `m_SellScalar`, `m_RefreshTimeSeconds` loaded from CDVendorComponent
- `m_LootMatrixID` → calls into loot system to build `m_Inventory` vector of `SoldItem` structs
- `VendorComponent::Buy()` handles the actual purchase transaction
- `SetupItem()` validates each item against preconditions

### Step-by-Step Addition Process

1. **Create an NPC object** (see NPC section) with the VENDOR (16) component type.

2. **Create items to sell** (see Items section), with `inVendor = 1`.

3. **Create a LootTable** for vendor inventory:
   ```sql
   INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
   VALUES (30001, 1600, 0, 1);
   INSERT INTO LootTable (itemid, LootTableIndex, MissionDrop, sortPriority)
   VALUES (30002, 1600, 0, 2);
   ```

4. **Create a LootMatrix** for the vendor:
   ```sql
   INSERT INTO LootMatrix (LootMatrixIndex, LootTableIndex, RarityTableIndex,
       percent, minToDrop, maxToDrop, flagID)
   VALUES (900, 1600, 1, 1.0, 1, 99, 0);
   ```
   Note: for vendors, `minToDrop`/`maxToDrop` don't apply the same way — the vendor shows all items.

5. **Create the VendorComponent row:**
   ```sql
   INSERT INTO VendorComponent (id, buyScalar, sellScalar, refreshTimeSeconds, LootMatrixIndex)
   VALUES (8001, 1.0, 0.5, 0.0, 900);
   ```

6. **Register VENDOR component in ComponentsRegistry:**
   ```sql
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30010, 16, 8001);
   ```

7. **For special pricing** (e.g. faction tokens): set `commendationLOT` and `commendationCost` on the `ItemComponent`. For crafting recipes: set `currencyCosts` field.

### Code Integration Points
- `VendorComponent::RefreshInventory()` — rebuilds the item list from the loot matrix
- `VendorComponent::Buy()` — deducts cost and grants item; reads `ItemComponent.baseValue * buyScalar`
- `VendorComponent::HandleMrReeCameras()` — special-case for a specific vendor

### Testing Checklist
- [ ] NPC LOT has VENDOR (16) component in `ComponentsRegistry`
- [ ] `VendorComponent` row exists with valid `LootMatrixIndex`
- [ ] `LootMatrix` and `LootTable` rows exist for the vendor's inventory
- [ ] All sold items have `inVendor = 1` in `ItemComponent`
- [ ] `buyScalar` and `sellScalar` are reasonable (1.0 = base price)
- [ ] Vendor dialog opens when interacting with the NPC
- [ ] Items display with correct prices

### Constraints & Validation Rules
- `buyScalar = 0` makes all items free — probably unintentional
- `refreshTimeSeconds = 0` means inventory never refreshes
- All items must have valid LOTs in `Objects` and `ItemComponent`

### Common Pitfalls
- Missing VENDOR component type in ComponentsRegistry for the NPC LOT
- `LootMatrixIndex` pointing to an empty or nonexistent matrix
- Items with `inVendor = 0` won't appear even if they're in the loot matrix

---

## 9. Quick Builds / Smashables

### Data Model

**`RebuildComponent`** (`CDRebuildComponentTable.h` — `struct CDRebuildComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `reset_time` | `float` | Seconds before built object resets/despawns |
| `complete_time` | `float` | Seconds to complete the build |
| `take_imagination` | `uint32_t` | Imagination cost to complete build |
| `interruptible` | `bool` | Can the build be interrupted? |
| `self_activator` | `bool` | Has its own activator (no separate activator spawned) |
| `custom_modules` | `std::string` | Custom module specification (rarely used) |
| `activityID` | `uint32_t` | Activity ID for quick build participation |
| `post_imagination_cost` | `uint32_t` | Imagination cost after build completes (unused) |
| `time_before_smash` | `float` | Seconds before incomplete QB is auto-smashed |

### Component Type
Quick builds use `eReplicaComponentType::QUICK_BUILD` (value 55) in `ComponentsRegistry`.

### How It Works
1. QB object spawns in world with activator (golden sparkle)
2. Player approaches and uses activator — `QuickBuildComponent::OnUse()`
3. Client enters build mode, imagination drains during `complete_time`
4. On completion: `CompleteQuickBuild()` fires callbacks, may spawn loot or trigger script
5. After `reset_time`, object despawns and respawns from spawner
6. If not completed within `time_before_smash`, auto-resets

For **smashables** (objects that just break and drop loot with no build mechanic), use the `DestructibleComponent` with `isSmashable = 1` and no `QUICK_BUILD` component.

### Step-by-Step Addition Process

1. **Insert into `Objects`.**

2. **Insert `RebuildComponent`:**
   ```sql
   INSERT INTO RebuildComponent (id, reset_time, complete_time, take_imagination,
       interruptible, self_activator, custom_modules, activityID,
       post_imagination_cost, time_before_smash)
   VALUES (9001, 30.0, 8.0, 6, 0, 0, '', 0, 0, 10.0);
   ```

3. **Add `DestructibleComponent`** (for loot on completion/smash):
   ```sql
   INSERT INTO DestructibleComponent (id, faction, life, imagination, LootMatrixIndex,
       CurrencyIndex, level, armor, isnpc, isSmashable)
   VALUES (9002, -1, 1, 0, 800, 0, 1, 0, 0, 1);
   ```

4. **Register in `ComponentsRegistry`:**
   ```sql
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30040, 2, 0);     -- RENDER
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30040, 3, 0);     -- SIMPLE_PHYSICS
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30040, 7, 9002);  -- DESTROYABLE
   INSERT INTO ComponentsRegistry (id, component_type, component_id) VALUES (30040, 55, 9001); -- QUICK_BUILD
   ```

5. **Place spawner in LUZ file** pointing to this LOT.

6. **Add mission task** if players must quick-build this object for a mission (task type 2 = ACTIVITY with the `activityID`).

### Code Integration Points
- `QuickBuildComponent` reads `CDRebuildComponent` via `GetByID()` on startup
- `QuickBuildComponent::CompleteQuickBuild()` fires `m_QuickBuildCompleteCallbacks` — scripts register here
- `ScriptComponent` Lua scripts can use `onQuickBuildComplete` callback

### Asset Requirements
- NIF model for both incomplete (scattered bricks) and complete states — *Blender MCP can author both as a pair in one session (`13_CLAUDE_VS_MCP.md` §2.2).*
- Activator visual effect asset — *typically reuses existing effect; if custom, Blender MCP for the mesh + image-gen MCP for the diffuse.*
- Animation for build completion — *authored in the Blender session.*

### Testing Checklist
- [ ] `RebuildComponent` row exists with reasonable times and imagination cost
- [ ] `DestructibleComponent` row exists if loot should drop
- [ ] `ComponentsRegistry` has QUICK_BUILD (55), RENDER (2), PHYSICS, and DESTROYABLE (7)
- [ ] Build completes successfully within `complete_time`
- [ ] Object resets after `reset_time` seconds
- [ ] Imagination is correctly deducted from player
- [ ] Mission task progresses if applicable

### Worked Example

A quick build that takes 6 imagination, 8 seconds to build, resets after 30 seconds:
```sql
INSERT INTO Objects (id, name, type) VALUES (30040, 'MyQuickBuild', 'Smashable');
INSERT INTO RebuildComponent (id, reset_time, complete_time, take_imagination,
    interruptible, self_activator, time_before_smash)
VALUES (9001, 30.0, 8.0, 6, 0, 0, 10.0);
INSERT INTO DestructibleComponent (id, faction, life, LootMatrixIndex, isSmashable)
VALUES (9002, -1, 1, 800, 1);
INSERT INTO ComponentsRegistry VALUES (30040, 2, 0);
INSERT INTO ComponentsRegistry VALUES (30040, 3, 0);
INSERT INTO ComponentsRegistry VALUES (30040, 7, 9002);
INSERT INTO ComponentsRegistry VALUES (30040, 55, 9001);
```

### Constraints & Validation Rules
- `take_imagination` must be >= 0; if player lacks imagination, QB cannot be completed
- `complete_time` should be > 0
- `reset_time = 0` means the object never resets after building
- `time_before_smash` should be less than `complete_time` would suggest an immediate smash — make it longer than `complete_time`

### Common Pitfalls
- Missing DESTROYABLE component — object cannot be smashed by enemies
- `self_activator = 0` but no separate activator spawner in LUZ — players can't start the build
- Setting `reset_time` very low causes the QB to immediately reset after completion, preventing any interaction with the built result

---

## 10. Properties/Housing

### Data Model

**`PropertyTemplate`** (`CDPropertyTemplateTable.h` — `struct CDPropertyTemplate`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Template ID |
| `mapID` | `uint32_t` | Zone ID of the property instance |
| `vendorMapID` | `uint32_t` | Zone ID where vendor/marketplace is located |
| `spawnName` | `std::string` | Name of spawn point in LUZ |

**`PropertyEntranceComponent`** (`CDPropertyEntranceComponentTable.h` — `struct CDPropertyEntranceComponent`)
| Column | Type | Notes |
|--------|------|-------|
| `id` | `uint32_t` | Component ID |
| `mapID` | `uint32_t` | Zone ID of property instances for this entrance |
| `propertyName` | `std::string` | Display name of the property type |
| `isOnProperty` | `bool` | Whether this entrance is on a property |
| `groupType` | `std::string` | Group/type identifier |

### How Properties Work
1. A zone has a `PropertyEntranceComponent` NPC/object that opens the property portal
2. The `CDPropertyTemplate` links a property `mapID` to its template and vendor zone
3. Players claim a property via the `PropertyManagementComponent`
4. Property data (model placements, etc.) is stored in the game database (`Properties` table, not CDClient)

### Server-Side Property Tables (game database, not CDClient)
The game database (not CDClient) stores:
- `Properties` — player-claimed property instances (character, zone, clone ID)
- `UGCData` — uploaded model data
- `PropertyContent` — what models are placed on a property

### Step-by-Step Addition Process

1. **Create the property zone** (see Zones section, zone ID e.g. 1300).

2. **Create the LUZ file** for the property zone, including a named spawn point matching `spawnName`.

3. **Insert into `PropertyTemplate`:**
   ```sql
   INSERT INTO PropertyTemplate (id, mapID, vendorMapID, spawnName)
   VALUES (100, 1300, 1200, 'Player_Prop_Spawn');
   ```

4. **Create a property entrance NPC** in the world zone, with `PROPERTY_ENTRANCE` component (type 43).

5. **Insert `PropertyEntranceComponent` row:**
   ```sql
   INSERT INTO PropertyEntranceComponent (id, mapID, propertyName, isOnProperty, groupType)
   VALUES (10001, 1300, 'My Property', 0, 'standard');
   ```

6. **Register in `ComponentsRegistry`:**
   ```sql
   INSERT INTO ComponentsRegistry (id, component_type, component_id)
   VALUES (30050, 43, 10001); -- PROPERTY_ENTRANCE
   ```

### Code Integration Points
- `PropertyEntranceComponent` (C++ class) handles the portal interaction
- `PropertyManagementComponent` manages model placement, claiming, renting
- `PropertyComponent` (C++ class, currently empty/unused) is on property zone control objects
- Game database tables `Properties` and `PropertyContent` store instance data

### Testing Checklist
- [ ] `PropertyTemplate` row exists with valid `mapID`
- [ ] Property zone LUZ file exists with matching `spawnName` spawn point
- [ ] Property entrance NPC has `PROPERTY_ENTRANCE` component in registry
- [ ] `PropertyEntranceComponent` row exists for the entrance NPC
- [ ] Players can enter the portal and land in their property instance
- [ ] Models can be placed and persist between sessions

### Constraints & Validation Rules
- `mapID` in `PropertyTemplate` must reference a valid zone in `ZoneTable`
- `spawnName` must exactly match a spawn point name in the LUZ file
- Property zone must support instancing (clone system)

### Common Pitfalls
- `spawnName` typo — players spawn at wrong location or spawn fails
- Property zone not registered for instancing — all players share one instance
- Missing `PropertyManagementComponent` on the zone control object — model placement broken
