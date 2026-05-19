# DarkflameServer Content Systems

## Overview

This document describes how LEGO Universe game content is defined, loaded, and managed in DarkflameServer. Content includes items, NPCs, missions, skills, loot, pets, vehicles, properties, zones, and all game-playable elements. Most content is defined in the CDClient SQLite database (the read-only asset catalog extracted from the original game), with runtime state stored in the GameDatabase.

---

## Items System

### How Items Are Defined

**Primary Definition**: CDClient database `ItemComponent` table
- Every item in the game has a LOT (LEGO Object Type) ID
- Item properties are stored in the `ItemComponent` table via `CDItemComponentTable`

**Item Data Structure**:
```cpp
struct CDItemComponent {
    uint32_t id;                    // Component ID (links to object via ComponentsRegistry)
    std::string equipLocation;      // Where item equips (Head, Chest, Hands, etc.)
    uint32_t baseValue;             // NPC vendor sell price
    bool isKitPiece;                // Part of a multi-item set
    uint32_t rarity;                // Rarity level (1-10)
    uint32_t itemType;              // Type enum
    int64_t itemInfo;               // Extra item-specific data
    bool inLootTable;               // Can be looted from enemies
    bool inVendor;                  // Available from NPC vendors
    bool isUnique;                  // Only one can be owned
    bool isBOP;                     // Bind on Pickup
    bool isBOE;                     // Bind on Equip
    uint32_t reqFlagID;             // Mission flag required to obtain
    uint32_t reqSpecialtyID;        // Specialty/class requirement
    uint32_t reqAchievementID;      // Achievement requirement
    uint32_t stackSize;             // Max stack size (1 = non-stackable)
    uint32_t color1;                // Color/customization data
    uint32_t decal;                 // Decal/customization data
    std::string reqPrecondition;    // Complex precondition script
    uint32_t equipEffects;          // Visual effect when equipped
    bool isTwoHanded;               // Weapon requires both hands
    uint32_t commendationLOT;       // Commendation currency for crafting
    uint32_t commendationCost;      // Cost in commendations
    std::string currencyCosts;      // Additional currency costs (JSON-like format)
    uint32_t forgeType;             // Crafting type (weapon, armor, etc.)
    float SellMultiplier;           // Vendor sell price multiplier
};
```

**Related Tables**:
- `CDObjectsTable` - Object definitions (LOT, name, type, interaction distance)
- `CDComponentsRegistryTable` - Maps object LOT to component IDs
- `CDItemSetsTable` - Item set definitions for set bonuses
- `CDItemSetSkillsTable` - Skills granted when full set is equipped
- `CDInventoryComponentTable` - Inventory container definitions

### How Items Are Loaded

**Loading Flow**:
1. `CDClientManager::LoadValuesFromDatabase()` - Loads all CDClient tables into memory at startup
2. `Item::Item(LOT lot, Inventory* inventory)` - Constructor creates item instance
3. `Item::GetInfo()` - Returns `CDItemComponent` from `CDItemComponentTable::GetItemComponentByID()`
4. `InventoryComponent::AddItem()` - Adds item to player's inventory
5. `Item` serializes to player XML for persistence

**Specific Files**:
- `/dDatabase/CDClientDatabase/CDClientTables/CDItemComponentTable.h` - Item definitions loading
- `/dGame/dInventory/Item.h` - Item instance class
- `/dGame/dComponents/InventoryComponent.h` - Inventory management

### Data Representation

An item in game exists as:
```cpp
class Item {
    LWOOBJID id;                    // Unique instance ID
    LOT lot;                        // LEGO Object Type (from CDClient)
    Inventory* inventory;           // Parent inventory
    uint32_t slot;                  // Slot in inventory
    uint32_t count;                 // Stack count
    bool bound;                     // Binding status
    std::vector<LDFBaseData*> config;  // Item configuration (for rockets, etc.)
    LWOOBJID parent;                // Parent item (for proxy items)
    LWOOBJID subKey;                // Subkey (for pet items)
};
```

### Item Configuration

Complex items (rockets, pets, customized items) store configuration in LDF format:
```cpp
// Example: Rocket configuration
std::vector<LDFBaseData*> rocketConfig;
// Contains: rocket model LOT, paint color, wheels, engine, seat, armor
```

### Required Fields and Validation

When adding an item, the system validates:
- Item exists in CDItemComponentTable
- Preconditions satisfied (level, achievement, flag)
- Stack size constraints respected
- Inventory has space
- Binding rules enforced (BOP/BOE)

---

## Missions System

### How Missions Are Defined

**Primary Definition**: CDClient database tables:
- `CDMissionsTable` - Mission metadata
- `CDMissionTasksTable` - Individual tasks
- `CDMissionNPCComponentTable` - NPC offering properties

**Mission Data Structure**:
```cpp
struct CDMissions {
    int32_t id;                         // Mission ID
    std::string defined_type;           // Type: "mission", "achievement"
    std::string defined_subtype;        // Subtype: "storyline", "daily", "activity"
    int32_t UISortOrder;                // Display order in UI
    int32_t offer_objectID;             // LOT of mission giver NPC
    int32_t target_objectID;            // LOT of mission target
    int64_t reward_currency;            // Coins rewarded
    int32_t LegoScore;                  // LEGO Score rewarded
    int64_t reward_reputation;          // Reputation points
    bool isChoiceReward;                // Player chooses reward item
    int32_t reward_item1-4;             // Reward item LOTs
    int32_t reward_item1-4_count;       // Reward item counts
    int32_t reward_emote1-4;            // Emote rewards
    int32_t reward_maximagination;      // Max imagination increase
    int32_t reward_maxhealth;           // Max health increase
    int32_t reward_maxinventory;        // Inventory slot increase
    int32_t reward_maxwallet;           // Coin storage increase
    int32_t time_limit;                 // Time limit in seconds (0 = none)
    bool isMission;                     // Is this a mission (vs achievement)?
    bool repeatable;                    // Can be repeated (dailies)
    int64_t reward_currency_repeatable; // Repeatable reward currency
    int32_t time_limit;                 // Mission time limit
    bool inMOTD;                        // In Match of the Day
    int64_t cooldownTime;               // Cooldown between repeats
    std::string prereqMissionID;        // Prerequisites (pipe-separated)
};
```

**Task Data Structure**:
```cpp
struct CDMissionTasks {
    uint32_t id;                        // Task ID (mission_id * 100 + task_num)
    int32_t taskType;                   // Task type enum
    std::string targetObjects;          // Target LOTs (comma-separated)
    int32_t targetValue;                // Target count/value
    // ... more fields for specific task types
};
```

**Related Tables**:
- `CDMissionNPCComponentTable` - NPC mission offering properties
- `CDMissionEmailTable` - Email rewards

### How Missions Are Loaded

**Loading Flow**:
1. **Startup**: `CDMissionsTable::LoadValuesFromDatabase()` - Loads all missions
2. **Character Load**: `MissionComponent::LoadFromXml()` - Loads player's mission progress
3. **Mission Creation**: `Mission::Mission(missionID)` - Creates mission instance
4. **Mission State**: Mission state loaded from character XML (progress, completions, timestamp)
5. **Task Progress**: `MissionTask` progress tracked in mission state

**Specific Files**:
- `/dGame/dMission/Mission.h` - Mission instance
- `/dGame/dMission/MissionTask.h` - Task progress tracking
- `/dGame/dComponents/MissionComponent.h` - Player mission management
- `/dDatabase/CDClientDatabase/CDClientTables/CDMissionsTable.h` - Mission definitions

### Data Representation

A mission in game exists as:
```cpp
class Mission {
    uint32_t missionId;                 // Mission ID from CDClient
    const CDMissions& clientInfo;       // Definition from CDMissionsTable
    eMissionState state;                // ACTIVE, COMPLETE, FAILED
    uint32_t completions;               // Times completed (for dailies)
    uint32_t timestamp;                 // Last completion time
    std::vector<MissionTask> tasks;     // Current task progress
    LOT chosenReward;                   // Chosen reward item (if choice reward)
    bool claimed;                       // Rewards claimed?
};
```

**Mission States**:
- `ACTIVE` - Player actively working on mission
- `COMPLETE` - Mission tasks completed, ready to turn in
- `FAILED` - Mission failed (time limit or other reason)
- `ABANDONED` - Player abandoned mission
- `NOT_STARTED` - Unlocked but not started

### Task Types

| Task Type | Purpose | Fields |
|-----------|---------|--------|
| COLLECT | Collect specific items | targetObjects (item LOTs), targetValue (quantity) |
| DELIVERY | Deliver items to NPC | targetObjects (NPC LOT) |
| DISCOVER | Find/explore area | targetObjects (zone LOT) |
| EMOTE | Use specific emote | targetObjects (emote ID) |
| EXPLORE | Discover zone | targetObjects (zone LOT) |
| COMPLETE_ACTIVITY | Complete minigame | targetObjects (activity ID) |
| SKILL | Use specific skill | targetObjects (skill ID) |
| DIALOGUE | Talk to NPC | targetObjects (NPC LOT) |
| DEFEAT | Defeat enemies | targetObjects (enemy LOT), targetValue (count) |

### Mission Serialization

Mission state persists as XML in character file:
```xml
<mission id="104">
    <state>1</state>  <!-- ACTIVE -->
    <progress>
        <task id="1" progress="5"/>  <!-- Task progress -->
    </progress>
    <timestamp>1234567890</timestamp>
</mission>
```

---

## Skills and Behaviors System

### How Skills Are Defined

**Primary Definition**: CDClient database tables:
- `CDSkillBehaviorTable` - Skill to behavior mapping
- `CDBehaviorTemplateTable` - Behavior templates
- `CDBehaviorParameterTable` - Behavior parameters

**Skill Definition Flow**:
```
SkillID (e.g., 1)
  → CDSkillBehaviorTable (maps to BehaviorID)
    → CDBehaviorTemplateTable (defines behavior type)
      → CDBehaviorParameterTable (configures behavior)
```

**Skill Data Structure**:
```cpp
struct CDSkillBehavior {
    uint32_t skillID;               // Skill ID
    uint32_t behaviorID;            // Linked behavior ID
    uint32_t imaginationcost;       // Imagination cost to cast
    uint32_t cooldowngroup;         // Cooldown group ID
    float cooldown;                 // Cooldown duration
    // ... additional fields
};
```

**Behavior Types** (from `BehaviorTemplate` enum):
- BASIC_ATTACK - Melee attack
- PROJECTILE_ATTACK - Ranged attack
- HEAL - Restore health
- BUFF - Apply positive effect
- STUN - Disable target
- KNOCKBACK - Push target away
- PLAY_EFFECT - VFX only
- ... 50+ total types

### How Skills Are Loaded

**Loading Flow**:
1. `CDClientManager::LoadValuesFromDatabase()` - Loads all behavior/skill tables
2. `SkillComponent::CastSkill(skillID)` - Player casts skill
3. `CDSkillBehaviorTable::GetSkillByID(skillID)` - Retrieves skill definition
4. `BehaviorContext` created with behavior parameters
5. Behavior executes (e.g., damage calculation, effect application)

**Specific Files**:
- `/dGame/dComponents/SkillComponent.h` - Skill casting
- `/dGame/dBehaviors/Behavior.h` - Base behavior class
- `/dGame/dBehaviors/BehaviorContext.h` - Behavior execution context
- `/dDatabase/CDClientDatabase/CDClientTables/CDSkillBehaviorTable.h` - Skill mapping

### Behavior Execution

**Behavior Class Hierarchy**:
```cpp
class Behavior {
    virtual void Calculate(BehaviorContext* context) {}
    virtual void Execute(BehaviorContext* context) {}
    virtual void Load(int32_t behaviorID) = 0;
};
```

**Example: Heal Behavior**
```cpp
class HealBehavior : public Behavior {
    void Execute(BehaviorContext* context) {
        // Get healing amount from behavior parameters
        int32_t healAmount = GetParameter(context->behaviorID, "amount");
        // Apply healing to target
        context->target->GetDestroyableComponent()->Heal(healAmount);
    }
};
```

**Behavior Template System**:
- Behaviors defined once in CDClient as templates
- Parameters stored separately (allows reuse)
- At runtime: template + parameters = concrete behavior instance

---

## Loot System

### How Loot Is Defined

**Primary Definition**: CDClient database tables:
- `CDLootMatrixTable` - Loot table selection matrix
- `CDLootTableTable` - Individual loot item entries
- `CDRarityTableTable` - Rarity probability distribution

**Loot Matrix Concept**:
```
Enemy has LootMatrixIndex = 123
  → CDLootMatrixTable[123] contains:
      - LootTableIndex = 456
      - RarityTableIndex = 789
      - percent = 0.8 (80% chance this matrix is used)
      - minToDrop = 1
      - maxToDrop = 3
  → CDLootTableTable[456] contains items:
      - ItemID 6416 (sword)
      - ItemID 6417 (armor)
      - ItemID 6418 (helmet)
  → CDRarityTableTable[789] contains rarity weights:
      - Common (60%)
      - Uncommon (30%)
      - Rare (9%)
      - Legendary (1%)
```

**Data Structures**:
```cpp
struct CDLootMatrix {
    uint32_t LootTableIndex;        // Loot table to use
    uint32_t RarityTableIndex;      // Rarity weights
    float percent;                  // Probability of this matrix
    uint32_t minToDrop;             // Minimum items to drop
    uint32_t maxToDrop;             // Maximum items to drop
    uint32_t flagID;                // Optional flag requirement
};

struct CDLootTable {
    uint32_t itemid;                // LOT of item
    uint32_t LootTableIndex;        // Which table this belongs to
    bool MissionDrop;               // Only drops on mission completion
    uint32_t sortPriority;          // Drop priority
};

struct CDRarityTable {
    float randmax;                  // Random weight threshold
    uint32_t rarity;                // Rarity level
};
```

### How Loot Is Dropped

**Loot Generation Flow**:
```cpp
// When entity dies
Entity::Die() 
  → GetComponent<DestroyableComponent>()->GetLootMatrixID()
  → LootComponent::DropLoot(lootMatrixID)
    1. Get matrix entries from CDLootMatrixTable
    2. For each matrix (up to weight threshold):
       a. Select random items from CDLootTableTable
       b. Roll rarity from CDRarityTableTable
       c. Create Item instance with that rarity
       d. Spawn physical item in world
       e. Notify player of loot drop
```

**Specific Files**:
- `/dGame/dComponents/LootComponent.h` - Loot dropping
- `/dCommon/Loot.h` - Loot system namespace
- `/dDatabase/CDClientDatabase/CDClientTables/CDLootMatrixTable.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDLootTableTable.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDRarityTableTable.h`

### Loot Table Assignment

Every smashable object (enemy, crate, etc.) has:
- `DestroyableComponent::SetLootMatrixID(uint32_t)` - Sets which loot matrix to use
- From CDClient: `CDDestructibleComponentTable` specifies the loot matrix per object type

---

## NPCs and Characters System

### How NPCs Are Defined

**Primary Definition**: CDObjects table (LOT definition) + specific component tables

**NPC Creation**:
1. Object defined in `CDObjectsTable` with type "NPC"
2. Object has `CDMissionNPCComponentTable` entry if offering missions
3. Object has `CDMovementAIComponentTable` entry for AI behavior
4. Object linked to script via `CDScriptComponentTable`

**NPC Components**:
```cpp
// All NPCs have these components:
CharacterComponent          // Level, appearance
DestroyableComponent        // Health, combat stats
ModelComponent              // Appearance model
MovementAIComponent         // Wander, patrol behavior
BaseCombatAIComponent       // Combat behavior
MissionOfferComponent       // If mission giver
VendorComponent             // If vendor
ScriptComponent             // Attached script for behaviors
PhysicsComponent            // Position and collision
```

**Data Structure** (from `CDMovementAIComponentTable`):
```cpp
struct CDMovementAIComponent {
    uint32_t id;
    std::string MovementType;       // "WanderOnly", "Patrol", "Chase", etc.
    float WanderDelayMin;           // Min seconds between wander moves
    float WanderDelayMax;           // Max seconds between wander moves
    float Speed;                    // Movement speed
};
```

### How NPCs Are Loaded

**NPC Spawning Flow**:
1. `Spawner::Spawn()` called
2. `EntityManager::CreateEntity(LOT)` creates entity
3. Entity components loaded from CDClient:
   - CharacterComponent (from CDClient defaults)
   - Components listed in `CDComponentsRegistryTable`
4. Script attached via `ScriptComponent::SetScript()`
5. Entity serialized to client
6. Entity Update loop calls component `Update()` methods

**Specific Files**:
- `/dGame/dEntity/Entity.h` - Entity creation
- `/dGame/dComponents/MovementAIComponent.h` - NPC movement
- `/dGame/dComponents/BaseCombatAIComponent.h` - NPC combat
- `/dZoneManager/Spawner.h` - Entity spawning

### Character vs NPC

**Character** (player):
- Unique per player account
- Persisted to GameDatabase (inventory, missions, appearance)
- XML serialization for detailed state
- Controlled by player input

**NPC** (non-player):
- Created from CDClient definitions
- Stateless (respawns from definition)
- No persistent inventory
- AI or script-driven behavior

---

## Pets System

### How Pets Are Defined

**Primary Definition**: CDClient database tables:
- `CDPetComponentTable` - Pet attributes (speed, imagination drain)
- `CDTamingBuildPuzzleTable` - Taming minigame definition
- Item LOT defined in `CDObjectsTable` with type "pet"

**Pet Data Structure**:
```cpp
struct CDPetComponent {
    uint32_t id;                    // Component ID
    float walkSpeed;                // Movement speed
    float runSpeed;                 // Run speed
    float sprintSpeed;              // Sprint speed
    float imaginationDrainRate;     // Imagination consumed per second
    // ... more fields
};
```

**Taming Minigame**:
```cpp
struct CDTamingBuildPuzzle {
    uint32_t NPCLot;                // Pet NPC LOT
    uint32_t puzzleModelLot;        // Model to build during taming
    uint32_t numBricks;             // Number of bricks to build
    uint32_t timeLimit;             // Time limit in seconds
};
```

### How Pets Work

**Pet Lifecycle**:

1. **Wild Pet** - Exists in zone as NPC-like entity
   - Has `PetComponent`
   - Script: `scripts/02_server/Pets/` handler
   - Player interacts with pet

2. **Taming Minigame** - Initiated on interaction
   - Spawns minigame model from `CDTamingBuildPuzzleTable::puzzleModelLot`
   - Player must build model with `numBricks` bricks
   - `timeLimit` seconds to complete
   - On success: Pet added to inventory

3. **Pet Item** - Stored in inventory
   - LOT identifies pet species
   - LDF config stores taming data (name, level, etc.)

4. **Active Pet** - Deployed in world
   - `PetComponent::Activate()` creates entity
   - Pet follows owner
   - Can be ordered with commands
   - Parent entity tracks ownership

**Pet Component Methods**:
```cpp
class PetComponent {
    void OnUse(Entity* originator);           // Start taming
    void TryBuild(uint32_t numBricks);        // Build attempt
    void NotifyTamingBuildSuccess();          // Taming complete
    void Activate(Item* item);                // Deploy pet
    void Deactivate();                        // Despawn pet
    void Command(NiPoint3 pos, LWOOBJID src); // Give pet command
};
```

**Specific Files**:
- `/dGame/dComponents/PetComponent.h` - Pet behavior
- `/dGame/dInventory/DatabasePet.h` - Pet persistence
- `/dDatabase/CDClientDatabase/CDClientTables/CDPetComponentTable.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDTamingBuildPuzzleTable.h`

---

## Vehicles and Mounts System

### How Vehicles Are Defined

**Primary Definition**:
- Vehicle item defined in `CDObjectsTable`
- `CDInventoryComponentTable` specifies inventory if vehicle contains items
- Physics defined in `CDPhysicsComponentTable`
- Behavior defined in script

**Vehicle Components**:
```cpp
PossessableComponent            // Can be mounted/controlled
HavokVehiclePhysicsComponent    // Vehicle-specific physics
DestroyableComponent            // Vehicle health
SkillComponent                  // Vehicle abilities (boost, etc.)
RacingControlComponent          // For racing vehicles
ScriptComponent                 // Vehicle behavior script
```

### How Vehicles Are Used

**Vehicle Mounting Flow**:
1. Player uses vehicle item
2. `InventoryComponent::UseItem(lot)` called
3. Vehicle entity spawned via `Item::SpawnEntity()`
4. `PossessableComponent::OnUse()` called on vehicle
5. Player entity gains `PossessorComponent`
6. Player's physics component replaced with vehicle physics
7. Client notified of possession change
8. Player input now controls vehicle

**Related Components**:
```cpp
// On vehicle entity:
PossessableComponent {
    LWOOBJID possessor;             // Current controller
    ePossessionType type;           // MOUNT, VEHICLE, etc.
    bool depossessOnHit;            // Dismount on damage?
};

// On player entity:
PossessorComponent {
    LWOOBJID possessable;           // Controlled vehicle
    bool inPossession;              // Currently mounted?
};
```

**Specific Files**:
- `/dGame/dComponents/PossessableComponent.h` - Mounting target
- `/dGame/dComponents/PossessorComponent.h` - Mounted player
- `/dGame/dComponents/HavokVehiclePhysicsComponent.h` - Vehicle physics

---

## Properties and Housing System

### How Properties Are Defined

**Primary Definition**: CDClient database tables:
- `CDPropertyTemplateTable` - Property definitions
- `CDPropertyEntranceComponentTable` - Property zone entry
- `CDZoneTableTable` - Property zone definitions

**Property Data Structure**:
```cpp
struct CDPropertyTemplate {
    uint32_t templateID;            // Property template ID
    uint32_t mapID;                 // Main zone ID
    uint32_t vendorMapID;           // Vendor/decoration zone
    uint32_t zoneID;                // Property instance zone
    std::string propertyName;       // Display name
};

struct CDPropertyEntranceComponent {
    uint32_t id;                    // Component ID
    uint32_t mapID;                 // Entrance zone
    uint32_t zoneID;                // Property zone
    float spawnX, spawnY, spawnZ;   // Spawn location
    float spawnRX, spawnRY, spawnRZ; // Spawn rotation
};
```

### How Properties Work

**Property Ownership**:
- Stored in GameDatabase
- Character has `propertyCloneID` in their data
- Property entities marked with owner ID

**Property Management**:
- `PropertyManagementComponent` - Rent, deeds, models
- `PropertyComponent` - Placeholder component
- `ScriptComponent` - Property-specific behaviors

**Property Features**:
- **Decoration** - Place custom models
- **Furniture** - Interactive objects
- **Rental** - Rents periodically based on type
- **Vendor** - Can set up vendor store

**Specific Files**:
- `/dGame/dComponents/PropertyComponent.h`
- `/dGame/dComponents/PropertyManagementComponent.h`
- `/dGame/dComponents/PropertyEntranceComponent.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDPropertyTemplateTable.h`

---

## Zones/Levels System

### How Zones Are Defined

**Primary Definition**: CDClient database table:
- `CDZoneTableTable` - Zone metadata

**Zone Data Structure**:
```cpp
struct CDZoneTable {
    uint32_t zoneID;                // Unique zone ID
    std::string zoneName;           // Display name
    uint32_t scriptID;              // Zone script ID
    float ghostdistance_min;        // Minimum ghost distance
    float ghostdistance;            // Primary ghost distance
    uint32_t population_soft_cap;   // Suggested max players
    uint32_t population_hard_cap;   // Absolute max players
    float smashableMinDistance;     // Minimum smashable interact distance
    float smashableMaxDistance;     // Maximum smashable interact distance
    std::string serverPhysicsFramerate; // Physics tick rate
    uint32_t zoneControlTemplate;   // Zone control NPC LOT
    uint32_t widthInChunks;         // World width in chunks
    uint32_t heightInChunks;        // World height in chunks
    bool petsAllowed;               // Can pets enter zone?
    bool mountsAllowed;             // Can mounts enter zone?
    float teamRadius;               // Team radius for activities
    bool PlayerLoseCoinsOnDeath;    // Coin drop on death?
};
```

### How Zones Are Loaded

**Zone Loading Flow**:
1. Player requests zone transition
2. `Zone::Load(zoneID)` called
3. `CDZoneTableTable::Query(zoneID)` retrieves zone definition
4. Zone folder loaded from resources/ (if exists)
5. Spawners activated
6. Zone script executed via `ScriptComponent`
7. Player entity spawned in zone
8. Entities serialized to client

**Zone Structure**:
```
resources/
└── zones/
    └── {zoneID}/
        ├── {zoneID}.lua            (Zone script - converted to C++)
        ├── spawns.txt              (Spawner definitions)
        ├── lighting.txt            (Lighting info)
        └── level.lvl               (Level/terrain data)
```

**Related Components**:
```cpp
Zone {
    uint32_t zoneID;
    Level* level;                   // Terrain/environment data
    std::map<uint32_t, Spawner*> spawners; // All spawners in zone
    std::vector<LUTriggers::Trigger> triggers; // Trigger volumes
};
```

**Specific Files**:
- `/dZoneManager/Zone.h` - Zone representation
- `/dZoneManager/Level.h` - Level terrain
- `/dZoneManager/Spawner.h` - Entity spawner
- `/dDatabase/CDClientDatabase/CDClientTables/CDZoneTableTable.h`

---

## Activities and Minigames

### How Activities Are Defined

**Primary Definition**: CDClient database tables:
- `CDActivitiesTable` - Activity metadata
- `CDActivityRewardsTable` - Activity reward mappings
- `CDRebuildComponentTable` - Quick-build properties

**Activity Data Structure**:
```cpp
struct CDActivities {
    uint32_t ActivityID;            // Activity ID
    uint32_t instanceMapID;         // Instanced zone ID
    uint32_t minTeams;              // Minimum teams
    uint32_t maxTeams;              // Maximum teams
    uint32_t minTeamSize;           // Min team members
    uint32_t maxTeamSize;           // Max team members
    uint32_t waitTime;              // Waiting period before start
    uint32_t startDelay;            // Delay before actual start
    bool requiresUniqueData;        // Requires instance data?
    uint32_t leaderboardType;       // Leaderboard type (racing, etc.)
    bool noTeamLootOnDeath;         // Don't drop loot on death
    float optionalPercentage;       // Optional objective percentage
};
```

### Activity Types

**Quick-Build Activities**:
- Player must build structure with imagination
- Properties: `reset_time`, `complete_time`, `take_imagination`
- Entity has `QuickBuildComponent` + `RebuildComponent`

**Racing**:
- Controlled by `RacingControlComponent`
- Tracks time, position, completion
- Assigns ranks and leaderboard scores

**Shooting Gallery**:
- `ShootingGalleryComponent` manages gameplay
- Calculates accuracy, score
- Delivers rewards

**Generic Minigames**:
- `MiniGameControlComponent` for custom games
- Script-driven via `ScriptComponent`

**Specific Files**:
- `/dGame/dComponents/ActivityComponent.h`
- `/dGame/dComponents/MiniGameControlComponent.h`
- `/dGame/dComponents/RacingControlComponent.h`
- `/dGame/dComponents/QuickBuildComponent.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDActivitiesTable.h`

---

## Quick Builds System

### How Quick Builds Are Defined

**Primary Definition**: CDClient tables:
- `CDRebuildComponentTable` - Quick-build properties
- `CDDestructibleComponentTable` - Smashable properties

**Quick Build Data**:
```cpp
struct CDRebuildComponent {
    uint32_t id;                    // Component ID
    float reset_time;               // Time to respawn after destruction
    float complete_time;            // Time to complete build
    uint32_t take_imagination;      // Imagination cost per second
    bool interruptible;             // Can be interrupted?
    bool self_activator;            // Activates nearby quick-builds?
    uint32_t activityID;            // Linked activity
    uint32_t post_imagination_cost; // Imagination cost on completion
    float time_before_smash;        // Time before smashing for loot
};
```

### How Quick Builds Work

**Quick Build Lifecycle**:

1. **Idle** - Waiting for player
   - Entity visible but inactive
   - `QuickBuildComponent::enabled = false`

2. **Building** - Player activates
   - `QuickBuildComponent::StartBuild()` called
   - Imagination deducted per tick
   - Visual feedback to player
   - Time counter running

3. **Complete** - Build finished
   - `QuickBuildComponent::CompleteQuickBuild()` called
   - Post-imagination cost deducted
   - Next quick-build activated (if self-activator)
   - Loot table accessed after `time_before_smash` delay

4. **Respawn** - After destruction
   - `reset_time` delay
   - Entity respawns with full health

**Component Methods**:
```cpp
class QuickBuildComponent {
    void StartBuild(Entity* builder);
    void CompleteQuickBuild();
    void CancelBuild();
    void Update(float deltaTime);  // Imagination deduction
};
```

**Specific Files**:
- `/dGame/dComponents/QuickBuildComponent.h`
- `/dDatabase/CDClientDatabase/CDClientTables/CDRebuildComponentTable.h`

---

## Collectibles System

### How Collectibles Are Defined

**Definition**: `CollectibleComponent` linked objects

**Collectible Features**:
- Small objects players can find and collect
- Examples: coins, adventure pack bits, decorations
- Identified by `CDPlayerFlagsTable` entry
- Rewarding collection unlocks achievements

### How Collectibles Work

**Collection Flow**:
1. Player approaches collectible
2. `CollectibleComponent::OnUse()` triggered
3. Item added to inventory or counted
4. Flag set in character data
5. Achievement checked

**Related**:
- `CollectibleComponent` - Defines collectible
- `CDPlayerFlagsTable` - Achievement/collection flag definitions

---

## Summary: Content Loading Pipeline

```
Startup:
1. CDClientDatabase::Connect(cdclient.db)
2. Load all CDClient tables into memory:
   - CDObjectsTable, CDItemComponentTable, CDMissionsTable
   - CDSkillBehaviorTable, CDBehaviorTemplateTable
   - CDLootMatrixTable, CDLootTableTable, CDRarityTableTable
   - CDZoneTableTable, CDActivitiesTable, etc.

Character Load:
1. Load character XML from GameDatabase
2. Load inventory items with Item instances
3. Load mission progress with Mission instances
4. Load active pet with PetComponent

Zone Load:
1. Load zone definition from CDZoneTableTable
2. Load spawners from zone file
3. Spawn entities with proper components from CDClient
4. Execute zone script
5. Serialize entities to client

Entity Interaction (example: use item):
1. Player uses item LOT
2. Look up CDItemComponentTable entry
3. Create Item instance
4. If item is skill: look up CDSkillBehaviorTable
   - Find BehaviorID
   - Load CDBehaviorTemplateTable entry
   - Create BehaviorContext with parameters from CDBehaviorParameterTable
   - Execute behavior
5. If item is pet: look up CDPetComponentTable
   - Create PetComponent
   - Link to taming minigame from CDTamingBuildPuzzleTable
6. If item is vehicle: create entity from CDObjectsTable
   - Add PossessableComponent
   - Mount player on vehicle
```

---

## Key Files Reference

### Content Definition (CDClient)
- `dDatabase/CDClientDatabase/CDClientTables/CDObjectsTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDItemComponentTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDMissionsTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDMissionTasksTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDSkillBehaviorTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDLootMatrixTable.h`
- `dDatabase/CDClientDatabase/CDClientTables/CDZoneTableTable.h`

### Runtime Implementation
- `dGame/dInventory/Item.h` - Item instances
- `dGame/dMission/Mission.h` - Mission progress
- `dGame/dComponents/SkillComponent.h` - Skill execution
- `dGame/dComponents/PetComponent.h` - Pet behavior
- `dGame/dComponents/PossessableComponent.h` - Vehicle control
- `dGame/dComponents/QuickBuildComponent.h` - Quick-build logic
- `dGame/dComponents/MissionComponent.h` - Mission tracking
- `dZoneManager/Zone.h` - Zone management
- `dZoneManager/Spawner.h` - Entity spawning

