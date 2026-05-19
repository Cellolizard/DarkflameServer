# DarkflameServer Code Organization

## Overview

DarkflameServer is a C++ LEGO Universe private server emulator consisting of approximately 1,200 source files organized into a modular architecture. The codebase separates concerns into distinct functional areas: core game logic (dGame), database layers (dDatabase, dCommon), networking (dNet), scripting (dScripts), and specialized servers (dZoneManager, dChatServer, dAuthServer, dMasterServer).

### File Statistics

| Module | Files | Purpose |
|--------|-------|---------|
| dGame | 341 | Core game logic, entities, components, behaviors, missions |
| dScripts | 613 | Server-side scripting system (C++ replacement for Lua) |
| dDatabase | 168 | Database access layer, CDClient catalog tables, game database |
| dCommon | 124 | Utilities, serialization, enums, data structures |
| dChatServer | 13 | Chat server implementation |
| dNavigation | 10 | Pathfinding and navigation systems |
| dNet | 21 | Network protocol layer |
| dPhysics | 14 | Physics simulation and collision detection |
| dZoneManager | 11 | Zone/world loading and management |
| dWorldServer | 3 | World server entry point |
| dAuthServer | 1 | Authentication server |

---

## Module Descriptions

### dGame (341 files)

**Primary Responsibility**: Core game logic, entity systems, component architecture, and game state management.

The dGame module follows a component-based entity system architecture. Every entity (player, NPC, item, etc.) consists of a central Entity object with attached Components that provide specific functionality.

#### Key Directories and Classes

**dGame/** (Root Level)
- **Entity.h/Entity.cpp** - Base entity class representing any object in the game world. Contains component management, messaging, and serialization logic.
  - Core methods: `Initialize()`, `AddComponent()`, `GetComponent()`, `Update()`
  - Owns all entities in an instance/zone
  
- **Character.h/Character.cpp** - Represents a player character including their name, level, appearance, and character-specific data. Persisted to XML.
  - Manages: name, appearance data, property clone ID, mission/achievement state
  - Serializes to/from XML and database
  
- **User.h/User.cpp** - Represents a connected player session and account information.
  
- **EntityManager.h/EntityManager.cpp** - Global singleton managing all entities in the game world, handles entity creation/destruction, messaging.
  
- **PlayerManager.h/PlayerManager.cpp** - Manages connected players and player-specific logic.
  
- **TeamManager.h/TeamManager.cpp** - Manages team creation and team-based gameplay.
  
- **TradingManager.h/TradingManager.cpp** - Handles player-to-player trading.

**dGame/dComponents/** (45 components)

The component-based architecture. All components inherit from `Component` base class:

```cpp
class Component {
    virtual void Update(float deltaTime) {}
    virtual void OnUse(Entity* originator) {}
    virtual void Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {}
    virtual void LoadFromXml(const tinyxml2::XMLDocument& doc) {}
    virtual void UpdateXml(tinyxml2::XMLDocument& doc) {}
};
```

**Core Components** (always-present on most entities):
- **CharacterComponent** - Player character data (level, skills, stats, rockets)
- **DestroyableComponent** - Health, armor, imagination (player/mob attributes)
- **ModelComponent** - Visual representation and rendering
- **InventoryComponent** - Item storage, equipment, inventory management
- **PhysicsComponent** (base) - Position, rotation, velocity
  - **ControllablePhysicsComponent** - Player-controlled movement
  - **PhantomPhysicsComponent** - Non-solid physics
  - **HavokVehiclePhysicsComponent** - Vehicle-specific physics

**Combat & Skills**:
- **SkillComponent** - Skill execution, cooldowns, casting
- **BaseCombatAIComponent** - NPC combat AI
- **BuffComponent** - Status effects and buffs
- **LevelProgressionComponent** - Experience and leveling

**Content & Gameplay**:
- **MissionComponent** - Mission tracking and progression
- **MissionOfferComponent** - NPC mission offerings
- **PetComponent** - Tameable pets and pet minigames
- **PossessableComponent** - Objects that can be mounted/controlled (vehicles, mounts)
- **PossessorComponent** - Entity that possesses other entities
- **InventoryComponent** - Item storage system
- **PropertyComponent** - Player housing/properties
- **PropertyManagementComponent** - Property management features
- **PropertyEntranceComponent** - Entry points to properties
- **PropertyVendorComponent** - NPCs that trade property items

**Interactive Objects**:
- **QuickBuildComponent** - Quick-build brick structures with health/completion time
- **RebuildComponent** - (Linked to quick builds in CDClient)
- **SwitchComponent** - Interactive switches
- **TriggerComponent** - Trigger volumes
- **ScriptComponent** - Attached C++ scripts
- **ScriptedActivityComponent** - Minigame/activity logic

**Specialized**:
- **RacingControlComponent** - Racing minigame logic
- **RacingStatsComponent** - Racing stats tracking
- **ShootingGalleryComponent** - Shooting minigame
- **MiniGameControlComponent** - Generic minigame handling
- **ActivityComponent** - Activity/group activity support
- **AchievementVendorComponent** - Achievement vendor NPC
- **DonationVendorComponent** - Donation/charity vendors
- **MovingPlatformComponent** - Moving platform paths
- **RailActivatorComponent** - Rail-mounted objects
- **BouncerComponent** - Bouncer trampoline objects
- **CollectibleComponent** - Collectible brick/item objects
- **ModuleAssemblyComponent** - Modular building systems
- **ProximityMonitorComponent** - Proximity triggers
- **ItemComponent** - Individual item properties
- **RenderComponent** - Custom rendering
- **MultiZoneEntranceComponent** - Zone transition points
- **BuildBorderComponent** - Building boundary enforcement
- **GhostComponent** - Ghost/spectator mode
- **PlayerForcedMovementComponent** - Scripted player movement
- **LUPExhibitComponent** - LUP (LEGO Universe Property) exhibits

**Design Pattern**: Component pattern using `eReplicaComponentType` enum to identify component types. Components are serialized to clients for synchronization.

#### dGame/dBehaviors/ (70+ behavior types)

Behavior tree system for skill execution and NPC AI. Each behavior is a template that can be instantiated with parameters.

**Behavior Types**:
- **Attack behaviors**: BasicAttackBehavior, ProjectileAttackBehavior, NpcCombatSkillBehavior
- **Effect behaviors**: PlayEffectBehavior, HealBehavior, ImaginationBehavior, BuffBehavior, ApplyBuffBehavior, RemoveBuffBehavior
- **Utility behaviors**: DurationBehavior, OverTimeBehavior, DelayBehavior, AndBehavior, SwitchBehavior, ChainBehavior
- **Movement behaviors**: ForceMovementBehavior, AirMovementBehavior, KnockbackBehavior, PullToPointBehavior
- **Control behaviors**: StunBehavior, InterruptBehavior, TauntBehavior, ClearTargetBehavior, TargetCasterBehavior
- **Special abilities**: VentureVisionBehavior, DarkInspirationBehavior, JetPackBehavior, CarBoostBehavior, PropertyTeleportBehavior
- **Control flow**: StartBehavior, EndBehavior, VerifyBehavior, SkillEventBehavior

**Template Hierarchy**:
- BehaviorTemplate enum (70+ types)
- BehaviorContext - Runtime context for behavior execution
- BehaviorSlot - Behavior slot assignment for skills

#### dGame/dMission/

**Mission.h** - Represents a single mission instance with state tracking
- Tracks current progress, task completion, rewards claimed
- Loads/saves mission state to XML

**MissionTask.h** - Individual tasks within a mission
- Task types: delivery, defeat X enemies, collect items, etc.
- Progress tracking and completion validation

**MissionPrerequisites.h** - Mission prerequisite checking (locked until other missions complete)

**Related**: Mission data is loaded from CDMissionsTable (CDClient database)

#### dGame/dInventory/

**Item.h** - Individual inventory item
- Properties: LOT, count, equipped status, item config, binding
- Persisted to database with inventory type

**Inventory.h** - Collection of items within an inventory
- Manages slots, item lookup, equipping/unequipping
- Multiple inventory types (general, mission, temporary, etc.)

**InventoryComponent.h** - Component that manages all inventories for an entity
- Methods: AddItem, RemoveItem, EquipItem, GetInventories()
- Handles item serialization/networking

**ItemSet.h** - Set bonuses when items from a set are equipped
- ItemSetPassiveAbility tracking

**EquippedItem.h** - Represents an equipped item with its current configuration

#### dGame/dGameMessages/

**GameMessages.h** - RPC-style message system for client-server communication
- Virtual methods for serialization/deserialization
- Examples: TeleportMessage, PlayAnimationMessage, StartSkillMessage
- Message routing through Entity::SendMessage()

#### dGame/dUtilities/

**SlashCommands/** - Admin/debug command implementations (e.g., `/kick`, `/ban`, `/teleport`)

### dDatabase (168 files)

**Primary Responsibility**: Database abstraction layer, CDClient catalog tables, game database access.

#### dDatabase/CDClientDatabase/

The CDClient database is the LEGO Universe asset catalog - a read-only SQLite database containing game content definitions.

**CDClientDatabase.h/CDClientManager.h**
- Global namespace for database initialization and queries
- `CDClientDatabase::Connect(filename)` - Opens the SQLite database
- `CDClientDatabase::ExecuteQuery(sql)` - Executes SELECT queries
- `CDClientDatabase::ExecuteDML(sql)` - Executes INSERT/UPDATE/DELETE

#### CDClientTables/ (43 tables)

Each table represents a type of game content:

**Core Content Tables**:

| Table | Purpose | Key Fields |
|-------|---------|-----------|
| **CDObjectsTable** | All game objects/entities | id (LOT), name, type, interactionDistance |
| **CDItemComponentTable** | Item definitions | id, equipLocation, baseValue, rarity, stackSize, itemType |
| **CDMissionsTable** | Mission definitions | id, defined_type, offer_objectID, reward_currency, LegoScore |
| **CDMissionTasksTable** | Task definitions within missions | id, taskType, targetObjects, targetValue |
| **CDMissionNPCComponentTable** | NPC mission offering metadata | id, missionID, offersMission, acceptsMission |
| **CDSkillBehaviorTable** | Skill to behavior mapping | skillID, behaviorID, imaginationcost, cooldown |
| **CDBehaviorTemplateTable** | Behavior tree templates | behaviorID, behavior_type, templateID |
| **CDBehaviorParameterTable** | Behavior parameters | behaviorID, parameterID, parameterValue |
| **CDLootMatrixTable** | Loot table index mapping | LootTableIndex, RarityTableIndex, minToDrop, maxToDrop |
| **CDLootTableTable** | Individual loot entries | itemid, LootTableIndex, MissionDrop |
| **CDRarityTableTable** | Item rarity distribution | rarity, randmax |
| **CDZoneTableTable** | Zone/world definitions | zoneID, zoneName, scriptID, ghostdistance, population_soft_cap |
| **CDPetComponentTable** | Pet attributes | id, walkSpeed, runSpeed, sprintSpeed, imaginationDrainRate |
| **CDInventoryComponentTable** | Inventory slot definitions | id, itemcount, invType |
| **CDActivitiesTable** | Activity/minigame definitions | ActivityID, minTeams, maxTeams, requiresUniqueData, leaderboardType |
| **CDActivityRewardsTable** | Activity reward mappings | ActivityID, ObjectTemplate, itemReward |
| **CDRebuildComponentTable** | Quick-build properties | id, reset_time, complete_time, take_imagination |
| **CDComponentsRegistryTable** | Entity to component mapping | id, component_type, component_id |
| **CDPropertyTemplateTable** | Property/house definitions | templateID, mapID, vendorMapID, zoneID |
| **CDPropertyEntranceComponentTable** | Property entrance properties | id, mapID, zoneID, spawnX, spawnY, spawnZ |
| **CDVendorComponentTable** | NPC vendor inventory | id, buyPriceMultiplier, sellPriceMultiplier, refreshTimeSeconds |
| **CDEmoteTable** | Emote animations | id, animationName |
| **CDAnimationsTable** | Animation definitions | animationGroupID, sequenceName, index |
| **CDDestructibleComponentTable** | Smashable object properties | id, faction, life, level, armor, impact_damage |
| **CDBrickIDTableTable** | Brick/model definitions | NDObjectID, NDLOT |
| **CDObjectSkillsTable** | Skills owned by objects | objectTemplate, skillID, castOnType, AffectCaster |
| **CDItemSetsTable** | Item set definitions | itemSetID, itemSetName, itemSetDesc |
| **CDItemSetSkillsTable** | Skills granted by item sets | itemSetID, skillID, skillCastType |
| **CDLevelProgressionLookupTable** | Level/XP progression | level, experience |
| **CDCurrencyTableTable** | Currency/resource definitions | currencyIndex, npcMinLevel, npcMaxLevel, firstTimeRewardAmount |
| **CDRewardCodesTable** | Promotional reward codes | code, rewardTableIndex |
| **CDRewardsTable** | Reward definitions | rewardID, currencyIndex, itemReward, itemCount |
| **CDPhysicsComponentTable** | Physics properties | id, bStatic, physicsAsset, jumpAudioEventSet |
| **CDMovementAIComponentTable** | NPC AI properties | id, MovementType, WanderDelayMin, WanderDelayMax, Speed |
| **CDScriptComponentTable** | Script assignments | id, scriptName, client |
| **CDTamingBuildPuzzleTable** | Pet taming minigame puzzles | NPCLot, puzzleModelLot, numBricks, timeLimit |
| **CDProximityMonitorComponentTable** | Proximity trigger properties | id, Proximities_listID |
| **CDPackageComponentTable** | Package/container definitions | id, itemcount |
| **CDPlayerFlagsTable** | Achievement/badge definitions | id, flagName, flagType |
| **CDRailActivatorComponent** | Rail properties | id, startAnimation, loopAnimation, stopAnimation |
| **CDFeatureGatingTable** | Feature toggle/gating | featureName, major, minor, build |

**Design Pattern**: Each table class inherits from `CDTable<TableName, StorageType>` and uses template specialization for type-safe access:
```cpp
class CDItemComponentTable : public CDTable<CDItemComponentTable, std::map<uint32_t, CDItemComponent>> {
    const CDItemComponent& GetItemComponentByID(uint32_t id);
    void LoadValuesFromDatabase();
};
```

#### dDatabase/GameDatabase/

Stores game state specific to a server instance:
- Character data and progression
- Player inventory and equipment
- Property ownership
- Leaderboards
- Guild data
- Account bans and reports

### dCommon (124 files)

**Primary Responsibility**: Shared utilities, enums, data structures, serialization, and common patterns.

#### dCommon/dEnums/

Comprehensive enumeration definitions for game types:
- **eGameMasterLevel** - Admin level (CIVILIAN, MODERATOR, ADMIN, DEVELOPER)
- **eInventoryType** - Inventory classifications (items, equipment, vault, etc.)
- **eAnimationFlags** - Character animation states
- **ePossessionType** - Vehicle/mount possession types
- **eReplicaComponentType** - Component type IDs (enum for all 45+ components)
- **eMissionState** - Mission progress states (ACTIVE, COMPLETE, FAILED, etc.)
- **eMissionTaskType** - Task types (COLLECT, DEFEAT, etc.)
- **eItemType** - Item classifications (WEAPON, ARMOR, CONSUMABLE, etc.)
- **eLootSourceType** - Where loot came from (LOOT, VENDOR, ACTIVITY, etc.)
- **eKillType** - Cause of death types
- **MessageType** - Game, Chat, World, Master message type enums

#### Key Utilities

**LDFFormat.h** - LDFBaseData struct system for flexible key-value data
- Used for item config, entity settings, mission state
- Runtime polymorphic data types (String, Int, Float, etc.)

**Amf3.h / AmfSerialize.h / AMFDeserialize.h** - AMF3 (Action Message Format) serialization for complex data structures

**BinaryIO.h** - Binary data reading/writing utilities

**GeneralUtils.h** - String manipulation, conversion utilities

**Logger.h** - Logging infrastructure

**NiPoint3.h / NiQuaternion.h** - Math vector and rotation types from Gamebryo engine

**Brick.h** - Brick/LOT data structures

**Loot.h / Loot namespace** - Loot generation and drop systems

**Preconditions.h** - Mission precondition evaluation

**DatabasePet.h** - Pet persistence structure

### dNet (21 files)

**Primary Responsibility**: Network protocol layer abstraction using RakNet library.

Key classes:
- **BitStream.h** - RakNet::BitStream wrapper for binary serialization
- Network packet routing and message handling
- Server discovery and authentication flows

### dZoneManager (11 files)

**Primary Responsibility**: Zone/world loading, object instantiation, spawner management.

**Key Classes**:

**Zone.h**
- Represents a loaded zone/world instance
- Contains all entities in the zone
- Properties: zone ID, name, population caps, physics framerate
- Scene references for visual representation

**Level.h** - Level/terrain data for a zone

**Spawner.h** - Spawns entities at predefined locations
- Supports spawner types: NPC, loot, enemies
- Respawn on death functionality
- Customizable spawn timing

**dZoneManager.h** - Global manager for all loaded zones

**WorldConfig.h** - Zone configuration from CDZoneTable

**LUTriggers.h** - Trigger volume definitions and trigger event handling

### dScripts (613 files)

**Primary Responsibility**: Server-side gameplay logic implemented in C++ (replacement for original Lua scripts).

**Directory Structure**:
- **02_server/** - Core server scripts
  - **DLU/** - DLU-specific enhancements
  - **Objects/** - Interactive object behaviors
  - **Enemy/** - Enemy/NPC AI behaviors
  - **Pets/** - Pet-specific logic
  - **Equipment/** - Equipment item effects
  - **Minigame/** - Minigame implementations
- **zone/** - Zone-specific scripts (by zone ID)
  - **PROPERTY/** - Property/house scripts
  - **LUPs/** - LUP-specific logic
- **ai/** - NPC AI scripts by faction (GF, NS, FV, etc.)
- **EquipmentScripts/** - Equipment behavior scripts
- **EquipmentTriggers/** - Equipment-triggered effects

**Design Pattern**: Scripts implement virtual functions from base classes:
```cpp
namespace CppScripts {
    class Script {
        virtual void OnStartup(Entity* self) {}
        virtual void OnUpdate(Entity* self, float deltaTime) {}
        virtual void OnCollisionPhantom(Entity* self, Entity* target) {}
        virtual void OnUse(Entity* self, Entity* user) {}
        // ... many more event handlers
    };
}
```

### dChatServer, dAuthServer, dMasterServer, dWorldServer

**Specialized servers** for chat, authentication, master server coordination, and world server entry points.

---

## Architecture Patterns

### 1. Entity-Component System (ECS)

Every game object is an Entity that contains a collection of Components. Components encapsulate behavior and data.

```
Entity
├── CharacterComponent (player stats)
├── InventoryComponent (items)
├── PhysicsComponent (position/velocity)
├── SkillComponent (abilities)
├── DestroyableComponent (health)
└── ScriptComponent (attached script)
```

### 2. Component Pattern

All Components inherit from base `Component` class:
- `Update(deltaTime)` - Called every frame
- `OnUse(originator)` - Called when entity is used
- `Serialize(bitstream)` - Network synchronization
- `LoadFromXml() / UpdateXml()` - Persistence

### 3. Message/Event Dispatch

Game messages routed through Entity messaging system:
- Components register handlers for specific message types
- Entity forwards messages to appropriate component
- Asynchronous callback-based architecture

### 4. Database Query Abstraction

CDClient tables wrapped in C++ classes:
- `LoadValuesFromDatabase()` - Populates from SQLite
- Type-safe query methods (e.g., `GetItemComponentByID(id)`)
- Cached in-memory representation for fast access

### 5. Serialization

Multiple serialization targets:
- **RakNet BitStream** - Network transmission to clients
- **XML (tinyxml2)** - Character state persistence
- **SQLite** - Game database and CDClient

---

## Inter-Module Dependencies

```
dGame (Core)
├── depends on → dDatabase (CDClient catalog)
├── depends on → dCommon (Utils, enums)
├── depends on → dScripts (Attached behaviors)
├── depends on → dNet (Message transmission)
└── depends on → dZoneManager (Zone loading)

dZoneManager
├── depends on → dGame/Entity (Entity creation)
├── depends on → dDatabase/CDZoneTable (Zone definitions)
└── depends on → dCommon (Utilities)

dDatabase
├── depends on → dCommon (Data types, enums)
└── depends on → SQLite3 (Database driver)

dScripts
├── depends on → dGame/Entity (Script context)
└── depends on → dCommon (Enums, utilities)

dChatServer, dAuthServer, dMasterServer
├── depends on → dCommon (Serialization)
└── depends on → dNet (Protocol handling)
```

---

## Code Quality Indicators

### Strengths
1. **Modular Organization** - Clear separation of concerns with minimal cross-module coupling
2. **Component Architecture** - Highly extensible entity system allows adding new behaviors without touching core code
3. **Type Safety** - Template-based database query system provides compile-time type checking
4. **Documentation** - Most classes have comprehensive header documentation
5. **Consistent Patterns** - Message handling, serialization, and component lifecycle follow established patterns

### Areas for Improvement
1. **Global State** - Some singleton managers (EntityManager, PlayerManager) represent global coupling
2. **Legacy Integration** - Mix of C++17 modern code with older patterns in some modules
3. **Script-Engine Binding** - Script system could benefit from clearer object lifetime management
4. **Physics Integration** - dPhysics is somewhat disconnected from entity system

---

## ASCII Dependency Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      dZoneManager                            │
│              (Zone Loading & Spawning)                       │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │       dGame (Core)           │
        │  - Entity Component System   │
        │  - Game Logic                │
        │  - Message Routing           │
        │  - Script Integration        │
        └──┬──────────────┬────────┬───┘
           │              │        │
           ▼              ▼        ▼
      ┌────────┐  ┌────────────┐  ┌──────────┐
      │dScripts│  │dDatabase   │  │dPhysics  │
      │ (C++   │  │ - CDClient │  │(Collision)
      │ Logic) │  │ - GameDB   │  └──────────┘
      └────────┘  └──────┬─────┘
                         │
         ┌───────────────┴───────────────┐
         ▼                               ▼
    ┌────────────┐               ┌────────────┐
    │  dCommon   │               │   SQLite3  │
    │ - Enums    │               │  Databases │
    │ - Utilities│               └────────────┘
    │ - Math     │
    └────────────┘

  ┌────────┐ ┌────────────┐ ┌──────────────┐
  │ dNet   │ │ dChatServer│ │ dAuthServer  │
  │        │ │            │ │              │
  └────────┘ └────────────┘ └──────────────┘
      ▲            ▲               ▲
      └────────────┴───────────────┘
         (RakNet Protocol Layer)
```

