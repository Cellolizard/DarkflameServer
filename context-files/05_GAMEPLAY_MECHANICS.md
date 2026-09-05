> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> File:line citations are frozen at analysis time (committed `aa2de8e8`, 2026-05-19).
>
> **Rot in this file:** Later sections correctly say scripts are C++ `CppScripts::Script`. Hot-path claims (`GetEntitiesByComponent` O(n), per-frame skill multimap, ItemSet LIKE on every equip) are **false on this tip** (indexed lookup / in-place erase / process-wide cache landed). They remain true on `origin/main`. `GetEntitiesByProximity` is still O(n) on both. Dispatch on this tip goes through `MessageHandlerRegistry` before the leftover switch.

# DarkflameServer Gameplay Mechanics

## 1. Server Architecture

DarkflameServer is a distributed system with four primary server types that communicate via RakNet networking:

### Server Types

1. **Master Server** (`dMasterServer/`)
   - Central coordination hub for the entire system
   - Manages zone instance allocation and load balancing
   - Routes authentication sessions between Auth, Chat, and World servers
   - Tracks active game servers and player distribution
   - Communicates via port 1000 (default)

2. **World Server** (`dWorldServer/WorldServer.cpp`)
   - Runs per-zone or per-instance (one process per zone/clone)
   - Handles entity updates, physics, and game logic
   - Manages player movement, combat, and interactions
   - Loads zone data from CDClient and asset files
   - Communicates with Master and Chat servers
   - Default port: 2007 (configurable per zone/instance)

3. **Chat Server** (`dChatServer/`)
   - Manages chat messages and team systems
   - Handles social features (friends, ignore lists, teams)
   - Routes chat packets to World servers
   - Intercepts world-routable packets to deliver them to players
   - Connected to by World servers on startup

4. **Auth Server** (`dAuthServer/`)
   - Handles initial client authentication
   - Verifies credentials against the Game Database
   - Generates session keys passed through Master to World servers
   - Minimal role in ongoing gameplay

### Communication Protocol

- **Protocol**: RakNet (custom LEGO Universe protocol on top of RakNet)
- **Packet Format**: Binary bitstream with typed message headers
  - Message structure: `[ID_USER_PACKET_ENUM] [ServiceType] [InternalPacketID] [Payload]`
  - ServiceType: Enum indicating server type (MASTER, WORLD, CHAT, AUTH)
  - InternalPacketID: Message-specific ID for routing
- **Message Types** (in `dCommon/dEnums/MessageType/`):
  - `MessageType::Server` - Internal RakNet messages (connection/disconnection)
  - `MessageType::Master` - Master server messages
  - `MessageType::World` - World server messages
  - `MessageType::Chat` - Chat server messages
  - `MessageType::Game` - Game logic messages (skills, movement, etc.)
- **Serialization**: `RakNet::BitStream` (see `dNet/BitStreamUtils.h`)

---

## 2. Game Loop

### Location
`dWorldServer/WorldServer.cpp:335-537` (main loop runs inside `while(true)`)

### Frame Rate Control
- Configurable via `PerformanceManager::SelectProfile(zoneID)`
- Two frame rate modes:
  - **High Framerate**: 50ms per frame (20 FPS) during startup/low player count
  - **Normal Framerate**: 16ms per frame (60 FPS) when zone is active
- Frame delta tracked in milliseconds (`currentFrameDelta`)

### Tick Sequence (per frame)

```
1. Measure frame timing (deltaTime = elapsed time since last frame)
2. Check connection status to Master and Chat servers
3. [GAME LOGIC - only if zone != 0]:
   a. Update all entities (UpdateEntities with deltaTime)
   b. Step physics world (dpWorld::StepWorld)
   c. Update ghosting (visibility culling, once per second)
   d. Update spawners and zone manager
4. Handle incoming packets:
   a. Master server packets (SESSION_KEY_RESPONSE, SHUTDOWN, etc.)
   b. Chat server packets (WORLD_ROUTE_PACKET, GM_ANNOUNCE, etc.)
   c. World/client packets (up to 1024 packets, max 1.5s processing time)
5. Update replica objects (send updates to clients via RakNet)
6. Periodic tasks:
   - Flush logs (every 15s)
   - Save character data (every 10 minutes)
   - Ping SQL database (every 10 minutes)
   - Check for empty zone shutdown (30min for main worlds, 5min for clones)
7. Sleep until next frame time
8. Check if ready for clients (connected to Master, send WORLD_READY)
9. Handle shutdown sequence if signaled
```

### Key Timing Variables
- `deltaTime`: Wall-clock seconds elapsed since last frame
- `currentFrameDelta`: Target milliseconds per frame
- `currentFramerate`: Frames per second (frames in 1 second)
- Example: 16ms framerate = 60 FPS, calculated as `MS_TO_FRAMES(16)`

### Entity Update Loop
```cpp
Game::entityManager->UpdateEntities(deltaTime);  // All entity component updates
dpWorld::StepWorld(deltaTime);                    // Physics simulation
Game::zoneManager->Update(deltaTime);             // Spawner updates
```

---

## 3. Entity Component System (ECS)

### Base Entity Class
**Location**: `dGame/Entity.h`

The Entity class is a container that holds multiple components. It represents any object in the game world (players, NPCs, creatures, props, etc.).

```cpp
class Entity {
    LWOOBJID m_ObjectID;           // 64-bit unique object ID
    LOT m_TemplateID;              // LEGO Object Type (from CDClient)
    std::unordered_map<eReplicaComponentType, Component*> m_Components;
    Character* m_Character;        // Pointer to character data (nullptr for non-player entities)
    NiPoint3 m_DefaultPosition;    // Spawn position
    NiQuaternion m_DefaultRotation;
    float m_Scale;
    std::vector<LDFBaseData*> m_Settings;        // LDF format (key-value) settings
    std::vector<LDFBaseData*> m_NetworkSettings; // Settings sent to clients
};
```

### Component Management

**Adding Components**: Runtime composition via templates
```cpp
Entity* myEntity = new Entity(...);
auto physicsComponent = myEntity->AddComponent<PhysicsComponent>(/*args*/);
auto destroyableComponent = myEntity->AddComponent<DestroyableComponent>(/*args*/);
```

**Getting Components**:
```cpp
auto* physics = entity->GetComponent<PhysicsComponent>();
auto* skill = entity->GetComponent<SkillComponent>();
bool hasComponent = entity->HasComponent(eReplicaComponentType::SKILL);
```

**Component Types** (from `dGame/dComponents/`):

| Component | Purpose |
|-----------|---------|
| **PhysicsComponent** | Collider; RigidBody physics properties (not movement) |
| **ControllablePhysicsComponent** | Player-controlled character movement and position |
| **SimplePhysicsComponent** | Stationary/kinematic physics objects |
| **HavokVehiclePhysicsComponent** | Vehicle physics (cars, rockets) |
| **PhantomPhysicsComponent** | Trigger volumes (collision regions) |
| **RigidbodyPhantomPhysicsComponent** | Rigid trigger volumes |
| **MovementAIComponent** | NPC pathfinding and movement |
| **DestroyableComponent** | Health, armor, imagination, factions, death handling |
| **SkillComponent** | Skill/ability execution and behavior tree evaluation |
| **InventoryComponent** | Item storage, equipment, consumables |
| **CharacterComponent** | Character data (name, level, appearance, stats) |
| **MissionComponent** | Mission/quest tracking and progression |
| **PetComponent** | Pet ownership and behavior |
| **PossessorComponent** | Can possess other entities |
| **PossessableComponent** | Can be possessed by other entities |
| **ScriptComponent** | Attached C++ script (CppScripts::Script) |
| **BaseCombatAIComponent** | NPC combat behavior |
| **ActivityComponent** | Mini-game/activity participation |
| **RenderComponent** | Visual properties (color, effects, animations) |
| **ModelComponent** | 3D model references and LOD |
| **ItemComponent** | Item-specific properties (damage, rarity, etc.) |
| **QuickBuildComponent** | Quick-build (construction minigame) properties |
| **ModuleAssemblyComponent** | Modular build system |
| **PropertyComponent** | Player property (housing) data |
| **PropertyManagementComponent** | Singleton managing all properties in zone |
| **PropertyEntranceComponent** | Portal to property instance |
| **VendorComponent** | NPC vendor (shop) functionality |
| **AchievementVendorComponent** | Vendor for achievement rewards |
| **MissionOfferComponent** | NPC mission giver |
| **SwitchComponent** | On/off state and switch mechanics |
| **TriggerComponent** | Event triggers (on/off state changes) |
| **ProximityMonitorComponent** | Detects entities in radius and triggers events |
| **RailActivatorComponent** | Rail-based movement (rides, flying) |
| **MovingPlatformComponent** | Moving platform with waypoints |
| **RocketLaunchpadControlComponent** | Rocket minigame |
| **RacingControlComponent** | Racing minigame |
| **ShootingGalleryComponent** | Shooting gallery minigame |
| **LevelProgressionComponent** | Character level/skill progression |
| **BuffComponent** | Temporary stat buffs/debuffs |
| **GhostComponent** | Ghost (spirit) form properties |
| **BuildBorderComponent** | Build/destruction zone boundaries |
| **SoundTriggerComponent** | Sound event triggers |
| **LUPExhibitComponent** | LUP (legacy) exhibit display |
| **MultiZoneEntranceComponent** | Portal to different zone |
| **BouncerComponent** | Bouncer NPC interaction |
| **CollectibleComponent** | Collectible item data |
| **PlayerForcedMovementComponent** | Forced movement (cinematics, knockback) |
| **UGCModularBuild**, **UGC** | User-generated content models |

### Entity Serialization & Network Updates

**Network Packets**: Replica system sends component state to connected clients
```cpp
void Entity::WriteBaseReplicaData(RakNet::BitStream& outBitStream, eReplicaPacketType packetType);
void Entity::WriteComponents(RakNet::BitStream& outBitStream, eReplicaPacketType packetType) const;
```

**Packet Types** (`eReplicaPacketType`):
- `CONSTRUCTION`: Initial object creation (all component data)
- `SERIALIZATION`: Component updates (only changed data)
- `DESTRUCTION`: Object removal

**LDF Format**: Key-value data structure stored in entities
- Used for both local settings and network-synchronized properties
- LDF types: `LDFData<T>` (generic template) with various types (int32, bool, string, etc.)
- Retrieved via: `entity->GetVar<T>(u"key_name")`
- Set via: `entity->SetVar(u"key_name", value)` (local) or `SetNetworkVar` (networked)

---

## 4. Physics System

### Overview
DarkflameServer uses **Havok physics** (wrapped via `dpWorld`). The physics system handles:
- Collision detection
- Gravity and forces
- Rigid body dynamics
- Trigger volumes (phantoms)
- Character controller (movement)

### Location
- `dPhysics/dpWorld.h/cpp` - Physics world manager
- `dPhysics/dpEntity.*` - Physics entity wrapper
- `dNavigation/dNavMesh.*` - Navigation mesh for AI pathfinding
- Physics components: `ControllablePhysicsComponent`, `PhysicsComponent`, etc.

### Physics World Update
```cpp
dpWorld::Initialize(zoneID);      // Load zone physics, collision shapes, nav mesh
dpWorld::StepWorld(deltaTime);     // Step Havok simulation
```

### Movement
- **Player Movement**: `ControllablePhysicsComponent` receives position updates from client
- **AI Movement**: `MovementAIComponent` uses nav mesh pathfinding
- **Position Updates**: Tracked via `PositionUpdate` struct, serialized to clients

### Spatial Partitioning (Performance)
Configuration via `worldconfig.ini`:
```ini
phys_spatial_partitioning=1
phys_sp_tilesize=102              # Tile size (smaller = better performance, less balance)
phys_sp_tilecount=24              # Tile count (102/24 is half of LU's original 205/12 ratio)
```

---

## 5. Networking & Game Messages

### Packet Structure
All game packets follow this binary format:
```
[MessageID:1]
[ServiceType:1]      // MASTER, WORLD, CHAT, AUTH, etc.
[InternalPacketID:4] // Message type ID
[Padding:1]
[Payload:variable]
```

### Game Message Types
Location: `dGame/dGameMessages/`

**Serialization Framework**:
```cpp
struct GameMsg {
    MessageType::Game msgId;           // Unique message ID
    LWOOBJID target;                   // Target entity ID
    eGameMasterLevel requiredGmLevel;  // GM level required to send
    
    virtual void Serialize(RakNet::BitStream& bitStream) const {}
    virtual bool Deserialize(RakNet::BitStream& bitStream) { return true; }
    virtual void Handle(Entity& entity, const SystemAddress& sysAddr) {}
    bool Send();                        // Send to entity's player
    void Send(const SystemAddress& sysAddr) const;
};
```

### Example Game Messages
- `StartSkill` - Initiate a skill/ability
- `EchoStartSkill` - Echo skill cast to other players
- `SyncSkill` - Sync skill execution
- `DoClientProjectileImpact` - Projectile hit confirmation
- `PlayAnimation` - Play character animation
- `SetFaction` - Set NPC faction affiliation
- `Kill` - Kill entity
- `Teleport` - Move entity to position
- `NotifyObject` - Send named notification to entity's script

### Message Handler Dispatch
**Location**: `dGame/dGameMessages/GameMessageHandler.h`
```cpp
void GameMessageHandler::HandleMessage(
    RakNet::BitStream& inStream,
    const SystemAddress& sysAddr,
    LWOOBJID objectID,
    MessageType::Game messageID
);
```

The handler:
1. Looks up the target entity in `EntityManager`
2. Calls `entity->HandleMsg(msg)` to route to registered handlers
3. Entities register message handlers via: `entity->RegisterMsg(msgId, handler_function)`

---

## 6. Combat System

### Skill Execution
**Location**: `dGame/dComponents/SkillComponent.h`

#### Two Execution Modes:

**1. Client-Driven Skills** (player attacks)
```cpp
bool SkillComponent::CastPlayerSkill(
    uint32_t behaviorId,   // Root behavior tree ID
    uint32_t skillUid,     // Unique skill ID from client
    RakNet::BitStream& bitStream, // Client's behavior path choices
    LWOOBJID target,       // Explicit target
    uint32_t skillID       // Skill ID (for cooldowns, etc.)
);
```

**2. Server-Side Skills** (NPC attacks, passive effects)
```cpp
void SkillComponent::CalculateUpdate(float deltaTime);
```

### Behavior Trees
**Location**: `dGame/dBehaviors/`

Behaviors form tree structures that define skill behavior. Common behaviors:

| Behavior | Purpose |
|----------|---------|
| `BasicAttackBehavior` | Standard melee/ranged attack |
| `DamageReductionBehavior` | Apply armor/defense reduction to damage |
| `KnockbackBehavior` | Apply knockback/velocity to target |
| `DarkInspirationBehavior` | Imagination drain on hit |
| `ImaginationBehavior` | Imagination cost/restore |
| `RepairBehavior` | Restore armor to target |
| `JetPackBehavior` | Launch target upward |
| `SpeedBehavior` | Apply speed buff |
| `OverTimeBehavior` | Apply effect over time (heal, damage, buff) |
| `SpawnBehavior` | Spawn minion/effect entity |
| `AndBehavior` | Composite (run all child behaviors) |
| `NpcCombatSkillBehavior` | AI-specific combat behavior |
| `SwitchMultipleBehavior` | Branch based on conditions |

### Damage Calculation
**DestroyableComponent** handles health/damage:

```cpp
class DestroyableComponent : public Component {
    int32_t m_iHealth;              // Current health
    float m_fMaxHealth;
    int32_t m_iArmor;               // Current armor
    float m_fMaxArmor;
    int32_t m_iImagination;         // Current imagination
    float m_fMaxImagination;
    
    std::vector<int32_t> m_FactionIDs;  // Factions this entity belongs to
    
    void SetHealth(int32_t value);
    void Heal(uint32_t health);
    void SetArmor(int32_t value);
    void Repair(uint32_t armor);
    void SetImagination(int32_t value);
    void Imagine(int32_t deltaImagination);
};
```

**Damage Flow**:
1. Behavior tree executes `BasicAttackBehavior` or similar
2. Damage value calculated from behavior parameters
3. `DestroyableComponent::SetHealth()` called with new health
4. If health <= 0, entity is killed (calls `Kill()` or `Smash()`)

### Combat AI
**Location**: `dGame/dComponents/BaseCombatAIComponent.h`

NPC combat automation:
- Targets nearest enemy faction members
- Uses AI-specific behavior trees
- Updates every frame based on proximity and combat state
- Integrates with `SkillComponent` for ability execution

---

## 7. Mission/Quest System

### Location
`dGame/dComponents/MissionComponent.h`
`dGame/dMission/Mission.h`

### Mission Data Structure
```cpp
class MissionComponent : public Component {
    std::unordered_map<uint32_t, Mission*> m_Missions;  // mission ID -> Mission*
};

class Mission {
    uint32_t m_MissionID;
    eMissionState m_State;  // UNKNOWN, ACTIVE, COMPLETE_AVAILABLE, COMPLETE_INACTIVE
    std::unordered_map<uint32_t, MissionTask*> m_Tasks;
};

class MissionTask {
    eMissionTaskType m_Type;    // Task type (see below)
    int32_t m_Progress;         // Current progress
    int32_t m_Target;           // Target value
};
```

### Mission States
```
UNKNOWN               = 0
ACTIVE                = 1
COMPLETE_AVAILABLE    = 2  (can turn in)
COMPLETE_INACTIVE     = 3  (completed, turned in)
```

### Mission Task Types
Each task type tracks progress differently:

| Task Type | Purpose | Progress Type |
|-----------|---------|---------------|
| `DISCOVER` | Discover a location/item | Binary (found or not) |
| `SCRIPT` | Custom script-based progress | Integer value |
| `EMOTE` | Emote in front of NPC | Count |
| `EXPLORE` | Explore zone | Location trigger |
| `COLLECTION` | Collect X of item LOT | Item count |
| `COMBAT` | Kill X enemies | Enemy count |
| `LEVEL` | Reach level X | Current level |
| `ACTIVITY` | Complete activity | Activity completion |
| `MISSION_TASK` | Complete mission task | Progress % |
| `VISIT_PROPERTY` | Visit property | Property visit |
| `INTERACT` | Interact with entity | Count |
| `SKILL` | Use skill X times | Use count |

### Mission Flow
```cpp
// Accept mission
missionComponent->AcceptMission(missionId);

// Progress task
missionComponent->Progress(
    eMissionTaskType type,     // Task type to progress
    int32_t value,             // Progress value (LOT, count, etc.)
    LWOOBJID associate,        // Associated entity (optional)
    const std::string& targets // Target names (optional)
);

// Complete mission
missionComponent->CompleteMission(missionId);
```

### Persistence
Mission data saved in `charxml` table:
- Mission states and task progress stored in character XML
- Loaded from database on login via `LoadFromXml()`
- Updated via `UpdateXml()` when changed

---

## 8. Event System

### Event Types

**1. Physics-Based Events**
```cpp
// Phantom collider entry/exit
void OnCollisionPhantom(LWOOBJID otherEntity);
void OnCollisionLeavePhantom(LWOOBJID otherEntity);

// Proximity monitor
void OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status);
```

**2. Script Events** (routed through CppScripts::Script)
```cpp
void OnFireEventServerSide(Entity* sender, std::string args, int32_t param1, param2, param3);
void OnNotifyObject(Entity* sender, const std::string& name, int32_t param1, param2);
void OnMissionDialogueOK(Entity* sender, int missionID, eMissionState state);
void OnPlayerLoaded(Entity* player);
void OnPlayerDied(Entity* player);
```

**3. Game Message Events** (via GameMessages)
```cpp
entity->HandleMsg(gameMsg);  // Routes to registered handlers
```

### Event Subscriptions
Entities can subscribe to script notifications:
```cpp
entity->Subscribe(scriptObjId, scriptPtr, notificationName);
entity->Unsubscribe(scriptObjId, notificationName);
entity->NotifyObject(sender, notificationName, param1, param2);
```

### Event Dispatch
**Location**: `dGame/EntityManager.cpp`

- Physics events trigger via collision callbacks (registered with `ProximityMonitorComponent`)
- Script events called by entity's `ScriptComponent` 
- All events eventually call virtual methods on attached scripts

---

## 9. Scripting System

### Framework
**Location**: `dScripts/CppScripts.h`

Scripts are C++ classes that inherit from `CppScripts::Script`. Unlike the original Lua-based system, DarkflameServer uses compiled C++ scripts for performance.

### Script Base Class
```cpp
class Script {
    // Lifecycle
    virtual void OnStartup(Entity* self) {}
    virtual void OnShutdown(Entity* self) {}
    virtual void OnUpdate(Entity* self, float deltaTime) {}
    
    // Events
    virtual void OnCollisionPhantom(Entity* self, Entity* target) {}
    virtual void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, param2, param3) {}
    virtual void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, param2) {}
    virtual void OnPlayerLoaded(Entity* self, Entity* player) {}
    virtual void OnPlayerDied(Entity* self, Entity* player) {}
    virtual void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState state) {}
    
    // And many more...
};
```

### Attaching Scripts
```cpp
ScriptComponent* scriptComp = entity->AddComponent<ScriptComponent>(scriptName);
scriptComp->SetScript("ZonePath/ScriptName");  // Looks up in dScripts/ directory
```

### Script Registry
**Location**: `dScripts/CppScripts.cpp`

Scripts are registered in a global map by name:
```cpp
extern std::unordered_map<std::string, CppScripts::Script*> g_Scripts;
```

Scripts are loaded once per zone and reused across all entities with that script.

### Entity Variables
Scripts access per-entity data via:
```cpp
// Local variables (not networked)
int32_t value = self->GetVar<int32_t>(u"variable_name");
self->SetVar<int32_t>(u"variable_name", 42);

// Network variables (synced to clients)
self->SetNetworkVar(u"variable_name", value, sysAddr);
int32_t value = self->GetNetworkVar<int32_t>(u"variable_name");
```

### Example Script
```cpp
class ExampleScript : public CppScripts::Script {
    void OnStartup(Entity* self) override {
        self->AddTimer("check_proximity", 1.0f);  // Timer every 1 second
    }
    
    void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override {
        if (name == "check_proximity") {
            // Do something...
        }
    }
};
```

---

## Summary

DarkflameServer's gameplay mechanics are built on:
1. **Distributed server architecture** with Master, World, Chat, and Auth servers communicating via RakNet
2. **Fixed-frame game loop** with configurable framerates (20 or 60 FPS) handling entities, physics, and packets
3. **Component-based entity system** with 50+ component types providing specialized behavior
4. **Havok physics** for collision, gravity, and character movement
5. **Behavior tree-based combat system** with skill execution and damage calculation
6. **Mission/quest progression** with multiple task types and state tracking
7. **Event system** connecting physics, scripts, and game messages
8. **C++ scripting framework** for zone-specific and entity-specific logic

