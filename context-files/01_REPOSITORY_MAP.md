# DarkflameServer Repository Map

**Project:** LEGO Universe Private Server Emulator (Darkflame)  
**Language:** C++20 / C99  
**Build System:** CMake 3.25+  
**Platforms:** Windows (MSVC), Linux (GCC/Clang), macOS  

## Quick Start
```bash
cmake --preset default
cmake --build --preset default
# Outputs binaries and config files to build/
```

---

## Complete Directory Structure

### Root Level

```
DarkflameServer/
├── dGame/                    # Core game logic and entity systems (341 files)
├── dDatabase/                # Database abstraction layer (168 files)
├── dScripts/                 # Lua scripting system for gameplay (613 files)
├── dCommon/                  # Shared utilities and common code (124 files)
├── dNet/                     # Network packet definitions and RakNet wrapper (21 files)
├── dServer/                  # Base server implementation (Server.cpp/h)
├── dZoneManager/             # Zone/world instance management (11 files)
├── dPhysics/                 # Custom physics system using navmesh (14 files)
├── dNavigation/              # Navigation mesh (Recast) integration (10 files)
├── dWorldServer/             # World server entry point and main loop
├── dChatServer/              # Chat server implementation (13 files)
├── dAuthServer/              # Authentication server (1 file)
├── dMasterServer/            # Master server (instance/server management) (5 files)
├── dWeb/                     # HTTP/WebSocket API layer (2 files)
├── dChatFilter/              # Chat content filtering (2 files)
├── thirdparty/               # Third-party libraries (vendored)
├── migrations/               # Database schema migrations (SQL)
├── tests/                    # Unit tests
├── docs/                     # Documentation
├── resources/                # Config files, navmesh data, vanity files
├── .github/workflows/        # CI/CD pipelines
└── cmake/                    # CMake modules and toolchains
```

---

## Module Details

### Core Game Logic: `dGame/` (341 .cpp/.h files)

The heart of the game simulation. Uses **component-based architecture** where entities have multiple components.

```
dGame/
├── Entity.h / Entity.cpp          # Base entity class - every game object is an Entity
├── EntityManager.h / EntityManager.cpp  # Global entity factory and update manager
├── dComponents/                   # 70+ component types (341 headers/sources)
│   ├── Component.h               # Base component abstract class
│   ├── DestroyableComponent.*    # Health, immunity, armor, destruction
│   ├── SkillComponent.*          # Skill execution system
│   ├── InventoryComponent.*      # Item management
│   ├── CharacterComponent.*      # Character stats (level, currency, etc)
│   ├── ControllablePhysicsComponent.*  # Player movement control
│   ├── PhantomPhysicsComponent.* # Non-moving collision volumes
│   ├── ModelComponent.*          # 3D model rendering
│   ├── PropertyComponent.*       # Player property/plot ownership
│   ├── RacingComponent.*         # Racing/racing stats
│   ├── PossessableComponent.*    # Rideable creatures
│   ├── ScriptComponent.*         # Lua script execution trigger
│   ├── PetComponent.*            # Pet AI and behavior
│   ├── BaseCombatAIComponent.*   # Enemy AI combat
│   ├── InventoryComponent.*      # Item management
│   ├── MissionComponent.*        # Mission/quest tracking
│   ├── LevelProgressionComponent.*  # Experience/leveling
│   └── ... (60+ more components)
├── dGameMessages/                 # Game message handling
│   ├── GameMessages.h / GameMessages.cpp  # Message definitions and handlers
│   ├── GameMessageHandler.*       # Routes game messages to correct handlers
│   └── ... (property, skill, activity messages)
├── dMission/                      # Mission/quest system
├── dInventory/                    # Inventory and item management
├── dUtilities/                    # Game utilities
│   └── SlashCommands/             # Command handler implementations
├── dBehaviors/                    # Behavior scripting system for complex objects
├── dPropertyBehaviors/            # Property-specific behaviors
│   └── ControlBehaviorMessages/   # Blueprint/building control
├── dEntity/                       # Entity-specific utilities
│   ├── EntityInfo.h              # Entity spawn/construction data
│   ├── EntityTimer.*             # Timed callbacks on entities
│   └── EntityCallbackTimer.*
└── Character.h / Character.cpp    # Character-specific entity subclass
```

**Key Classes:**
- `Entity` (Entity.h:64) - All game objects inherit from Entity
- `Component` (Component.h:20) - Base for all entity components
- `EntityManager` (EntityManager.h:23) - Singleton managing all entities
- `Character` - Player character entity

**Game Loop (dWorldServer/WorldServer.cpp:~1000+):**
1. Receive packets from RakNet
2. Route to appropriate packet handler
3. Call `EntityManager::UpdateEntities(deltaTime)`
4. Update physics world with `dpWorld::StepWorld()`
5. Serialize entities for clients
6. Send outgoing packets

---

### Database Layer: `dDatabase/` (168 files)

Abstraction over MySQL and SQLite backends.

```
dDatabase/
├── GameDatabase/                 # Game world data (accounts, characters, items)
│   ├── Database.h/Database.cpp   # Main database interface
│   ├── GameDatabase.h            # Abstract interface
│   ├── MySQL/                    # MySQL implementation
│   │   ├── MySQLDatabase.cpp
│   │   └── ... (table adapters)
│   ├── SQLite/                   # SQLite implementation
│   │   ├── SQLiteDatabase.cpp
│   │   └── ... (table adapters)
│   ├── ITables/                  # Interface definitions for each table
│   │   ├── IAccountsTable.h
│   │   ├── ICharacterTable.h
│   │   ├── IInventoryTable.h
│   │   ├── IMissionTable.h
│   │   ├── IPropertyTable.h
│   │   └── ... (20+ table interfaces)
│   └── TestSQL/                  # Mock database for testing
├── CDClientDatabase/              # Client data (LOTs, skills, missions, etc)
│   ├── CDClientManager.cpp
│   ├── CDClientDatabase.h/cpp
│   ├── CDClientTables/           # 100+ table loaders
│   │   ├── CDItemTableTable.*    # Item definitions
│   │   ├── CDZoneTableTable.*    # Zone definitions
│   │   ├── CDSkillTable.*        # Skill definitions
│   │   ├── CDBehaviorParameterTable.*
│   │   └── ... (100+ more)
│   └── CDClientCompiledQuery.h
└── ... (connection pooling, transaction management)
```

**Key Classes:**
- `Database` (Database.h:9) - Singleton accessor for game database
- `GameDatabase` (GameDatabase.h) - Abstract interface
- `CDClientDatabase` - Singleton for read-only client data
- `CDClientManager` - Table loader and cache

**Supported Backends:**
- **MySQL** (via MariaDB Connector C++): Production use
- **SQLite**: Development/testing, single-file database

---

### Network Layer: `dNet/` (21 files)

RakNet wrapper and packet serialization definitions.

```
dNet/
├── dServer.h / dServer.cpp       # RakNet peer wrapper
│   ├── RakPeerInterface          # Underlying RakNet (thirdparty)
│   ├── ReplicaManager            # Object synchronization
│   ├── NetworkIDManager          # Network ID assignment
│   └── Methods: Send(), Receive(), Connect(), Disconnect()
├── BitStreamUtils.h/cpp          # RakNet BitStream helpers
├── dNetCommon.h                  # Shared network constants
├── AuthPackets.h/cpp             # Auth server <-> World server packets
├── ChatPackets.h/cpp             # Chat server packets
├── WorldPackets.h/cpp            # Client <-> World server packets
├── MasterPackets.h/cpp           # Master server management packets
├── ClientPackets.h/cpp           # Direct client packet definitions
├── PacketUtils.h/cpp             # Packet parsing utilities
├── MailInfo.h/cpp                # Mail message structure
└── ZoneInstanceManager.h/cpp     # Zone spawning/management
```

**Key Classes:**
- `dServer` (dServer.h:18) - Main network interface for each server
- `Packet` (RakNet) - Network packet from RakNet library

**Network Architecture:**
```
Client <--RakNet--> WorldServer <--RakNet--> MasterServer
                         |
                    ChatServer
                         |
                    AuthServer
```

**RakNet Configuration:**
- **Protocol:** Custom UDP-based (RakNet)
- **Reliable:** True (RakNet provides reliability)
- **Encryption:** Optional (configurable per server)
- **Max MTU:** 1500 bytes (adjustable)

---

### Physics System: `dPhysics/` (14 files)

Custom spatial physics for collision and proximity detection.

```
dPhysics/
├── dpWorld.h / dpWorld.cpp       # Physics world manager (namespace)
│   ├── StepWorld(deltaTime)      # Update all physics
│   ├── AddEntity() / RemoveEntity()
│   └── GetNavMesh()
├── dpEntity.h / dpEntity.cpp     # Physics object representation
│   ├── dpCollisionResult          # Collision query result
│   └── Shape (base for below)
├── dpShapeBase.h / dpShapeBase.cpp  # Abstract physics shape
├── dpShapeBox.h / dpShapeBox.cpp    # AABB box collision
├── dpShapeSphere.h / dpShapeSphere.cpp  # Sphere collision
├── dpCollisionChecks.h/cpp       # Collision test functions
├── dpGrid.h / dpGrid.cpp         # Spatial grid acceleration
└── ... (collision algorithms)
```

**Not a Full Physics Engine:**
- No rigid body dynamics
- No gravity or forces
- Collision only (hit detection)
- Used for: trigger zones, proximity queries, AI collision
- Real player movement is handled by client with server validation

**Integration:**
- Physics objects sync with Entity components (ControllablePhysicsComponent, PhantomPhysicsComponent)
- Updated every frame in game loop

---

### Navigation System: `dNavigation/` (10 files)

AI pathfinding using Recast Navigation (thirdparty).

```
dNavigation/
├── dNavMesh.h / dNavMesh.cpp     # Navmesh manager
│   ├── BuildNavMesh(zoneID)      # From zone files
│   ├── FindPath()                # A* pathfinding query
│   └── GetRandomPoint()
├── dTerrain/                     # Terrain data structures
├── DetourExtensions.h            # Extended Detour query functions
└── ... (Recast navmesh integration)
```

**Third-party:** `thirdparty/recastnavigation/` - Recast Navigation library

**Usage:**
- AI enemy pathfinding
- Player pathfinding queries
- Pre-computed from zone NavMesh files during build

---

### Scripting System: `dScripts/` (613 files)

Lua-based game scripting for behavior and customization.

```
dScripts/
├── CppScripts.h / CppScripts.cpp  # C++ script registry and loader
├── 02_server/                     # Server-side scripts (Lua)
│   ├── DLU/                       # Custom Darkflame Universe scripts
│   ├── Enemy/                     # Enemy behavior scripts
│   ├── Equipment/                 # Item equipment effects
│   ├── Map/                       # Zone-specific logic
│   ├── Minigame/                  # Minigame implementations
│   ├── Objects/                   # Interactive object scripts
│   ├── Pets/                      # Pet behavior
│   └── ... (organized by category)
├── ai/                            # AI behavior scripts (Lua)
│   ├── ACT / AG / FV / GF         # Zone codes
│   ├── GENERAL / PROPERTY         # General/property AI
│   ├── RACING / SPEC / WILD       # Activity-specific
│   ├── MINIGAME / PETS / NS       # More categories
│   └── ... (enemy behavior trees)
├── zone/                          # Zone-specific scripts
│   ├── AG / PROPERTY / LUPs       # Adventure Gate, Property, etc
│   └── ... (zone behavior)
├── client/                        # Client-side script stubs
│   └── ai/
├── EquipmentScripts/              # Equipment effect handlers
├── EquipmentTriggers/             # Equipment trigger handlers
└── ... (various script types)
```

**Script Loading:**
- `ScriptComponent` attaches scripts to entities
- Scripts loaded from `dScripts/` folder at runtime
- Executed via Lua VM integrated with C++ game logic

**Supported Hooks:**
- `onLoad()` - Entity initialization
- `onUpdate(deltaTime)` - Frame update
- `onCollisionPhantom()` - Trigger zone entry/exit
- `onUse()` - Entity interaction
- Custom message handlers

---

### World Server: `dWorldServer/` (3 files)

Main server entry point and zone management.

```
dWorldServer/
├── WorldServer.cpp               # main() entry point
│   ├── Initialize servers (dServer instances)
│   ├── Connect to MasterServer
│   ├── Load zone data
│   ├── Main game loop:
│   │   1. Process incoming packets
│   │   2. Update entities
│   │   3. Update physics
│   │   4. Serialize updates to clients
│   │   5. Send packets
│   └── Graceful shutdown
├── PerformanceManager.h/cpp       # Frame rate and timing stats
└── CMakeLists.txt
```

**Entry Point:** `dWorldServer/WorldServer.cpp:80 - int main(int argc, char** argv)`

**Key Initialization (WorldServer.cpp ~150-250):**
```cpp
1. Load config (worldconfig.ini)
2. Connect to database
3. Initialize CDClient database
4. Create dServer instance with RakNet
5. Connect to MasterServer
6. Load zone data (from CDClient)
7. Initialize EntityManager
8. Initialize dZoneManager
9. Start main loop
```

---

### Chat Server: `dChatServer/` (13 files)

Chat, friends, guild, and social features.

```
dChatServer/
├── ChatServer.cpp                # main() entry point
│   └── Similar startup to WorldServer
├── ChatPacketHandler.cpp         # Route incoming packets
├── PlayerContainer.h/cpp         # Online player tracking
├── TeamContainer.h/cpp           # Team/group management
├── ChatWeb.h/cpp                 # Web API for chat
├── ChatIgnoreList.h/cpp          # Ignore list management
├── ChatJSONUtils.h/cpp           # JSON serialization for chat data
└── CMakeLists.txt
```

**Key Features:**
- Player online/offline tracking
- Friend list management
- Guild management
- Team/group management
- Cross-server communication with WorldServer

---

### Master Server: `dMasterServer/` (5 files)

Instance management, server coordination, authentication.

```
dMasterServer/
├── MasterServer.cpp              # main() entry point
│   ├── World instance spawning
│   ├── Server registration
│   ├── Database migrations
│   └── Admin CLI
├── InstanceManager.h/cpp         # Spawn/manage WorldServer instances
├── Start.h / Start.cpp           # Server startup sequence
└── CMakeLists.txt
```

**Responsibilities:**
1. Accept WorldServer registrations
2. Track online instances
3. Route player login requests to available instances
4. Run database migrations on startup
5. Broadcast shutdown messages
6. Handle admin commands (character bans, etc.)

**Entry Point:** `dMasterServer/MasterServer.cpp:80 - int main(int argc, char** argv)`

---

### Auth Server: `dAuthServer/` (1 file)

Account authentication and login.

```
dAuthServer/
├── AuthServer.cpp                # main() entry point
│   ├── Account creation
│   ├── Login validation
│   ├── Password hashing (bcrypt)
│   └── Session token generation
└── CMakeLists.txt
```

---

### Common Utilities: `dCommon/` (124 files)

Shared code across all servers.

```
dCommon/
├── Game.h / Game.cpp             # Global Game namespace
│   ├── extern Logger* logger
│   ├── extern dServer* server
│   ├── extern dConfig* config
│   └── extern AssetManager* assetManager
├── dConfig.h / dConfig.cpp       # Config file parser (.ini)
├── Logger.h / Logger.cpp         # Logging system
├── Diagnostics.h/cpp             # Performance diagnostics
├── GeneralUtils.h/cpp            # String, math, utility functions
├── BinaryPathFinder.h/cpp        # Find binary directory at runtime
├── BinaryIO.h/cpp                # Binary file I/O
├── dEnums/                       # Game enums (eReplicaComponentType, etc)
│   └── MessageType/              # Message type enums
├── dClient/                      # Client-side protocol structures
├── LDFFormat.h/cpp               # LDF (LEGO Data Format) parsing
├── Amf3.h / AMF*.cpp             # Flash AMF3 serialization
├── MD5 related files             # (via thirdparty)
└── ... (ZString, bitstream, etc)
```

**Key Classes:**
- `dConfig` (dConfig.h) - INI file configuration parser
- `Logger` - Logging to console/files
- Various enum definitions for game types

---

### Zone Management: `dZoneManager/` (11 files)

Zone loading and management.

```
dZoneManager/
├── dZoneManager.h / dZoneManager.cpp
│   ├── LoadZone(zoneID, instanceID)
│   ├── GetZone()
│   └── Spawner management
├── Zone.h / Zone.cpp             # Zone instance representation
│   ├── Entity spawning
│   ├── Spawner list
│   └── Zone-specific state
├── Spawner.h / Spawner.cpp       # Enemy/object spawner
│   ├── Spawn timing
│   ├── Spawn group management
│   └── Respawn logic
└── CMakeLists.txt
```

**Zone Loading:**
1. Load zone definition from CDClient database
2. Spawn initial entities (scenery, NPCs, enemies)
3. Setup spawners for dynamic respawning
4. Start zone controller scripts

---

### Web API: `dWeb/` (2 files)

HTTP and WebSocket API layer (Mongoose library).

```
dWeb/
├── Web.h / Web.cpp               # HTTP/WebSocket server
│   ├── HTTPRoute registration
│   ├── HTTPRoute handling
│   ├── WebSocket events
│   └── JSON request/response
└── CMakeLists.txt
```

**Built-on:** `thirdparty/mongoose/` - Mongoose HTTP library

**Usage:**
- Admin dashboard
- Game APIs
- Status endpoints

---

### Chat Filter: `dChatFilter/` (2 files)

Chat content filtering.

```
dChatFilter/
├── dChatFilter.h / dChatFilter.cpp
│   ├── FilterMessage()
│   ├── IsProfanity()
│   └── Blocklist management
└── CMakeLists.txt
```

---

## Build System

### CMake Structure

**Root:** `/CMakeLists.txt` (354 lines)

```cmake
cmake_minimum_required(VERSION 3.25)
project(Darkflame)

# C++ Standard: C++20
# C Standard: C99

# Third-party libraries (vendored)
add_subdirectory(thirdparty SYSTEM)

# Main libraries
add_subdirectory(dCommon)
add_subdirectory(dDatabase)
add_subdirectory(dNet)
add_subdirectory(dGame)
add_subdirectory(dZoneManager)
add_subdirectory(dNavigation)
add_subdirectory(dPhysics)
add_subdirectory(dServer)
add_subdirectory(dWeb)

# Binary targets
add_subdirectory(dWorldServer)
add_subdirectory(dChatServer)
add_subdirectory(dAuthServer)
add_subdirectory(dMasterServer)

# Tests
add_subdirectory(tests)
```

**Build Presets:** `CMakePresets.json` (639 lines)

Available configurations:
- `linux-gnu-debug`, `linux-gnu-release`, `linux-gnu-relwithdebinfo`
- `linux-clang-debug`, `linux-clang-release` (experimental)
- `windows-msvc` (MSVC toolchain, x64 architecture)
- `macos` (macOS with Xcode)

**Example Build:**
```bash
cmake --preset linux-gnu-release
cmake --build --preset linux-gnu-release
```

### Third-party Dependencies

**Thirdparty:** `/thirdparty/` (11 libraries)

| Library | Purpose | Version | Location |
|---------|---------|---------|----------|
| **raknet** | Networking (UDP, reliability, replica sync) | Custom | `thirdparty/raknet/` |
| **mariadb-connector-cpp** | MySQL client library | Latest | `thirdparty/mariadb-connector-cpp/` |
| **SQLite** | Embedded SQL database | 3.x | `thirdparty/SQLite/` |
| **tinyxml2** | XML parsing | Latest | `thirdparty/tinyxml2/` |
| **nlohmann/json** | JSON serialization | 3.x | `thirdparty/nlohmann/` |
| **mongoose** | HTTP/WebSocket server | Latest | `thirdparty/mongoose/` |
| **recastnavigation** | AI pathfinding navmesh | Latest | `thirdparty/recastnavigation/` |
| **magic_enum** | Enum reflection | Latest | `thirdparty/magic_enum/` |
| **libbcrypt** | Password hashing | Latest | `thirdparty/libbcrypt/` |
| **cpplinq** | LINQ queries in C++ | Latest | `thirdparty/cpplinq/` |
| **MD5** | Hash function | Custom | `thirdparty/MD5/` |

**RakNet:** Heavily modified from official version; includes ReplicaManager and NetworkIDManager for object synchronization.

### Configuration Files

**`resources/`** - Copied to build output on first build:
- `sharedconfig.ini` - Shared server config
- `authconfig.ini` - Auth server config
- `chatconfig.ini` - Chat server config
- `worldconfig.ini` - World server config
- `masterconfig.ini` - Master server config
- `blocklist.dcf` - Chat blocklist

**`CMakeVariables.txt`** - Version information:
```
PROJECT_VERSION_MAJOR=1
PROJECT_VERSION_MINOR=0
PROJECT_VERSION_PATCH=0
```

---

## Database Schema

### GameDatabase Tables (SQLite/MySQL)

See `/migrations/dlu/sqlite/0_initial.sql` for complete schema. Key tables:

| Table | Purpose |
|-------|---------|
| `accounts` | Player accounts with password hash |
| `charinfo` | Character metadata (name, level, etc) |
| `charxml` | Character XML (inventory, mission progress) |
| `friends` | Friendship relationships |
| `mail` | In-game mail messages |
| `leaderboard` | Minigame leaderboards |
| `pet_names` | Pet naming (approval tracking) |
| `properties` | Player plots/properties |
| `property_contents` | Items on properties |
| `inventory` | Player inventory items |
| `missions` | Character mission progress |
| `behaviors` | Saved behaviors (building mode) |

### CDClientDatabase

Read-only tables loaded from LEGO client files. Examples:
- `Objects` (LOT definitions, properties, components)
- `ItemTable` (item definitions)
- `SkillBehavior` (skill definitions)
- `ZoneTable` (zone definitions)
- 100+ more tables

---

## File Statistics

| Directory | .cpp | .h | Total |
|-----------|------|-----|--------|
| dGame | 170 | 171 | 341 |
| dDatabase | 84 | 84 | 168 |
| dCommon | 62 | 62 | 124 |
| dScripts | 306 | 307 | 613 |
| dNet | 10 | 11 | 21 |
| dZoneManager | 6 | 5 | 11 |
| dPhysics | 7 | 7 | 14 |
| dNavigation | 5 | 5 | 10 |
| dChatServer | 7 | 6 | 13 |
| dMasterServer | 3 | 2 | 5 |
| dWorldServer | 2 | 1 | 3 |
| dAuthServer | 1 | 0 | 1 |
| dWeb | 1 | 1 | 2 |
| dChatFilter | 1 | 1 | 2 |
| **TOTAL (excluding thirdparty)** | **667** | **663** | **1330** |

**Including thirdparty:** 1600+ files

---

## Key Data Structures

### LWOOBJID (Unique Entity ID)
- 64-bit integer uniquely identifying every game object
- Format: `LWOOBJID = uint64_t`
- Used throughout for entity references

### LOT (LEGO Object Template)
- 32-bit identifier for object type
- References CDClient Objects table
- Determines which components an entity has

### Component Types (eReplicaComponentType)
Enum defining all component types:
- Physics components (1-10)
- Gameplay components (11-50)
- Social components (51-70)
- etc.

---

## Initialization Flow

### MasterServer Startup
1. Parse `masterconfig.ini`
2. Connect to GameDatabase
3. Load CDClientDatabase
4. Initialize RakNet peer
5. Wait for WorldServer registrations
6. Accept login requests
7. Spawn WorldServer instances as needed

### WorldServer Startup  
1. Parse `worldconfig.ini`
2. Connect to GameDatabase
3. Load CDClientDatabase
4. Initialize RakNet peer (zone-specific port)
5. Connect to MasterServer
6. Load zone definition (from CDClient)
7. Initialize physics world (dpWorld)
8. Initialize entity manager
9. Load/spawn initial entities
10. Enter main game loop

### Game Loop (repeats ~30fps)
1. Receive packets from RakNet
2. Process each packet (user input, etc)
3. Update entities (physics, AI, timers)
4. Check collisions
5. Update navigation
6. Serialize entity state
7. Send packets to clients
8. Check for shutdown signal

---

## Architecture Pattern

**Component-Based Entity System (CBS)**

- Every game object is an `Entity`
- Entities are containers for `Component` instances
- Each component handles one aspect (physics, inventory, AI, etc)
- Data = Component instances
- Logic = Component update methods + Script handlers
- No inheritance hierarchies for game objects

**Advantages:**
- Flexibility (components can be added/removed dynamically)
- Reusability (components used across many entity types)
- Loose coupling (components don't depend on each other)

---

## Technology Summary

| Layer | Technology |
|-------|-----------|
| **Language** | C++20, C99, Lua scripting |
| **Build** | CMake 3.25+ with presets |
| **Networking** | RakNet (UDP, custom protocol) |
| **Database** | MySQL (production), SQLite (development) |
| **Physics** | Custom collision system + Recast navmesh |
| **HTTP API** | Mongoose library |
| **Scripting** | Lua VM integrated with C++ |
| **JSON** | nlohmann/json |
| **XML** | tinyxml2 |
| **Hashing** | bcrypt (passwords), MD5 (other uses) |
| **Serialization** | Flash AMF3, RakNet BitStream, LDF format |

---

## Entry Points by Server

| Server | File | Function |
|--------|------|----------|
| **MasterServer** | `dMasterServer/MasterServer.cpp` | `main()` at line 80 |
| **WorldServer** | `dWorldServer/WorldServer.cpp` | `main()` at line ~150 |
| **ChatServer** | `dChatServer/ChatServer.cpp` | `main()` entry |
| **AuthServer** | `dAuthServer/AuthServer.cpp` | `main()` entry |

---

## Next Steps for Exploration

1. **Game Logic:** Read `Entity.h` and `EntityManager.h`
2. **Components:** Browse `dGame/dComponents/` for specific mechanics
3. **Networking:** Check `dNet/dServer.h` for packet handling
4. **Database:** Review `dDatabase/GameDatabase/Database.h`
5. **Scripting:** See `dScripts/CppScripts.h` for script integration
6. **Physics:** Study `dPhysics/dpWorld.h` for collision system
7. **Navigation:** See `dNavigation/dNavMesh.h` for pathfinding

