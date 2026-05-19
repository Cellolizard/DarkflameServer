# DarkflameServer Content Dependencies

This document maps every significant foreign-key relationship across the CDClient tables that
DarkflameServer reads at startup. Each section gives the exact column names (drawn from the C++
header structs), notes whether the link is validated at load-time or only at runtime, and explains
the practical impact of a broken reference.

---

## Overview

All in-game content is rooted in a 32-bit integer called a **LOT** (Lego Object Template). The LOT
is the primary key of `CDObjects` and the foreign key used everywhere else. The lookup pattern is:

```
LOT
 └─ CDObjects.id                        -- human-readable name, type string
      └─ CDComponentsRegistry.id        -- one row per component attached to this LOT
           ├─ component_type            -- eReplicaComponentType integer value
           └─ component_id              -- primary key in the matching component table
```

From that root, references fan out to skills, behaviors, loot, missions, zones, and more. None of
the links are enforced by foreign-key constraints in the SQLite CDClient database — they are soft
references resolved at runtime by C++ code that queries the in-memory tables loaded at startup.

---

## Dependency Reference Table

| # | Source Content | Source Table | Source Column | References | Target Table | Target Column | Impact if Missing |
|---|---|---|---|---|---|---|---|
| 1 | Object definition | `CDComponentsRegistry` | `id` (LOT) | object metadata | `CDObjects` | `id` | Object has no name/type; some systems skip it |
| 2 | Component registration | `CDComponentsRegistry` | `component_id` | item data | `CDItemComponentTable` | `id` | Item component silently returns default; no equip location |
| 3 | Component registration | `CDComponentsRegistry` | `component_id` | NPC/enemy stats | `CDDestructibleComponentTable` | `id` | Entity has no HP/armor/loot; cannot die normally |
| 4 | Component registration | `CDComponentsRegistry` | `component_id` | rebuild puzzle | `CDRebuildComponentTable` | `id` | Quick-build object does nothing when approached |
| 5 | Component registration | `CDComponentsRegistry` | `component_id` | vendor inventory | `CDVendorComponentTable` | `id` | Vendor NPC has empty shop |
| 6 | Component registration | `CDComponentsRegistry` | `component_id` | NPC mission list | `CDMissionNPCComponentTable` | `id` | NPC offers/accepts no missions |
| 7 | Component registration | `CDComponentsRegistry` | `component_id` | pet behavior | `CDPetComponentTable` | `id` | Pet spawns but cannot be tamed or controlled |
| 8 | Component registration | `CDComponentsRegistry` | `component_id` | item set skills | `CDInventoryComponentTable` | `id` | NPC spawn inventory is empty |
| 9 | Component registration | `CDComponentsRegistry` | `component_id` | package loot | `CDPackageComponentTable` | `id` | Package item gives no loot on open |
| 10 | Component registration | `CDComponentsRegistry` | `component_id` | property entrance | `CDPropertyEntranceComponentTable` | `id` | Entrance object does not route player to property |
| 11 | Component registration | `CDComponentsRegistry` | `component_id` | script | `CDScriptComponentTable` | `id` | Object has no server-side script; behavior is inert |
| 12 | Item equip skill | `CDObjectSkillsTable` | `objectTemplate` (LOT) | skill data | `CDSkillBehaviorTable` | `skillID` | Skill cast produces no behavior; silent failure |
| 13 | Skill execution | `CDSkillBehaviorTable` | `behaviorID` | behavior template | `CDBehaviorTemplateTable` | `behaviorID` | Skill fires but no behavior tree executes |
| 14 | Behavior parameters | `CDBehaviorParameterTable` | (behaviorID hash key) | behavior values | `CDBehaviorTemplateTable` | `behaviorID` | Behavior uses default 0 values; may deal no damage |
| 15 | Mission giver | `CDMissionsTable` | `offer_objectID` | NPC object | `CDObjects` | `id` | Mission has no giver; cannot be offered to player |
| 16 | Mission turn-in | `CDMissionsTable` | `target_objectID` | NPC object | `CDObjects` | `id` | Mission cannot be completed; no turn-in NPC |
| 17 | Mission reward items | `CDMissionsTable` | `reward_item1..4` | item LOT | `CDObjects` | `id` | Reward slot gives nothing; no crash |
| 18 | Repeatable reward items | `CDMissionsTable` | `reward_item1..4_repeatable` | item LOT | `CDObjects` | `id` | Repeat reward gives nothing on subsequent completions |
| 19 | Mission task target | `CDMissionTasksTable` | `target` | LOT or skill | `CDObjects` / `CDSkillBehaviorTable` | `id` / `skillID` | Task progress never counts; mission uncompletable |
| 20 | Mission task group | `CDMissionTasksTable` | `targetGroup` | comma-separated LOTs | `CDObjects` | `id` | Tasks counting group members fail silently |
| 21 | Loot chain start | `CDDestructibleComponentTable` | `LootMatrixIndex` | loot matrix | `CDLootMatrixTable` | `LootTableIndex` (key) | Enemy drops no loot on death |
| 22 | Loot table rows | `CDLootMatrixTable` | `LootTableIndex` | loot table | `CDLootTableTable` | `LootTableIndex` (key) | Loot matrix resolves to empty table |
| 23 | Loot item | `CDLootTableTable` | `itemid` | item object | `CDObjects` | `id` | Loot slot ignored; item never spawned |
| 24 | Loot rarity | `CDLootMatrixTable` | `RarityTableIndex` | rarity thresholds | `CDRarityTableTable` | index key | All drops treated as equal probability |
| 25 | Vendor items | `CDVendorComponentTable` | `LootMatrixIndex` | loot matrix | `CDLootMatrixTable` | key | Vendor has no listed items |
| 26 | Package loot | `CDPackageComponentTable` | `LootMatrixIndex` | loot matrix | `CDLootMatrixTable` | key | Package gives nothing when opened |
| 27 | Sub-items | `CDItemComponentTable` | `subItems` (CSV LOTs) | item objects | `CDObjects` | `id` | Missing sub-item silently skipped |
| 28 | Commendation currency | `CDItemComponentTable` | `commendationLOT` | currency item | `CDObjects` | `id` | Commendation purchase fails |
| 29 | Mission NPC link | `CDMissionNPCComponentTable` | `missionID` | mission definition | `CDMissionsTable` | `id` | NPC component exists but mission data missing; may crash |
| 30 | Quick-build activity | `CDRebuildComponentTable` | `activityID` | activity definition | `CDActivitiesTable` | `ActivityID` | Activity rewards/leaderboard unavailable |
| 31 | Activity instance zone | `CDActivitiesTable` | `instanceMapID` | zone entry | `CDZoneTableTable` | `zoneID` | Activity cannot load its instance world |
| 32 | Zone control object | `CDZoneTableTable` | `zoneControlTemplate` | LOT | `CDObjects` | `id` | Zone-level behavior scripts do not run |
| 33 | Zone script | `CDZoneTableTable` | `scriptID` | script row | `CDScriptComponentTable` | `id` | Zone has no server script |
| 34 | Property template zone | `CDPropertyTemplateTable` | `mapID` | zone | `CDZoneTableTable` | `zoneID` | Property world cannot be loaded |
| 35 | Property vendor map | `CDPropertyTemplateTable` | `vendorMapID` | zone | `CDZoneTableTable` | `zoneID` | Property vendor world inaccessible |
| 36 | Property entrance target | `CDPropertyEntranceComponentTable` | `mapID` | zone | `CDZoneTableTable` | `zoneID` | Entrance teleports nowhere |
| 37 | Item set members | `CDItemSetsTable` | `itemIDs` (CSV LOTs) | item objects | `CDObjects` | `id` | Set bonus does not count missing items |
| 38 | Item set skill bonuses | `CDItemSetsTable` | `skillSetWith2..6` | skill set | `CDItemSetSkillsTable` | `SkillSetID` | Bonus skill not granted when set completed |
| 39 | Item set skill link | `CDItemSetSkillsTable` | `SkillID` | skill data | `CDSkillBehaviorTable` | `skillID` | Set bonus fires but no behavior executes |
| 40 | Pet taming puzzle | `CDTamingBuildPuzzleTable` | `puzzleModelLot` | object LOT | `CDObjects` | `id` | Taming build model is missing; puzzle fails to start |
| 41 | Death behavior | `CDDestructibleComponentTable` | `death_behavior` | behavior tree | `CDBehaviorTemplateTable` | `behaviorID` | Death sequence uses no behavior |
| 42 | Object skill (AI) | `CDObjectSkillsTable` | `skillID` | skill entry | `CDSkillBehaviorTable` | `skillID` | AI combat uses no skill; enemy cannot attack |

---

## Full Dependency Graph (ASCII)

```
[LOT / CDObjects]  (CDObjectsTable — id, name, type, interactionDistance)
    │
    ├─ CDComponentsRegistry (id=LOT → component_type, component_id)
    │   │
    │   ├─ component_type=2  → RenderComponent     (renderAssetID → .nif model file on disk)
    │   │
    │   ├─ component_type=3  → CDPhysicsComponent  (physics shape / hull reference)
    │   │
    │   ├─ component_type=5  → CDScriptComponentTable
    │   │                          └─ script_name  → .lua server script on disk
    │   │
    │   ├─ component_type=7  → CDDestructibleComponentTable
    │   │                          ├─ LootMatrixIndex → CDLootMatrixTable
    │   │                          │       └─ LootTableIndex → CDLootTableTable
    │   │                          │               └─ itemid → CDObjects (item LOT)
    │   │                          ├─ RarityTableIndex → CDRarityTableTable
    │   │                          ├─ CurrencyIndex   → CDCurrencyTableTable
    │   │                          └─ death_behavior  → CDBehaviorTemplateTable
    │   │                                  └─ templateID (behavior type integer)
    │   │                                  └─ CDBehaviorParameterTable (behaviorID → params)
    │   │
    │   ├─ component_type=9  → (CDObjectSkillsTable keyed by objectTemplate=LOT)
    │   │                          └─ skillID → CDSkillBehaviorTable
    │   │                                  ├─ behaviorID → CDBehaviorTemplateTable
    │   │                                  │       └─ CDBehaviorParameterTable
    │   │                                  │           (sub-behaviors via action/behavior params)
    │   │                                  └─ imaginationcost, cooldown, cooldowngroup
    │   │
    │   ├─ component_type=11 → CDItemComponentTable
    │   │                          ├─ equipLocation   (string slot name)
    │   │                          ├─ itemType        → eItemType enum
    │   │                          ├─ subItems        → CDObjects (comma-sep LOTs)
    │   │                          ├─ commendationLOT → CDObjects (currency LOT)
    │   │                          ├─ reqFlagID       → player flag integer
    │   │                          └─ reqAchievementID → mission/achievement ID
    │   │   (skills on items are in CDObjectSkillsTable, not ItemComponentTable)
    │   │
    │   ├─ component_type=12 → CDRebuildComponentTable (Quick Build / Rebuild)
    │   │                          └─ activityID → CDActivitiesTable
    │   │                                  └─ instanceMapID → CDZoneTableTable
    │   │                                          └─ zoneID → .luz file on disk
    │   │
    │   ├─ component_type=16 → CDVendorComponentTable
    │   │                          └─ LootMatrixIndex → CDLootMatrixTable
    │   │                                  └─ (see loot chain above)
    │   │
    │   ├─ component_type=17 → CDInventoryComponentTable
    │   │                          └─ itemid → CDObjects (item LOT pre-given to NPC)
    │   │
    │   ├─ component_type=26 → CDPetComponentTable
    │   │                          ├─ walkSpeed, runSpeed, sprintSpeed
    │   │                          └─ imaginationDrainRate
    │   │   (pet item link: item LOT with component_type=11 AND component_type=26)
    │   │   (taming puzzle: CDTamingBuildPuzzleTable keyed by NPC LOT)
    │   │       └─ puzzleModelLot → CDObjects
    │   │
    │   ├─ component_type=31 → CDMovementAIComponentTable
    │   │                          (wander speed, roam radius, AI style)
    │   │
    │   ├─ component_type=39 → CDScriptedActivityComponentTable
    │   │                          └─ activityID → CDActivitiesTable
    │   │
    │   ├─ component_type=43 → CDPropertyEntranceComponentTable
    │   │                          └─ mapID → CDZoneTableTable
    │   │
    │   ├─ component_type=55 → CDRebuildComponentTable (QUICK_BUILD alias)
    │   │                          └─ (same as component_type=12)
    │   │
    │   ├─ component_type=60 → CDMissionNPCComponentTable
    │   │                          └─ missionID → CDMissionsTable
    │   │
    │   └─ component_type=73 → (CDObjectSkillsTable for AI combat skills)
    │                              └─ skillID → CDSkillBehaviorTable
    │                                      └─ behaviorID → CDBehaviorTemplateTable
    │
    └─ (CDObjectSkillsTable.objectTemplate = LOT)
           └─ skillID → CDSkillBehaviorTable (see skill chain above)

[CDMissions]  (CDMissionsTable — id, defined_type, defined_subtype)
    ├─ offer_objectID  → CDObjects (NPC that gives the mission)
    ├─ target_objectID → CDObjects (NPC that accepts completion)
    ├─ reward_item1    → CDObjects (item LOT)
    ├─ reward_item2    → CDObjects (item LOT)
    ├─ reward_item3    → CDObjects (item LOT)
    ├─ reward_item4    → CDObjects (item LOT)
    ├─ reward_item1_repeatable → CDObjects
    ├─ reward_item2_repeatable → CDObjects
    ├─ reward_item3_repeatable → CDObjects
    ├─ reward_item4_repeatable → CDObjects
    ├─ prereqMissionID → CDMissionsTable (pipe-separated mission IDs)
    └─ CDMissionTasks (one-to-many: CDMissionTasks.id = CDMissions.id)
           ├─ taskType  (integer — determines how target is interpreted)
           │     0 = Unknown
           │     2 = Smash (target = enemy LOT)
           │     4 = Collect (target = item LOT)
           │     8 = GoTo (target = zone/LOT)
           │    10 = UseEmote
           │    11 = UseConsumable (target = item LOT)
           │    14 = UseSkill (target = skillID)
           │    22 = ObtainItem (target = item LOT)
           │    23 = Discover (target = object LOT)
           ├─ target      → CDObjects.id  OR  CDSkillBehaviorTable.skillID (depends on taskType)
           ├─ targetGroup → comma-separated LOTs → CDObjects.id
           └─ taskParam1  → zone ID or LOT depending on taskType

[CDZone / CDZoneTableTable]
    ├─ zoneID            → .luz file on disk (res/maps/<zoneName>/<zoneID>/)
    │                           └─ spawner objects in LUZ → CDObjects (spawned LOTs)
    ├─ scriptID          → CDScriptComponentTable.id
    ├─ zoneControlTemplate → CDObjects.id (zone controller LOT)
    └─ (CDPropertyTemplateTable.mapID → CDZoneTableTable.zoneID)
           └─ vendorMapID → CDZoneTableTable.zoneID

[CDItemSets]  (CDItemSetsTable)
    ├─ itemIDs (CSV)     → CDObjects.id (member item LOTs)
    └─ skillSetWith2..6  → CDItemSetSkillsTable.SkillSetID
                               └─ SkillID → CDSkillBehaviorTable.skillID
                                       └─ behaviorID → CDBehaviorTemplateTable

[Behavior Tree]  (CDBehaviorTemplateTable + CDBehaviorParameterTable)
    CDBehaviorTemplateTable:
        ├─ behaviorID   (primary key, referenced from CDSkillBehaviorTable.behaviorID)
        ├─ templateID   (behavior type integer — maps to C++ BehaviorTemplates enum)
        └─ effectID     → VisEffect table (client-only visual)
    CDBehaviorParameterTable:
        ├─ (behaviorID + parameterName) → float value
        └─ Common sub-behavior params: "action", "behavior 1", "behavior 2",
           "on_success", "on_fail", "chain_delay" → all resolve to child behaviorID
           (recursive: a behavior tree is a tree of behaviorID references)
```

---

## Per-Dependency Details

### 1. Object (LOT) → ComponentsRegistry → All Component Tables

**Relationship name:** LOT Registration  
**How stored:** `CDComponentsRegistry.id` = LOT, `component_type` = `eReplicaComponentType` integer,
`component_id` = row PK in target table. Multiple rows per LOT (one per component type attached).  
**Validation:** Loaded entirely into an `unordered_map<uint64_t, uint32_t>` at startup keyed by
`(LOT << 32 | component_type)`. Presence of the component table row is NOT verified at load — the
lookup returns 0 (default) for missing entries.  
**Impact of removing target:** The component C++ class constructor receives `component_id = 0`.
Most components use 0 as "use defaults" for simple cases, but structured components like
`CDItemComponent` return a static `Default` object with zeroed fields, which can cause items to have
no equip slot, zero value, and incorrect behavior.  
**Cascade considerations:** Every LOT must have at minimum a RENDER component (type 2) entry or it
will be invisible to clients. Removing `CDObjects` row for a LOT makes the object name/type
unavailable — several gameplay systems key off the `type` string.

---

### 2. Item → Skill → Behavior

**Relationship name:** Item Skill Chain  
**How stored:**
- `CDObjectSkillsTable.objectTemplate` = LOT of the item
- `CDObjectSkillsTable.skillID` → `CDSkillBehaviorTable.skillID`
- `CDSkillBehaviorTable.behaviorID` → `CDBehaviorTemplateTable.behaviorID`
- `CDBehaviorParameterTable` stores float parameters keyed by `(behaviorID, parameterName)` hash

**Validation:** `CDSkillBehaviorTable` returns a zeroed default struct if `skillID` is missing.
`CDBehaviorTemplateTable::GetByBehaviorID` returns a zeroed struct for unknown IDs. Neither check
causes a crash at load — failures surface only when the skill is cast.  
**Impact of removing target:**
- Missing `CDSkillBehaviorTable` row: skill cast silently does nothing (no behavior ID).
- Missing `CDBehaviorTemplateTable` row: the behavior type resolves to 0 (INVALID), behavior
  executes as a no-op.
- Missing `CDBehaviorParameterTable` rows: behavior values default to 0 — a damage behavior deals
  0 damage, a heal heals 0, a projectile has 0 speed.

**Cascade considerations:** Sub-behaviors referenced by parameters like `"action"`, `"behavior 1"`,
`"on_success"` are also looked up in `CDBehaviorTemplateTable`. Removing any node in the tree
silently truncates that branch.

---

### 3. Mission → NPC Giver/Turn-in

**Relationship name:** Mission NPC Assignment  
**How stored:**
- `CDMissionsTable.offer_objectID` = LOT of the NPC offering the mission
- `CDMissionsTable.target_objectID` = LOT of the NPC accepting turn-in
- `CDMissionNPCComponentTable.missionID` = mission ID, linked to object via `CDComponentsRegistry`
  component_type=60

**Validation:** Not validated at load. The mission system queries `CDMissionNPCComponentTable` when
a player interacts with an NPC to populate its mission list. If `offer_objectID` points to a missing
LOT, the mission simply never appears in any NPC's dialog because no live object with that LOT
exists in any zone.  
**Impact of removing target:** Mission becomes permanently inaccessible to players. No crash.  
**Cascade considerations:** Both `offer_objectID` and `target_objectID` must point to objects that
are actually placed in a loaded zone via a spawner in the .luz file, *and* have
`CDMissionNPCComponentTable` rows listing the mission ID with `offersMission=true` /
`acceptsMission=true`.

---

### 4. Mission → Reward Items

**Relationship name:** Mission Reward LOT  
**How stored:** `CDMissionsTable.reward_item1`, `reward_item2`, `reward_item3`, `reward_item4`
(and repeatable variants) — all are `int32_t` LOTs. Value 0 means no reward in that slot.  
**Validation:** Not validated at load or at mission completion time beyond a null/zero check.  
**Impact of removing target:** If the reward LOT is nonzero but missing from `CDObjects` or
`CDComponentsRegistry`, the server attempts to create the item and typically finds no ItemComponent
data; the item may be created with an invalid equip location or not created at all (implementation
dependent). No crash, but the reward is lost.  
**Cascade considerations:** Reward items must also have valid `CDItemComponentTable` rows with
proper `itemType` and `stackSize` values or inventory insertion logic may behave incorrectly.

---

### 5. MissionTask → Target LOT / Target Skill

**Relationship name:** Mission Task Target  
**How stored:**
- `CDMissionTasksTable.target` — integer whose meaning depends on `taskType`
- `CDMissionTasksTable.targetGroup` — comma-separated string of LOTs or IDs
- `CDMissionTasksTable.taskParam1` — secondary target or zone ID

**Validation:** Not validated. The mission task system compares runtime events (smash, collect, use
skill) against `target` / `targetGroup` values. If a referenced LOT no longer exists in CDObjects,
the comparison simply never matches — the task can never complete.  
**Impact of removing target:** Mission task is permanently stuck at 0 progress. The mission becomes
uncompletable.  
**Cascade considerations:** `taskType=14` (UseSkill) stores a `skillID` in `target`. If that
`skillID` is removed from `CDSkillBehaviorTable`, the task never fires even if the player casts a
skill. Ensure both the task target *and* any corresponding item/skill entries remain consistent.

---

### 6. LootMatrix → LootTable → Item LOT

**Relationship name:** Loot Drop Chain  
**How stored:**
- Enemy / vendor / package holds an integer `LootMatrixIndex`
- `CDLootMatrixTable` rows are grouped by `LootMatrixIndex`; each row holds `LootTableIndex`,
  `RarityTableIndex`, `percent`, `minToDrop`, `maxToDrop`
- `CDLootTableTable` rows grouped by `LootTableIndex`; each row holds `itemid` (LOT),
  `MissionDrop`, `sortPriority`

**Validation:** `CDLootMatrixTable::GetMatrix` and `CDLootTableTable::GetTable` both return
empty vectors for unknown index values rather than crashing.  
**Impact of removing target:**
- Missing `LootMatrixIndex` from the matrix table: no loot drops at all from that source.
- Missing `LootTableIndex`: that matrix slot drops nothing.
- Missing `itemid` in `CDObjects`: item is not instantiated; loot slot lost silently.

**Cascade considerations:** `CDLootMatrixTable.RarityTableIndex` must match a key in
`CDRarityTableTable` or all items roll at equal probability. `flagID` in loot matrix rows controls
whether a player who already has a certain flag can receive that loot — verify flag integers are
valid.

---

### 7. Zone → LUZ File → Spawner LOTs

**Relationship name:** Zone Content Pipeline  
**How stored:**
- `CDZoneTableTable.zoneID` — integer zone identifier
- `CDZoneTableTable.zoneName` — string used to locate the .luz file under `res/maps/`
- `.luz` file contains spawner objects, each with a LOT that is spawned into the world

**Validation:** The zone table entry is validated at world-load time — if `zoneID` is missing from
the table, the world server cannot start for that zone. The .luz file path is constructed from
`zoneName`; a mismatch causes a file-not-found error and zone load failure (hard error, not silent).
Spawner LOTs inside the .luz are looked up against `CDObjects` at entity creation time — missing
LOTs produce a warning log and the spawner is skipped.  
**Impact of removing target:**
- Missing zone table row: world server for that zone refuses to start.
- Missing .luz file: same.
- Missing spawner LOT in CDObjects: that spawner is inactive; NPCs, enemies, or interactables
  defined by it will not appear in the zone.

**Cascade considerations:** Zone scripts (`CDZoneTableTable.scriptID`) and zone control objects
(`zoneControlTemplate`) must also be valid. Zone transitions (via `CDPropertyEntranceComponentTable`
or `CDActivitiesTable.instanceMapID`) that reference a removed zone will fail at runtime when a
player attempts the transition.

---

### 8. Vendor → Item List (LOTs)

**Relationship name:** Vendor Inventory  
**How stored:**
- `CDVendorComponentTable.LootMatrixIndex` — integer pointing into `CDLootMatrixTable`
- The loot matrix rows and loot table rows resolve to item LOTs (see chain 6 above)
- `CDItemComponentTable.inVendor = true` on each item is a flag but is not re-checked at vendor
  query time — the loot matrix is the authoritative source

**Validation:** No validation at load. The vendor component queries its loot matrix when the shop
UI is requested.  
**Impact of removing target:** Vendor shows an empty shop if the LootMatrixIndex is invalid. Items
in the matrix that lack valid `CDItemComponentTable` rows are returned to the client with missing
metadata (no equip location, no icon), which can confuse the UI.  
**Cascade considerations:** Vendor refreshes (`refreshTimeSeconds`) periodically re-roll available
stock from the loot matrix. Any items added to the matrix must have valid CDObjects and
CDItemComponentTable entries before a refresh cycle fires.

---

### 9. Skill → Behavior ID → C++ Behavior Class

**Relationship name:** Skill-to-Behavior Dispatch  
**How stored:**
- `CDSkillBehaviorTable.behaviorID` — integer referencing a behavior tree root
- `CDBehaviorTemplateTable.templateID` — integer mapped to a C++ `BehaviorTemplates` enum value
- The server instantiates a C++ `Behavior` subclass based on `templateID`

**Validation:** At skill cast time, `CDBehaviorTemplateTable::GetByBehaviorID` is called. If the
behavior ID is absent, a zeroed struct with `templateID=0` is returned, which maps to
`BehaviorTemplates::EMPTY_BEHAVIOR` — a no-op. No crash, but the skill does nothing.  
**Impact of removing target:** Skill fires but has zero effect. If a weapon's primary skill is
broken this way, the weapon becomes non-functional.  
**Cascade considerations:** Behavior `templateID` values must map to registered C++ behavior
classes. Adding a new template ID requires a C++ code change to register the class in
`Behavior::CreateBehavior()`. No amount of CDClient data changes alone can introduce a new behavior
type.

---

### 10. Behavior → Sub-behaviors (Tree Structure)

**Relationship name:** Behavior Tree Recursion  
**How stored:**
- `CDBehaviorParameterTable` stores float values; sub-behavior links are parameters whose values
  are integer behavior IDs cast to float
- Common parameter names that hold child behavior IDs: `"action"`, `"behavior 1"`, `"behavior 2"`,
  `"on_success"`, `"on_fail"`, `"blocked_behavior"`, `"miss_behavior"`, `"chain_delay"`

**Validation:** None. Child behavior IDs are resolved at cast time using the same
`GetByBehaviorID` lookup. Missing children are no-ops.  
**Impact of removing target:** That branch of the behavior tree becomes inactive. For example,
removing a child `"on_success"` behavior means a hit does not trigger the follow-up effect.  
**Circular dependency risk:** The behavior parameter system does not detect cycles. A behavior that
references itself (directly or transitively) via a parameter would cause infinite recursion at
runtime. See the Circular Dependency section below.

---

### 11. Pet → Item (What Makes an Item a Pet)

**Relationship name:** Pet Item Duality  
**How stored:** A pet is a LOT that has *both* component_type=11 (ITEM) and component_type=26 (PET)
entries in `CDComponentsRegistry`. The ItemComponent row gives inventory/equip data; the
PetComponent row gives taming/movement stats.  
Additionally, `CDTamingBuildPuzzleTable` stores a row keyed by the *NPC pet LOT* (not the item
LOT) with `puzzleModelLot` pointing to the build model LOT.  
**Validation:** The pet system checks for both component types at taming time. Missing either
causes the pet to either not drop as an item or not be tameable respectively.  
**Impact of removing target:**
- Missing PET component: egg item drops, but right-clicking it to start taming fails.
- Missing ITEM component: pet NPC exists in world but cannot become a carried egg item.
- Missing `CDTamingBuildPuzzleTable` row: taming interaction starts but puzzle cannot load.
- Missing `puzzleModelLot` in CDObjects: puzzle model fails to spawn.

---

### 12. QuickBuild → Activity

**Relationship name:** Rebuild Activity Link  
**How stored:** `CDRebuildComponentTable.activityID` → `CDActivitiesTable.ActivityID`  
**Validation:** `CDActivitiesTable::GetActivity` returns an empty optional for unknown IDs. The
rebuild component proceeds without activity data (no leaderboard entry, no activity rewards).  
**Impact of removing target:** Quick-build completes normally but does not register an activity
score or grant activity rewards. In minigame contexts where the rebuild is the game entry point,
the instance map cannot be looked up (`CDActivitiesTable.instanceMapID`) and the door to the
minigame world fails to open.  
**Cascade considerations:** Activity leaderboards (`CDActivitiesTable.leaderboardType`) and
optional entry costs (`optionalCostLOT`, `optionalCostCount`) are only meaningful if the
CDActivitiesTable row exists and the referenced LOT is valid.

---

### 13. Property → Zone

**Relationship name:** Property World Mapping  
**How stored:**
- `CDPropertyTemplateTable.mapID` → `CDZoneTableTable.zoneID`
- `CDPropertyTemplateTable.vendorMapID` → `CDZoneTableTable.zoneID`
- `CDPropertyEntranceComponentTable.mapID` → `CDZoneTableTable.zoneID`

**Validation:** The property entrance component resolves its target zone at player interaction time.
A missing `CDZoneTableTable` row causes the zone transfer to fail with a runtime error.  
**Impact of removing target:** Players cannot enter the property world. The entrance object appears
but teleportation fails.  
**Cascade considerations:** The property template row's `spawnName` string must match a spawner
object name in the .luz file of the target zone, or the player spawn position defaults to origin.

---

## Circular Dependency Risks

### Behavior Trees

**Risk: Yes, possible.** Behavior parameters store child behavior IDs as float values in
`CDBehaviorParameterTable`. Nothing in the data model or server code prevents behavior A from
listing behavior A (or a cycle A→B→A) as a child parameter. If this occurs, `Behavior::Execute`
will recurse until the call stack overflows, causing a crash.

**Mitigation:** When authoring behavior trees, ensure all child parameter references form a
directed acyclic graph (DAG). The server provides no cycle detection. Validate by tracing the tree
manually before inserting into CDClient.

### Sub-items

**Risk: Low but possible.** `CDItemComponentTable.subItems` is a comma-separated string of LOTs.
If item A lists item A as a sub-item (or A→B→A), and the game attempts to recursively hand out
sub-items, this could loop. In practice the server iterates sub-items shallowly (not recursively),
so a direct self-reference causes item A to be given again rather than infinite loop — but it does
produce unexpected duplicate item grants.

**Mitigation:** Sub-item LOTs should always point to *different* LOTs that are not themselves
items with sub-item lists.

### Mission Prerequisites

**Risk: Low.** `CDMissionsTable.prereqMissionID` is a pipe-separated list of mission IDs that must
be complete before this mission unlocks. A direct cycle (mission A requires mission B, mission B
requires mission A) means neither mission is ever unlockable. The server does not detect this; both
missions remain permanently locked.

**Mitigation:** Prerequisites must form a DAG. Avoid bidirectional prerequisite links.

### Mission Task Targets

**Risk: None known.** Mission tasks reference external LOTs/skills as targets; there is no mechanism
for a mission task to directly reference its own mission ID in a way that creates a loop.

---

## Safe Deletion Checklist

Before removing any LOT or content row, verify there are no remaining references:

**For any LOT (CDObjects row):**
- [ ] Search `CDComponentsRegistry` for rows where `id` = this LOT — remove all component entries
- [ ] Search `CDMissionsTable.offer_objectID` and `target_objectID` — update affected missions
- [ ] Search `CDMissionsTable.reward_item1..4` and `reward_item1..4_repeatable` — remove or replace rewards
- [ ] Search `CDMissionTasksTable.target` — tasks targeting this LOT will be permanently broken
- [ ] Search `CDMissionTasksTable.targetGroup` (CSV parse) — same concern
- [ ] Search `CDLootTableTable.itemid` — remove loot entries referencing this LOT
- [ ] Search `CDInventoryComponentTable.itemid` — remove NPC pre-inventory entries
- [ ] Search `CDItemComponentTable.subItems` (CSV parse) — remove this LOT from any parent's sub-item list
- [ ] Search `CDItemComponentTable.commendationLOT` — update items using this as currency
- [ ] Search `CDItemSetsTable.itemIDs` (CSV parse) — remove from any item set definitions
- [ ] Search `CDObjectSkillsTable.objectTemplate` — remove skill rows linked to this LOT
- [ ] Search `CDTamingBuildPuzzleTable.puzzleModelLot` — remove or update puzzle entries
- [ ] Verify no live spawner in any .luz file references this LOT as the spawned template

**For a mission (CDMissions row):**
- [ ] Search `CDMissionsTable.prereqMissionID` (pipe-separated) for this mission ID
- [ ] Search `CDMissionNPCComponentTable.missionID` — remove all NPC associations
- [ ] Search `CDMissionTasksTable.id` — remove all task rows belonging to this mission
- [ ] Check `CDMissionsTable.randomPool` (CSV) for this mission ID in random pools

**For a skill (CDSkillBehaviorTable row):**
- [ ] Search `CDObjectSkillsTable.skillID` — remove skill associations from all objects/items
- [ ] Search `CDItemSetSkillsTable.SkillID` — remove from item set skill grants
- [ ] Search `CDMissionTasksTable.target` where `taskType=14` (UseSkill) — broken tasks
- [ ] The `behaviorID` this skill points to may be safe to remove if no other skill references it

**For a behavior (CDBehaviorTemplateTable row):**
- [ ] Search `CDSkillBehaviorTable.behaviorID` — skills pointing here become no-ops
- [ ] Search all `CDBehaviorParameterTable` float values that equal this behaviorID — child references become dead
- [ ] Search `CDDestructibleComponentTable.death_behavior` — death sequences pointing here

**For a loot matrix (CDLootMatrixTable rows):**
- [ ] Search `CDDestructibleComponentTable.LootMatrixIndex`
- [ ] Search `CDVendorComponentTable.LootMatrixIndex`
- [ ] Search `CDPackageComponentTable.LootMatrixIndex`

**For a zone (CDZoneTableTable row):**
- [ ] Search `CDActivitiesTable.instanceMapID`
- [ ] Search `CDPropertyTemplateTable.mapID` and `vendorMapID`
- [ ] Search `CDPropertyEntranceComponentTable.mapID`
- [ ] Verify the corresponding .luz and .lvl files are also removed from `res/maps/`

---

## Content ID Ranges

### LOT (CDObjects.id) — uint32_t

| Range | Convention | Notes |
|-------|------------|-------|
| 1 – 999 | Core system objects | Zone controllers, spawn markers, system templates |
| 1000 – 13999 | Original LEGO Universe content | Items, enemies, NPCs, interactables from LU |
| 14000 – 19999 | Late LU / unreleased content | Some IDs used by test/cut content |
| 20000 – 29999 | Uncertain / sparse | May have gaps; check CDClient before using |
| 30000+ | Safe range for custom content | Recommended starting point for server-custom LOTs |
| 0 | Reserved — NULL LOT | Never use; treated as invalid throughout the codebase |

**Recommendation:** Start custom LOTs at 30000 or higher. Check `SELECT MAX(id) FROM Objects` in
your CDClient SQLite database to find the current maximum and add a comfortable buffer.

### Mission IDs (CDMissions.id) — int32_t

| Range | Convention |
|-------|------------|
| 1 – 1799 | Original LU missions and achievements |
| 1800 – 2999 | Extended / late LU content |
| 3000+ | Safe range for custom missions |

**Note:** `prereqMissionID` is stored as a pipe-separated string, so mission IDs do not need to be
sequential — gaps are fine.

### Behavior IDs (CDBehaviorTemplateTable.behaviorID) — uint32_t

| Range | Convention |
|-------|------------|
| 1 – ~30000 | Original LU behavior definitions |
| 30001+ | Safe range for custom behaviors |

Behavior IDs are referenced only by float-cast values in `CDBehaviorParameterTable` and by
`CDSkillBehaviorTable.behaviorID`. When adding custom behaviors, pick an ID well above the current
maximum (`SELECT MAX(behaviorID) FROM BehaviorTemplate`) to avoid collisions.

### Skill IDs (CDSkillBehaviorTable.skillID) — uint32_t

| Range | Convention |
|-------|------------|
| 1 – ~800 | Original LU skills |
| 801+ | Safe range for custom skills |

Skills are referenced from `CDObjectSkillsTable.skillID` and `CDItemSetSkillsTable.SkillID`.

### Zone IDs (CDZoneTableTable.zoneID) — uint32_t

| Range | Zone Type |
|-------|-----------|
| 0 | Invalid / no zone |
| 1000 – 1999 | Main world zones (Avant Gardens, Nimbus Station, etc.) |
| 1100, 1200, … | Typically zone variants (+100 per major world) |
| 1800 – 1899 | Racetrack zones |
| 2000 – 2999 | Property zones per world |
| 9000+ | Suggested range for custom zones |

Zone IDs determine the .luz file path. The convention is:
```
res/maps/<zoneName>/<zoneID>/<zoneID>.luz
```
Pick a zoneID that has no existing entry in `CDZoneTableTable` and no corresponding directory under
`res/maps/`.

### Activity IDs (CDActivitiesTable.ActivityID) — uint32_t

Activity IDs are referenced from `CDRebuildComponentTable.activityID`. Original LU activities use
IDs up to roughly 150. Custom activities should use 200+ to avoid collisions.

### Component IDs (CDItemComponentTable.id, etc.) — uint32_t

Component IDs are the `component_id` stored in `CDComponentsRegistry` and the `id` primary key of
each component table. They are **per-table** — the same integer can appear in both
`CDItemComponentTable.id` and `CDDestructibleComponentTable.id` without conflict because the
`component_type` in the registry disambiguates which table to look up.

When inserting a new component row, use `SELECT MAX(id) FROM ItemComponent` (or the relevant
table) and increment from there. Reusing an existing `id` in a component table will silently
overwrite the data for whatever object previously owned that component entry (since the table is
loaded into a map keyed by `id`).

---

## Quick-Reference: Lookup Path by Content Goal

| Goal | Start Here | Follow |
|------|-----------|--------|
| Find all components of a LOT | `CDComponentsRegistry WHERE id=<LOT>` | `component_type` → table, `component_id` → row |
| Find what loot an enemy drops | `CDDestructibleComponentTable WHERE id=<compID>` | `LootMatrixIndex` → matrix → table → LOT |
| Find what a vendor sells | `CDVendorComponentTable WHERE id=<compID>` | `LootMatrixIndex` → matrix → table → LOT |
| Find all missions an NPC offers | `CDMissionNPCComponentTable WHERE id=<compID>` | `missionID` → CDMissions |
| Find what skill an item uses | `CDObjectSkillsTable WHERE objectTemplate=<LOT>` | `skillID` → CDSkillBehavior → `behaviorID` |
| Find all items that drop a LOT | `CDLootTableTable WHERE itemid=<LOT>` | `LootTableIndex` → CDLootMatrix → source |
| Find missions rewarding a LOT | `CDMissionsTable WHERE reward_item1=<LOT> OR ...` | direct |
| Find all tasks targeting a LOT | `CDMissionTasksTable WHERE target=<LOT>` | `id` → CDMissions for mission context |
| Find what item set uses a LOT | `CDItemSetsTable.itemIDs` (CSV search) | `setID` → CDItemSets |
| Find all zones using an activity | `CDActivitiesTable WHERE ActivityID=<id>` | `instanceMapID` → CDZoneTableTable |
