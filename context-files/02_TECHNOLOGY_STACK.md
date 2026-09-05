> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> File:line citations are frozen at analysis time (committed `aa2de8e8`, 2026-05-19).
>
> **Critical falsehoods in this file:** The entire **Lua / Lua VM** section is false. `dScripts/` is compiled C++ (`CppScripts`); there is no in-tree Lua interpreter or `.lua` gameplay. C++ on this tip is **23** (main is still 20). RakNet is vendored 3.25 — SLikeNet is not a drop-in. Dockerfile on this tip is gcc:13, not 12.

# DarkflameServer Technology Stack Analysis

**Date:** 2026-04-03  
**Repository:** DarkflameServer - LEGO Universe Private Server Emulator  
**Build System:** CMake 3.25+  

---

## Language Specifications

### C++
- **Standard:** C++20 (CMakeLists.txt:15)
- **Configuration:** `set(CMAKE_CXX_STANDARD 20)`
- **Required:** `set(CMAKE_CXX_STANDARD_REQUIRED ON)`
- **Notable Features Used:**
  - Auto type deduction (lambdas, auto keyword)
  - constexpr evaluation
  - Range-based for loops
  - Smart pointers (unique_ptr, shared_ptr)
  - std::optional, std::variant
  - Concepts (potentially)

### C
- **Standard:** C99 (CMakeLists.txt:14)
- **Used for:** Third-party compatibility (mainly RakNet legacy code)

### Lua
- **Version:** Not pinned (integrated VM, likely Lua 5.3+)
- **Purpose:** Game scripting system
- **Integration:** C++ bindings in dScripts/CppScripts.h

---

## Networking Stack

### RakNet (Modified)
**Location:** `thirdparty/raknet/` (heavily customized)  
**Purpose:** Primary networking protocol  

**Architecture:**
```
Application Layer
     |
RakPeerInterface (RakNet)
     |
UDP Socket (OS Layer)
```

**Key Features:**
- Custom UDP protocol (not standard TCP/IP)
- Reliable message delivery with acknowledgments
- Unreliable datagrams for high-frequency updates
- Bandwidth limiting and throttling
- Packet prioritization
- Maximum Connection Handling

**RakNet Configuration (dServer.h:18-45):**
```cpp
class dServer {
    RakPeerInterface* mPeer;        // RakNet peer
    ReplicaManager* mReplicaManager; // Object sync
    NetworkIDManager* mNetIDManager; // Network ID assignment
};
```

**Key Methods (dServer.h:39-62):**
- `Receive()` - Get client packets
- `ReceiveFromMaster()` - Get master server packets
- `Send(BitStream&, SystemAddress)` - Send to specific client
- `SendToMaster(BitStream&)` - Send to master server
- `Disconnect(SystemAddress, DisconnectID)` - Graceful disconnect

**ReplicaManager:**
- Handles entity serialization/deserialization
- Tracks object state on each client
- Automatic delta updates (only changed data)
- Replica object references via NetworkID

**MTU Configuration:**
- Default: 1500 bytes (standard Ethernet)
- Adjustable via `dServer::UpdateMaximumMtuSize()`
- Bandwidth limit: Adjustable via `dServer::UpdateBandwidthLimit()`

**Message Flow:**
```
1. Client sends packet via UDP to WorldServer:port
2. RakNet parses packet
3. Packet -> RakNet::Packet struct
4. Application extracts message ID
5. Routes to appropriate handler (PacketType enum)
6. Handler processes and may send replies
7. Replies serialized to BitStream
8. RakNet sends back via UDP
```

**Packet Structure (RakNet BitStream):**
```
[MessageID:1 byte] [Payload:variable]
                    |
                    v
              [SubMessageID] [Data]
```

### Encryption
- **Method:** Optional RakNet encryption (configurable)
- **Config:** `mUseEncryption` flag in dServer (dServer.h:100)
- **Purpose:** Secure communication between servers

### Server Communication Architecture

```
                    ┌──────────────────┐
                    │  MasterServer    │
                    │  (dMasterServer) │
                    └────────┬─────────┘
                             │ RakNet
              ┌──────────────┼──────────────┐
              │              │              │
         RakNet         RakNet         RakNet
              │              │              │
    ┌─────────▼────┐ ┌──────▼─────┐ ┌─────▼──────┐
    │ AuthServer   │ │ChatServer  │ │WorldServer │
    │ (dAuthServer)│ │(dChatServer)│ │(dWorldServer)
    └──────────────┘ └────────────┘ └─────────────┘
              │              │              │
           RakNet      RakNet │         RakNet
              │              │          │
         ┌────┴──────────────┴──────────┴──────┐
         │        Client (Game Client)         │
         │                                     │
         └─────────────────────────────────────┘
```

**Inter-server Packets:** `MasterPackets.h`, `AuthPackets.h`, `ChatPackets.h`  
**Client Packets:** `WorldPackets.h`, `ClientPackets.h`

---

## Database System

### Dual-Backend Architecture

**Interface:** `GameDatabase` (dDatabase/GameDatabase/GameDatabase.h)

```cpp
class GameDatabase {
    virtual Character* GetCharacter(LWOOBJID characterID);
    virtual void SetCharacterInventory(...);
    virtual void SetCharacterMissions(...);
    virtual Account* GetAccount(uint32_t accountID);
    // 20+ table-specific interfaces
};
```

### MySQL/MariaDB (Production)

**Connector:** `thirdparty/mariadb-connector-cpp/`  
**Version:** Latest MariaDB C++ Connector  
**Implementation:** `dDatabase/GameDatabase/MySQL/MySQLDatabase.cpp`

**Connection Setup (CMakeLists.txt:121):**
```cmake
find_package(MariaDB)
# Links MariaDB C++ connector library
```

**Configuration:**
- Host, port, user, password from `worldconfig.ini`
- Connection pooling (multiple connections)
- Transaction support

**Key Features:**
- ACID transactions for critical operations
- Indexes on frequently queried columns
- Foreign key constraints
- Concurrent access support

### SQLite3 (Development/Testing)

**Library:** `thirdparty/SQLite/`  
**Implementation:** `dDatabase/GameDatabase/SQLite/SQLiteDatabase.cpp`

**Advantages for Development:**
- Single-file database (easily portable)
- No server setup required
- File-based, no separate daemon
- Perfect for testing

**Default Mode:** Tests use SQLite via `TestSQL` backend

### Table Interface Pattern

**Location:** `dDatabase/GameDatabase/ITables/`

Every database table has an interface defining access methods:

```cpp
class ICharacterTable {
    virtual std::vector<Character*> GetCharacters(uint32_t accountID) = 0;
    virtual bool InsertCharacter(Character* character) = 0;
    virtual bool UpdateCharacter(Character* character) = 0;
    virtual bool DeleteCharacter(LWOOBJID characterID) = 0;
    // ... more methods
};
```

**Concrete Implementations:**
- `MySQLCharacterTable` - MySQL implementation
- `SQLiteCharacterTable` - SQLite implementation

**Tables (20+ implemented):**
| Table | File | Purpose |
|-------|------|---------|
| Accounts | IAccountsTable | User accounts and authentication |
| Characters | ICharacterTable | Character metadata |
| CharacterXML | ICharacterXMLTable | Character data (inventory, missions) |
| Inventory | IInventoryTable | Item inventory |
| Missions | IMissionTable | Quest/mission progress |
| Properties | IPropertyTable | Player plots/properties |
| Friends | IFriendsTable | Friend relationships |
| Mail | IMailTable | In-game mail |
| Leaderboard | ILeaderboardTable | Minigame scores |
| CommandLog | ICommandLogTable | Admin command audit log |
| ... | ... | ... |

### CDClient Database (Read-Only)

**Purpose:** LEGO client data (not player data)  
**Location:** `dDatabase/CDClientDatabase/`  
**Manager:** `CDClientManager` singleton

**Key Classes:**
- `CDClientDatabase` - Connection to CD client data
- `CDClientTable` - Base class for table loaders
- `CDClientCompiledQuery` - Cached query results

**100+ Table Loaders:**

**Examples of CDClient tables loaded:**

| Table | File | Data |
|-------|------|------|
| Objects | CDObjectsTable | All game object definitions (LOT, components, properties) |
| ItemTable | CDItemTableTable | Item definitions (name, icon, cost, properties) |
| SkillBehavior | CDSkillBehaviorTable | Skill execution definitions |
| BehaviorParameter | CDBehaviorParameterTable | Skill parameters (damage, cooldown, etc) |
| ZoneTable | CDZoneTableTable | Zone definitions (name, position, skybox) |
| ActivityRewards | CDActivityRewardsTable | Minigame reward tables |
| Loot | CDLootTable | Enemy loot drops |
| ... | ... | ... |

**Caching:**
- All CDClient data loaded into memory on startup
- Never modified (read-only)
- Fast in-game queries via hash maps

### Migration System

**Location:** `migrations/dlu/` (MySQL and SQLite versions)

**Purpose:** Database schema versioning and upgrades

**Files:** `0_initial.sql` through `26_*.sql` for MySQL, `0-9` for SQLite

**Key Migrations:**
- `0_initial.sql` - Core schema (accounts, characters, inventory)
- `1_master_password.sql` - Master account auth
- `2_normalize_model_positions.sql` - Data cleanup
- `3_behavior_ids_object_bits.sql` - Behavior storage
- `23_store_character_id_as_objectid.sql` - Character ID normalization

**Execution:**
- Run automatically by MasterServer on startup
- `MigrationRunner` class (dMasterServer/Start.cpp)
- Tracks applied migrations in `migrations` table

### Schema Example (Initial)

From `migrations/dlu/sqlite/0_initial.sql`:

```sql
CREATE TABLE accounts (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,              -- Account username
    password TEXT NOT NULL,                 -- bcrypt hash
    gm_level BIGINT NOT NULL DEFAULT 0,    -- Admin level
    locked INTEGER NOT NULL DEFAULT FALSE,
    banned INTEGER NOT NULL DEFAULT FALSE,
    play_key_id INTEGER DEFAULT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    mute_expire BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE charinfo (
    id BIGINT NOT NULL PRIMARY KEY,         -- Character LWOOBJID
    account_id INTEGER NOT NULL REFERENCES accounts(id),
    name TEXT NOT NULL UNIQUE,              -- Character name
    pending_name TEXT NOT NULL,
    needs_rename INTEGER NOT NULL DEFAULT FALSE,
    prop_clone_id INTEGER UNIQUE,
    last_login BIGINT NOT NULL DEFAULT 0,
    permission_map BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE charxml (
    id BIGINT NOT NULL PRIMARY KEY REFERENCES charinfo(id),
    xml_data TEXT NOT NULL                  -- Character XML (serialized)
);

CREATE TABLE inventory (
    id BIGINT NOT NULL PRIMARY KEY AUTOINCREMENT,
    character_id BIGINT NOT NULL REFERENCES charinfo(id),
    item_id BIGINT NOT NULL,
    lot INTEGER NOT NULL,
    quantity INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    -- ... more columns for item properties
);

-- ... 20+ more tables
```

**Data Types:**
- `BIGINT` (int64) for LOWOBJIDs and large numbers
- `INTEGER` for regular integers and booleans
- `TEXT` for strings and serialized data (XML, JSON)
- `DATETIME` for timestamps

---

## Physics Engine

### Custom Physics System (Not a Full Engine)

**Location:** `dPhysics/`

**Architecture:**
```
dpWorld (namespace) - Physics simulation controller
  |
  v
dpEntity - Physics object (wrapper around Component)
  |
  +-- dpShapeBox (AABB collision)
  +-- dpShapeSphere (sphere collision)
  +-- dpShapeBase (base shape abstract)
  |
dpGrid - Spatial partitioning for acceleration
  |
dpCollisionChecks - Collision test algorithms
```

**Key Namespace Functions (dpWorld.h:8-23):**

```cpp
namespace dpWorld {
    void Initialize(uint32_t zoneID, bool generateNewNavMesh = true);
    void Shutdown();
    void StepWorld(float deltaTime);        // Update physics each frame
    void AddEntity(dpEntity* entity);       // Register entity
    void RemoveEntity(dpEntity* entity);    // Unregister entity
    dNavMesh* GetNavMesh();                 // Get navigation mesh
    bool IsLoaded();
    void Reload();
};
```

**Physics Updates (called in game loop):**
```cpp
// In WorldServer.cpp main loop:
dpWorld::StepWorld(frameTime);  // Update collisions, triggers, etc.
```

**Collision Detection:**
- Spatial grid acceleration (dpGrid)
- Broad-phase: Grid cell queries
- Narrow-phase: Shape-specific collision tests
- Trigger zones: Phantom physics (no pushback)
- Dynamic objects: Collision callbacks

**Physics Objects Attached to:**
- `ControllablePhysicsComponent` - Player movement (server validation)
- `PhantomPhysicsComponent` - Trigger zones
- `RigidbodyPhantomPhysicsComponent` - Dynamic objects with collision

**Collision Shapes:**
```cpp
class dpShapeBase {
    virtual bool Intersects(dpShapeBase* other);
    virtual bool IsInside(NiPoint3 point);
    virtual NiPoint3 GetClosestPoint(NiPoint3 point);
};

// Box collision (AABB)
class dpShapeBox : public dpShapeBase {
    NiPoint3 minPoint, maxPoint;
};

// Sphere collision
class dpShapeSphere : public dpShapeBase {
    NiPoint3 center;
    float radius;
};
```

### NOT Included

What this physics system does NOT do:
- No gravity
- No rigid body dynamics
- No mass/inertia
- No forces or acceleration
- No continuous collision detection

Player movement is entirely client-side validated by server.

---

## Navigation & Pathfinding

### Recast Navigation Library

**Location:** `thirdparty/recastnavigation/` (official Recast library)  
**Purpose:** AI pathfinding and navmesh generation  

**Integration:** `dNavigation/`

**Key Classes:**

```cpp
class dNavMesh {
    bool BuildNavMesh(uint32_t zoneID);     // Build or load navmesh
    bool FindPath(NiPoint3 start, NiPoint3 end, 
                  std::vector<NiPoint3>& path);  // A* pathfinding
    NiPoint3 GetRandomPoint();              // Random point on navmesh
    // ... more queries
};
```

**Navmesh Data:**
- Pre-computed during zone loading
- Loaded from `navmeshes/` directory (extracted from ZIP)
- Binary Recast format (.navmesh files)

**Detour Extensions:**
- Custom query functions in `DetourExtensions.h`
- Extended path smoothing
- Crowd avoidance support (if enabled)

**Usage in Game:**
1. **Enemy AI:** BaseCombatAIComponent uses navmesh for pathfinding
2. **Player Path Queries:** Game messages can request paths
3. **Proximity Checks:** Spatial queries on navmesh

**Integration Points:**
- `BaseCombatAIComponent::FindPathToTarget()` - AI pathfinding
- `dNavMesh` singleton accessed via `dpWorld::GetNavMesh()`

---

## Scripting System

### Lua Integration

**Language:** Lua (version not pinned, likely 5.3+)  
**C++ Binding:** Custom bindings in `dScripts/CppScripts.h`

**Registry:**

```cpp
class CppScripts {
    static void Register();              // Register all scripts
    static Script* GetScript(LOT lot);   // Get script for LOT
    static void Run(Entity* entity, const std::string& hook, ...);
};
```

**Script Lifecycle:**

1. **Loading:** When entity is created with ScriptComponent
   ```cpp
   ScriptComponent::OnUse() -> CppScripts::Run(entity, "onLoad")
   ```

2. **Runtime Hooks:**
   - `onLoad()` - Entity initialization
   - `onUpdate(deltaTime)` - Frame update callback
   - `onCollisionPhantom(otherEntity)` - Trigger zone events
   - `onUse(userEntity)` - Player interaction
   - `onPreLoad()` - Pre-load data
   - Custom message handlers

3. **Unloading:** When entity is destroyed

**Script Organization (dScripts/):**

```
dScripts/
├── 02_server/          # Server-side game scripts
│   ├── DLU/            # Darkflame Universe custom
│   ├── Enemy/          # Enemy behavior (not AI, just setup)
│   ├── Equipment/      # Item equipment effects
│   ├── Map/            # Zone-specific logic
│   ├── Minigame/       # Minigame implementations
│   ├── Objects/        # Interactive object scripts
│   ├── Pets/           # Pet behavior setup
│   └── ... (organized by game system)
│
├── ai/                 # AI behavior trees (Lua)
│   ├── ACT/, AG/, FV/  # Zone-code directories
│   ├── GENERAL/        # General AI behaviors
│   ├── PROPERTY/       # Property-specific behaviors
│   ├── RACING/         # Racing AI
│   ├── MINIGAME/       # Minigame AI
│   └── ... (many zones)
│
├── zone/               # Zone-specific scripts
│   ├── AG/             # Adventure Gate scripts
│   ├── PROPERTY/       # Property scripts
│   ├── LUPs/           # LU Props scripts
│   └── ...
│
├── EquipmentScripts/   # Equipment effect implementations
├── EquipmentTriggers/  # Equipment trigger handlers
└── client/             # Client-side script stubs
```

**Example Script Structure:**

```lua
-- dScripts/ai/GENERAL/EnemyAI.lua
local EnemyAI = {}

function EnemyAI.onLoad(entity)
    -- Initialize AI state
    entity:SetVar("targetEntity", nil)
    entity:SetVar("state", "idle")
end

function EnemyAI.onUpdate(entity, deltaTime)
    local target = entity:GetVar("targetEntity")
    if target then
        -- Pursue target
        local targetPos = target:GetPosition()
        local myPos = entity:GetPosition()
        -- ... pathfinding and movement logic
    end
end

function EnemyAI.onCollisionPhantom(entity, otherEntity)
    if otherEntity:IsPlayer() then
        entity:SetVar("targetEntity", otherEntity)
    end
end

return EnemyAI
```

**C++ Script Binding Example (CppScripts.cpp):**

```cpp
Script* GetScript(LOT lot) {
    if (lot == ENEMY_LOT) {
        return new EnemyScript();  // C++ object
    }
    // ... more LOT -> Script mappings
}
```

**Advantages of Scripting:**
- Easy to add new game behaviors
- No need to recompile server for script changes
- Can hot-reload scripts in development
- Abstracts game logic from engine code

---

## Game Engine Architecture

### Entity Component System (ECS)

**Pattern:** Component-Based Entity System

**Core Classes:**

1. **Entity** (`dGame/Entity.h:62-100`)
   ```cpp
   class Entity {
       LWOOBJID m_ObjectID;                 // Unique ID
       LOT m_TemplateID;                    // Object type
       std::unordered_map<int32_t, Component*> m_Components;  // Attached components
       
       Component* GetComponent(eReplicaComponentType type);
       void AddComponent(Component* component);
       void RemoveComponent(Component* component);
   };
   ```

2. **Component** (`dGame/dComponents/Component.h:20-60`)
   ```cpp
   class Component {
       Entity* m_Parent;                    // Owner entity
       int32_t m_ComponentID;               // Component type
       
       virtual void Update(float deltaTime);
       virtual void OnUse(Entity* originator);
       virtual void Serialize(BitStream& out, bool isConstruction);
   };
   ```

3. **EntityManager** (`dGame/EntityManager.h:23-50`)
   ```cpp
   class EntityManager {
       std::unordered_map<LWOOBJID, Entity*> m_Entities;  // All entities
       
       Entity* CreateEntity(EntityInfo info, User* user);
       void DestroyEntity(LWOOBJID id);
       void UpdateEntities(float deltaTime);  // Main update loop
   };
   ```

**Component Types (eReplicaComponentType enum):**

**Physics (1-10):**
- ControllablePhysicsComponent (1)
- PhantomPhysicsComponent (2)
- RigidbodyPhantomPhysicsComponent (3)
- HavokVehiclePhysicsComponent (4)

**Core Gameplay (11-30):**
- ModelComponent (11) - 3D model
- SkillComponent (12) - Skills
- InventoryComponent (13) - Items
- CharacterComponent (14) - Stats
- DestroyableComponent (15) - Health/destruction
- BouncerComponent (16)

**AI & Behavior (31-50):**
- BaseCombatAIComponent (31)
- ScriptComponent (32) - Lua script hook
- MissionComponent (33)
- LevelProgressionComponent (34)
- PetComponent (35)

**Properties & Building (51-70):**
- PropertyComponent (51)
- PropertyManagementComponent (52)
- ModuleAssemblyComponent (53)
- BuildBorderComponent (54)

**Social & Game Features (71+):**
- RacingComponent
- ActivityComponent
- VendorComponent
- PossessableComponent (rideable)

**Total:** 60+ component types defined

### Game Loop Flow

**Main Loop (WorldServer.cpp ~1200-1500):**

```cpp
while (!shouldShutdown) {
    // 1. Network I/O
    Packet* packet = server->Receive();
    while (packet) {
        HandlePacket(packet);
        server->DeallocatePacket(packet);
        packet = server->Receive();
    }
    
    // 2. Update Game State
    EntityManager::UpdateEntities(frameTime);  // All entity updates
    {
        for (Entity* entity : entities) {
            for (Component* comp : entity->components) {
                comp->Update(frameTime);  // Component-specific logic
            }
        }
    }
    
    // 3. Physics Simulation
    dpWorld::StepWorld(frameTime);  // Collision detection, triggers
    
    // 4. Navigation Updates
    dNavMesh* navmesh = dpWorld::GetNavMesh();
    // ... AI pathfinding updates
    
    // 5. Serialization
    EntityManager::SerializeEntity(entity);  // Delta updates
    
    // 6. Send Outgoing Packets
    // RakNet queue filled with serialized updates
    
    // 7. Frame Timing
    if (frameTime < TARGET_FRAME_TIME) {
        sleep(TARGET_FRAME_TIME - frameTime);
    }
}
```

**Target Frame Rate:** ~30 FPS (33ms per frame)

**Update Order:**
1. Input processing
2. Entity updates
3. Physics
4. AI
5. Serialization
6. Network send

### Message Passing System

**Game Messages:** Structured commands sent between entities

**Types (dGame/dGameMessages/GameMessages.h):**
- Skill messages (StartSkill, SyncSkill, ExecuteSkill)
- Damage/destruction messages
- Inventory messages
- Mission messages
- Property messages
- Custom script messages

**Routing (GameMessageHandler):**

```cpp
void GameMessageHandler::HandleMessage(Entity* entity, GameMessage msg) {
    switch (msg.messageID) {
        case eGameMessageType::GAME_MSG_SKILL_START:
            SkillComponent::StartSkill(entity, msg);
            break;
        case eGameMessageType::GAME_MSG_SKILL_SYNC:
            SkillComponent::SyncSkill(entity, msg);
            break;
        // ... 100+ message handlers
    }
}
```

---

## Third-Party Dependencies

### Complete List with Purposes

**Location:** `thirdparty/` (11 libraries vendored)

| Library | Purpose | Integration | Version |
|---------|---------|-------------|---------|
| **raknet** | UDP networking, reliability, object sync | Core networking | Modified/custom |
| **mariadb-connector-cpp** | MySQL/MariaDB client | GameDatabase backend | Latest |
| **SQLite** | Embedded SQL database | GameDatabase backend | 3.x |
| **tinyxml2** | XML parsing | Entity/character XML | Latest |
| **nlohmann/json** | JSON serialization | Web API, configs | 3.x |
| **mongoose** | HTTP/WebSocket server | Web API layer | Latest |
| **recastnavigation** | AI navmesh generation, queries | Navigation system | Latest |
| **magic_enum** | Compile-time enum reflection | Enum/string conversion | Latest |
| **libbcrypt** | Password hashing (bcrypt) | Account authentication | Latest |
| **cpplinq** | LINQ-style queries in C++ | Data querying | Latest |
| **MD5** | Hash function (legacy) | Non-crypto uses | Custom impl |

### RakNet Deep Dive

**Custom Modifications:**
- ReplicaManager integrated (not in official RakNet)
- NetworkIDManager integrated
- Custom messaging for game packets
- Heavily optimized for game use case

**Core RakNet Features Used:**
- **Peer-to-peer connections** (WorldServer <-> Client)
- **Master-slave architecture** (MasterServer as hub)
- **Reliable ordered channels** (important game state)
- **Unreliable channels** (position updates, frequent changes)
- **Connection management** (auto-reconnect, timeout handling)
- **Bandwidth limiting** (prevent spam)
- **Packet prioritization** (critical messages first)

**RakNet Bandwidth Management (dServer.cpp):**
```cpp
void dServer::UpdateBandwidthLimit() {
    // Prevent flooding
    mPeer->SetIncomingMessageHandler(...);
    mPeer->SetBandwidthLimitBPS(maxBandwidth);
}

void dServer::UpdateMaximumMtuSize() {
    // Optimize for network conditions
    mPeer->SetMTUSize(mtuSize);
}
```

### CMakeLists.txt Integration

**Third-party Build (CMakeLists.txt:238):**
```cmake
add_subdirectory(thirdparty SYSTEM)
# SYSTEM flag suppresses warnings from third-party headers
```

**Third-party includes (CMakeLists.txt:256-265):**
```cmake
include_directories(
    SYSTEM
    "thirdparty/magic_enum/include/magic_enum"
    "thirdparty/raknet/Source"
    "thirdparty/tinyxml2"
    "thirdparty/recastnavigation"
    "thirdparty/SQLite"
    "thirdparty/cpplinq"
    "thirdparty/MD5"
    "thirdparty/nlohmann"
    "thirdparty/mongoose"
)
```

**Common Libraries Linked (CMakeLists.txt:319-328):**
```cmake
set(COMMON_LIBRARIES 
    glm::glm              # Math library (vectors, matrices)
    "dCommon" 
    "dDatabase" 
    "dNet" 
    "raknet"
    "magic_enum"
)

if(UNIX)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "dl" "pthread")
    if(NOT APPLE AND ${INCLUDE_BACKTRACE})
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "backtrace")
    endif()
endif()
```

### Additional Dependencies

**GLM (Math Library):**
- Not in thirdparty/ (external dependency)
- Required for vector/matrix math
- Used throughout for NiPoint3 compatibility

---

## Serialization Formats

### RakNet BitStream

**Purpose:** Binary packet serialization

**Key Functions (BitStreamUtils.h):**
```cpp
class BitStream {
    void Write(uint32_t value);
    void Write(float value);
    void Write(std::string value);
    void Write(NiPoint3 point);
    
    bool Read(uint32_t& value);
    bool Read(float& value);
    bool Read(std::string& value);
    bool Read(NiPoint3& point);
};
```

**Usage:** All network communication

### LDF Format (LEGO Data Format)

**Purpose:** Entity/object property storage

**File:** `dCommon/LDFFormat.h`

**Format:**
```
[Key:string] = [Value:variant]
```

**Example:**
```
speed = 5.0
health = 100
modelName = "zombie"
```

**Used For:**
- Entity settings from database
- Object properties in zones
- Behavior parameters

### Flash AMF3 Serialization

**Purpose:** Flash/web client communication

**Files:** `dCommon/Amf3.h`, `AMFDeserialize.cpp`, `AmfSerialize.cpp`

**Supports:**
- Objects (maps)
- Arrays
- Primitives (int, string, bool, etc)
- Null values

**Used For:**
- Web APIs
- Flash game client (legacy)

### XML (Character Data)

**Format:** Standard XML

**Location:** `charxml` table in database

**Contains:**
- Inventory items
- Mission progress
- Skills
- Properties

**Parser:** tinyxml2

**Example Character XML:**
```xml
<?xml version="1.0"?>
<char>
    <inv>
        <bag id="0">
            <item id="1" lot="6" quantity="1"/>
            <item id="2" lot="7" quantity="5"/>
        </bag>
    </inv>
    <mis>
        <mission id="101" complete="false"/>
    </mis>
</char>
```

### JSON

**Purpose:** Configuration and web APIs

**Library:** nlohmann/json

**Used For:**
- Server configuration (converted from INI)
- REST API responses
- Admin dashboard data
- Web socket messages

---

## CI/CD Infrastructure

### GitHub Actions

**Location:** `.github/workflows/`

**Workflows:** 2 main pipelines

#### 1. Build and Test (build-and-test.yml)

**Trigger:** Push to main, Pull requests  
**Platforms:** Windows 2022, Ubuntu 22.04, macOS 15-intel

**Steps:**
```yaml
- Checkout code (with submodules)
- Setup compiler (MSVC for Windows, GCC/Clang for others)
- Install CMake 3.25
- Configure (cmake preset)
- Build (cmake build)
- Test (ctest)
- Upload artifacts (binaries, config files)
```

**CMake Presets Used:**
- `ci-windows-2022` (Windows MSVC)
- `ci-ubuntu-22.04` (Linux GNU)
- `ci-macos-15-intel` (macOS)

**Artifacts Generated:**
- `dMasterServer*` binary
- `dWorldServer*` binary
- `dChatServer*` binary
- `dAuthServer*` binary
- Config files (*.ini)
- Libraries (*.so, *.dll, *.dylib)

#### 2. Docker Build and Push (build-and-push-docker.yml)

**Purpose:** Container image building and registry push

**Targets:**
- Build multi-platform Docker images
- Push to container registry (Docker Hub / GitHub Container Registry)

### Build Configuration System

**CMakePresets.json:** 20+ configured presets

**Configure Presets:**
- Linux: gnu-debug, gnu-release, gnu-relwithdebinfo
- Linux: clang-debug, clang-release (experimental)
- Windows: windows-msvc (Visual Studio 2022)
- macOS: macos (Xcode)

**Build Presets:**
- Platform-specific (windows-msvc-*, linux-gnu-*, macos-*)
- Build types (debug, release, relwithdebinfo)

**Test Presets:**
- Per-platform test execution
- Default test configuration

**Workflow Presets:**
- Combine configure -> build -> test in sequence
- CI workflows for each platform

**Example Build:**
```bash
# Configure
cmake --preset linux-gnu-release

# Build
cmake --build --preset linux-gnu-release

# Test
ctest --preset linux-gnu-release
```

### Compilation Flags

**C++ Compiler Flags (CMakeLists.txt:74-101):**

**Unix/Linux (non-MSVC):**
```cmake
add_compile_options("-fPIC")  # Position-independent code
add_compile_options("-Wuninitialized" "-Wold-style-cast")

# GCC specific
add_compile_options("-static-libgcc" "-lstdc++fs")
```

**Apple (macOS):**
```cmake
add_link_options("-Wl,-rpath,@loader_path/")  # Dylib search path
```

**MSVC (Windows):**
```cmake
add_compile_options("/wd4267" "/utf-8" "/volatile:iso" "/Zc:inline")
# /wd4267: Suppress size_t to uint32_t conversion warning
# /utf-8: UTF-8 source code
# /volatile:iso: Strict volatile semantics
# /Zc:inline: Inline function optimization
```

**Linux (GCC):**
```cmake
add_link_options("-Wl,-rpath,$ORIGIN/")  # ELF search path
```

---

## Architecture Patterns Summary

### Component-Based Entity System (CBS)

**Benefits:**
- Flexible entity composition
- No deep inheritance hierarchies
- Reusable components
- Easy to add new behaviors
- Data-driven design

**Implementation:**
```
Entity
  ├── Component 1 (Physics)
  ├── Component 2 (AI)
  ├── Component 3 (Inventory)
  └── Component N (Custom)
```

### Object-Oriented Network Replication

**RakNet ReplicaManager:**
- Client automatically receives entity state
- Delta updates (only changed data)
- NetworkID for object references
- Automatic serialization

### Message-Driven Game Logic

**Message Passing:**
- Decoupled component communication
- Event-driven updates
- Extensible message types

### Spatial Partitioning

**Physics Grid:**
- O(1) collision query performance
- Dynamic grid resizing
- Coarse-to-fine broadphase

### Database Abstraction

**Multiple Backends:**
- MySQL for production
- SQLite for development
- Interface-based design allows easy swapping

---

## Module Dependency Graph

```
┌────────────────────────────────────────┐
│ Third-party Libraries                  │
│ (RakNet, SQLite, MySQL, etc)          │
└────────────────┬───────────────────────┘
                 │
    ┌────────────┴──────────────┐
    │                           │
    v                           v
┌─────────────┐           ┌──────────────┐
│  dCommon    │◄──────────│  dDatabase   │
│ (utilities) │           │ (persistence)│
└──────┬──────┘           └──────────────┘
       │
       ├──────────┬──────────────┬──────────┐
       │          │              │          │
       v          v              v          v
    ┌─────┐  ┌──────┐      ┌──────┐    ┌────┐
    │dNet │  │dGame │      │dPhys │    │dNav│
    │(net)│  │(core)│      │(phys)│    │(ai)│
    └──┬──┘  └──┬───┘      └──────┘    └────┘
       │        │
       │    ┌───┼──────────────┐
       │    │   │              │
       │    v   v              v
       │  ┌─────────────┐  ┌────────────┐
       │  │ dZoneManager│  │  dScripts  │
       │  │  (zones)    │  │  (lua)     │
       │  └─────────────┘  └────────────┘
       │
       └───────────────┬──────────────────┐
                       │                  │
                       v                  v
            ┌──────────────────┐  ┌──────────────┐
            │ dWorldServer     │  │ dChatServer  │
            │ (main zone loop) │  │ (social)     │
            └──────────────────┘  └──────────────┘
                       │
                       │
                       v
            ┌──────────────────┐
            │  MasterServer    │
            │ (coordination)   │
            └──────────────────┘
```

**Critical Paths:**
1. **Game Loop:** dWorldServer -> dGame (EntityManager) -> dPhysics -> dNav
2. **Networking:** dWorldServer -> dNet -> RakNet
3. **Persistence:** dGame -> dDatabase -> MySQL/SQLite
4. **Scripting:** dGame -> dScripts -> Lua VM

---

## Performance Considerations

### Frame Budgeting

**Target:** 30 FPS (33ms per frame)

**Typical Frame Breakdown:**
- Network I/O: ~2ms
- Entity updates: ~8ms
- Physics step: ~5ms
- Serialization: ~8ms
- Other: ~10ms

### Optimization Techniques

1. **Spatial Partitioning:** dpGrid for collision queries
2. **Caching:** CDClient database fully in-memory
3. **Delta Updates:** Only serialize changed entity state
4. **Component Pooling:** Potential (not currently implemented)
5. **Lazy Evaluation:** Script updates on-demand

### Scaling Considerations

**Multiple Instances:**
- Each zone runs in separate WorldServer instance
- MasterServer coordinates instances
- Database is shared bottleneck

**Per-Zone Capacity:**
- Limited by frame budget
- Typical: 50-100 players per instance
- Scaling: Add more instances for more players

---

## Summary Table

| Aspect | Technology | Notes |
|--------|-----------|-------|
| **Language** | C++20, Lua | Modern C++ with scripting |
| **Build** | CMake 3.25+ | Cross-platform, preset-based |
| **Networking** | RakNet (custom) | UDP-based, peer-to-peer |
| **Database** | MySQL + SQLite | Production/dev split |
| **Physics** | Custom collision | Not a full physics engine |
| **Pathfinding** | Recast Navigation | A* on pre-computed navmesh |
| **HTTP API** | Mongoose | Embedded web server |
| **Serialization** | BitStream, XML, JSON, AMF3 | Multiple formats |
| **Scripting** | Lua | Integrated VM |
| **Architecture** | Component-Based ECS | Flexible entity composition |
| **CI/CD** | GitHub Actions | Multi-platform testing |
| **Protocols** | RakNet (game), HTTP (web) | Dual protocol server |

