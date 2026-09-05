> **RETIRED (2026-04-03 Claude dump).** Not current. Living map: [`00_INDEX.md`](00_INDEX.md) · [`STATUS.md`](STATUS.md).
> File:line citations are frozen at analysis time (committed `aa2de8e8`, 2026-05-19).
>
> **Rot in this file:** Dual MySQL/SQLite player DB + CDClient SQLite catalog + character XML blob are still true. Migration counts in the dump (0–23 / “24 migrations”) are wrong: MySQL is **0–26**, SQLite player **0–9**. ItemSet LIKE is cached process-wide on this tip; the CDClient column is still a text list (no junction table).

# DarkflameServer Data Persistence

## 1. Database Layer

### Database Architecture

DarkflameServer supports **two database backends** via an abstraction layer:

#### MySQL/MariaDB (Production)
- **Location**: `dDatabase/GameDatabase/MySQL/MySQLDatabase.h/cpp`
- **Connection**: Via MySQLConnectorC++
- **Primary Database**: Game state (characters, items, properties, etc.)
- **Schema**: Defined in `migrations/dlu/mysql/` (numbered SQL files)

#### SQLite (Development/Testing)
- **Location**: `dDatabase/GameDatabase/SQLite/SQLiteDatabase.h/cpp`
- **Connection**: Via CppSQLite3
- **Primary Database**: Game state (same schema as MySQL)
- **Schema**: Defined in `migrations/dlu/sqlite/` (numbered SQL files)

### Abstraction Layer
**Location**: `dDatabase/GameDatabase/GameDatabase.h`

Abstract base class `GameDatabase` defines the interface:
```cpp
class GameDatabase {
    virtual void Connect() = 0;
    virtual void Destroy(std::string source = "") = 0;
    virtual void ExecuteCustomQuery(const std::string_view query) = 0;
    virtual void Commit() = 0;
    virtual bool GetAutoCommit() = 0;
    virtual void SetAutoCommit(bool value) = 0;
    virtual void DeleteCharacter(const LWOOBJID characterId) = 0;
    
    // Table interfaces (see below)
    virtual IPlayKeys* ...
    virtual ILeaderboard* ...
    // etc.
};
```

Database selection:
```cpp
Database::Connect();  // In WorldServer.cpp
// Determines SQL type from config and instantiates MySQLDatabase or SQLiteDatabase
```

### Connection Configuration
**Config Files**:
- `resources/sharedconfig.ini` - Common settings
- `resources/worldconfig.ini` - World server settings
- `resources/masterconfig.ini` - Master server settings
- `resources/chatconfig.ini` - Chat server settings
- `resources/authconfig.ini` - Auth server settings

**Key Settings**:
```ini
[database]
db_type=mysql           # 'mysql' or 'sqlite'
mysql_host=localhost
mysql_user=darkflame
mysql_password=password
mysql_database=darkflame
mysql_port=3306
```

### Connection Pooling
- No explicit connection pool in DarkflameServer
- MySQL: Single persistent connection per server instance
- SQLite: Single file-based connection per server instance
- Thread-safe queries via CppSQLite3 (SQLite) or MySQLConnectorC++ (MySQL)

---

## 2. CDClient Database

### Purpose
The CDClient database contains **read-only game data** (items, creatures, missions, behaviors, etc.). This is derived from LEGO Universe's original client-side database.

### Location & Format

**File**: `resServer/CDServer.sqlite` (relative to binary directory)
- **Format**: SQLite3 database
- **Size**: ~50-100MB depending on version
- **Read-Only**: Never written to by server (immutable asset database)

### Loading

**Location**: `dWorldServer/WorldServer.cpp:185-197`
```cpp
CDClientDatabase::Connect(
    (BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string()
);
CDClientManager::LoadValuesFromDatabase();
```

**Initialization**: On World server startup, all CDClient tables are loaded into memory via `CDClientManager::LoadValuesFromDatabase()` for O(1) lookup performance.

### Table Accessor Classes

**Location**: `dDatabase/CDClientDatabase/CDClientTables/`

**Pattern**: Each table has a class inheriting from `CDTable<T>`:
```cpp
template <typename T>
class CDTable {
    std::vector<T> entries;  // Loaded from database
    virtual void LoadTableData() = 0;
    static T::StorageType& Instance() { return s_instance; }
};
```

**Sample CDClient Tables** (86 total):

| Table | Purpose | Key Data |
|-------|---------|----------|
| `CDObjectsTable` | All object templates (LOT -> properties) | LOT, name, collision, model path |
| `CDComponentsRegistryTable` | Component IDs per LOT | LOT -> component IDs |
| `CDInventoryComponentTable` | Item inventory properties | LOT -> inventory size, type |
| `CDItemComponentTable` | Item properties | LOT -> damage, armor, rarity |
| `CDDestructibleComponentTable` | Destructible object properties | LOT -> health, armor, loot |
| `CDMissionsTable` | Mission definitions | Mission ID -> description, rewards |
| `CDMissionTasksTable` | Mission task definitions | Mission ID -> task type, target |
| `CDSkillBehaviorTable` | Skill -> behavior tree mapping | Skill ID -> behavior ID |
| `CDBehaviorParameterTable` | Behavior parameters | Behavior ID -> parameters |
| `CDBehaviorTemplateTable` | Behavior tree structure | Behavior ID -> child behaviors |
| `CDLootMatrixTable` | NPC loot tables | LOT -> loot matrix ID |
| `CDLootTableTable` | Loot drops | Loot table ID -> items, drop %s |
| `CDActivityRewardsTable` | Activity rewards | Activity ID -> reward items |
| `CDZoneTableTable` | Zone metadata | Zone ID -> zone name, template |
| `CDPhysicsComponentTable` | Physics shapes | LOT -> collision radius, shape |
| `CDMovementAIComponentTable` | AI movement properties | LOT -> patrol speed, etc. |
| `CDPetComponentTable` | Pet properties | Pet LOT -> diet, abilities |
| `CDEmoteTable` | Emote definitions | Emote ID -> animation |
| `CDVendorComponentTable` | Vendor shop inventory | LOT -> items for sale |
| `CDPropertyTemplateTable` | Player property templates | Property LOT -> size, cost |
| `CDRarityTableTable` | Item rarity percentages | LOT -> rarity %, modifiers |

### Accessing CDClient Data

**Example**:
```cpp
auto* objectsTable = CDClientManager::GetTable<CDObjectsTable>();
auto* objects = objectsTable->GetAll();  // Get all objects
auto object = objectsTable->GetByID(lotID);  // Get specific LOT

// Use object.componentID, object.displayName, etc.
```

---

## 3. Character Data

### Persistence Pipeline

#### Loading on Login
1. **Auth** verifies credentials
2. **Master** routes to appropriate **World** server
3. **World** calls `UserManager::CreateUser()` which:
   - Loads character from `charinfo` table
   - Loads character XML from `charxml` table
   - Loads character stats into memory

#### Saving (Periodic)
**Location**: `dWorldServer/WorldServer.cpp:490-497` (every 10 minutes)
```cpp
if (framesSinceLastUsersSave >= saveTime && zoneID != 0) {
    UserManager::Instance()->SaveAllActiveCharacters();
    // Each User calls character->Save()
}
```

#### Storage

**Character Info Table** (`charinfo`):
```
id (BIGINT, PRIMARY KEY)      - Character object ID (LWOOBJID)
account_id (INT)              - Account ID reference
name (TEXT UNIQUE)            - Character display name
pending_name (TEXT)           - Name change pending
needs_rename (INT)            - Force rename flag
prop_clone_id (INT UNIQUE)    - Player property clone ID
last_login (BIGINT)           - Last login timestamp
permission_map (BIGINT)       - Bit flags for permissions
```

**Character XML Table** (`charxml`):
```
id (BIGINT, PRIMARY KEY)      - Character ID
xml_data (TEXT)               - Serialized character state (XML)
```

**Character XML Contents**:
The XML contains:
```xml
<c>
  <inv>
    <!-- Inventory items, equipment, vault -->
  </inv>
  <m>
    <!-- Mission states and progress -->
  </m>
  <lvl>
    <!-- Level, experience, skill progression -->
  </lvl>
  <!-- Account flags, zones visited, completed activities, etc. -->
</c>
```

### Character Components Saved
When character saves via `character->Save()`:
1. **MissionComponent**: Mission states serialized to XML
2. **InventoryComponent**: Item list and equipment
3. **LevelProgressionComponent**: Level, XP, skill trees
4. **PropertyComponent**: Player property data

### In-Memory Character Class
**Location**: `dGame/dEntity/Character.h`

```cpp
class Character {
    LWOOBJID m_ID;
    User* m_Parent;
    std::string m_Name;
    Entity* m_Entity;  // Associated entity in world
    
    void Save(bool userInvariant = false);
    void LoadFromXml(tinyxml2::XMLDocument& doc);
    void UpdateXml(tinyxml2::XMLDocument& doc);
};
```

---

## 4. Asset & Resource Files

### Directory Structure
```
res/
├── cdclient.fdb           # CDClient data (read-only)        — SQLite MCP edits the converted CDServer.sqlite
├── animations/            # Animation files                  — authored alongside meshes in Blender (Blender MCP)
├── meshes/                # 3D model meshes (NIF format)     — Blender MCP, see doc 13 §2.2
├── textures/              # Texture files (DDS)              — image-gen + texconv MCP, see doc 13 §2.3
├── effects/               # Particle effects                 — typically reuses existing; otherwise Blender + texconv
├── sounds/                # Audio files (OGG)                — audio-gen MCP, see doc 13 §2.4
├── scripts/               # Lua/script files (for clients)   — Claude authors natively
└── zones/
    ├── zone_0.luz         # Zone 0 (character select)        — NO MCP path; see doc 13 §3.1
    ├── zone_1000.luz      # Avant Gardens zone file          — same
    └── ...
```

### Asset Manager
**Location**: `dCommon/dClient/AssetManager.h/cpp`

Loads assets from either:
1. **Unpacked directory**: Individual files in `res/` folder
2. **Packed format**: LEGO `.pak` package files (if zipped)

```cpp
class AssetManager {
    bool HasFile(std::string name) const;
    bool GetFile(std::string name, char** data, uint32_t* len) const;
    AssetStream GetFile(std::string name) const;  // Stream interface
};
```

### LUZ Zone Files

**Format**: Binary LEGO Universe Zone file (custom format)
**Location**: `dZoneManager/Zone.cpp` (file loading)

**File Header**:
- FileFormatVersion (enum: PreAlpha, Alpha, Beta, EarlyAlpha, etc.)
- MapRevision
- WorldID (must match zone ID)
- Spawnpoint (default player spawn position)
- SceneCount (number of scenes in zone)

**Scenes**: Each zone can have multiple "scenes" (loaded from LVL files)
- Each scene has a Level file (LVL binary format)
- Scene data includes object placement, triggers, paths

**Paths**: Movement paths for NPCs and moving platforms
- PathType: Movement, MovingPlatform, Property, Camera, Spawner, Showcase, Race, Rail
- PathBehavior: Loop, Bounce, Once
- PathWaypoints with position, rotation, speed, configuration

### LVL Level Files

**Format**: Binary LEGO Universe Level file
**Parser**: `dZoneManager/Level.cpp`

**Chunk Structure**:
```
[FileInfoChunk]:
  [Version:4]
  [Revision:4]
  [EnvironmentChunkStart:4]
  [ObjectChunkStart:4]
  [ParticleChunkStart:4]
  
[EnvironmentChunk]:
  // Scene lighting, skybox, weather
  
[ObjectChunk]:
  [ObjectCount:4]
  [Objects...]:
    [LOT:4]
    [Position:Point3]
    [Rotation:Quaternion]
    [Scale:Float]
    [Settings:LDF]  // Key-value pairs
    
[ParticleChunk]:
  // Particle effects placement
```

**Chunk Types**:
```cpp
enum ChunkTypeID {
    FileInfo = 1000,
    SceneEnvironment = 2000,
    SceneObjectData = 2001,
    SceneParticleData = 2002
};
```

**Objects Loaded**:
From LVL file, all objects with their LOT, position, rotation, scale are:
1. Spawned as Entity in EntityManager
2. Components initialized based on CDComponentsRegistry
3. Settings (LDF) applied from zone file or CDClient defaults

### NIF Model Files

**Format**: Netimmerse File (3D model format)
**Location**: `res/meshes/*.nif`
**Parser**: Not fully implemented in DarkflameServer (mostly client-side)

- Contains 3D geometry, textures, animations
- Referenced by `ModelComponent` on entities
- File path comes from CDClient `CDObjectsTable`
- Server uses for collision shapes but not rendering

**Creation path**: With the Blender MCP connected, Claude can drive Blender's Python API to model, rig, and run a NIF exporter add-on directly. Output lands in `res/meshes/`, and the CDClient row referencing the path is set via the SQLite MCP. See `13_CLAUDE_VS_MCP.md` §2.2 (and read the NIF dialect caveat — LU uses an early NetImmerse variant that may require a NifSkope post-step). Companion DDS textures come from the image-gen + texconv MCP (§2.3).

### FDB (CDClient)

**Format**: LEGO Universe client database (SQLite)
**Checksum**: Calculated on startup if `check_fdb=1` in config
```cpp
MD5 md5;
while (!cdclient.eof()) {
    cdclient.read(buffer, 1024);
    md5.update(buffer, bytesRead);
}
g_DatabaseChecksum = md5.hexdigest();
```

- Sent to clients to verify they have matching data
- Prevents desync between server and client LOT data
- Optional: can disable with `check_fdb=0`

---

## 5. Configuration System

### Config Files & Loading

All servers load INI configuration files at startup:

**Location**: `resources/*.ini`

**Master Server** (`masterconfig.ini`):
```ini
max_clients=1000
port=1000
password=ourmaster
verbose_logging=0
```

**World Server** (`worldconfig.ini`):
```ini
source=https://github.com/DarkflameUniverse/DarkflameServer
disable_chat=0
phys_spatial_partitioning=1
phys_sp_tilesize=102
phys_sp_tilecount=24
disable_anti_speedhack=0
pets_take_imagination=1
hardcore_mode=0
solo_racing=0
allow_nameplate_off=0
cdclient_mismatch_title=Version out of date
cdclient_mismatch_message=Please update your client
```

**Chat Server** (`chatconfig.ini`):
```ini
port=1501
max_clients=1000
```

**Auth Server** (`authconfig.ini`):
```ini
port=1001
max_clients=1000
disable_auth=0
```

### Config Loader
**Location**: `dCommon/dConfig.h/cpp`

```cpp
class dConfig {
    dConfig(const std::string& configPath);
    std::string GetValue(const std::string& key) const;
    void LogSettings() const;
};
```

Usage:
```cpp
Game::config = new dConfig("worldconfig.ini");
bool antiSpeedhack = Game::config->GetValue("disable_anti_speedhack") != "1";
uint32_t port = stoi(Game::config->GetValue("port"));
```

### Performance Profiles
**Location**: `dWorldServer/PerformanceManager.h`

Different zones use different performance profiles:
```cpp
PerformanceManager::SelectProfile(zoneID);
uint32_t frameDelta = PerformanceManager::GetServerFrameDelta();  // 16ms or 50ms
```

---

## 6. Migration System

### Purpose
Tracks database schema changes across server versions.

### Migration Files
**Location**: `migrations/dlu/mysql/` and `migrations/dlu/sqlite/`

Numbered files (0-indexed) run in order:
```
0_initial.sql              - Create all tables
1_master_password.sql      - Add password column
2_normalize_model_positions.sql
...
23_store_character_id_as_objectid.sql
```

### Migration Runner
**Location**: `dDatabase/MigrationRunner.cpp`

```cpp
class MigrationRunner {
    void RunMigrations(const std::filesystem::path& migrationsPath, GameDatabase* db);
};
```

On server startup:
1. Check `migration_history` table for completed migrations
2. Run any pending migrations in order
3. Record completion in `migration_history`

### Migration History Table
```
version (INT PRIMARY KEY)   - Migration number
name (TEXT)                 - Migration name
run_on (DATETIME)          - When it was run
```

---

## 7. Game Database Schema (Key Tables)

### Accounts
```sql
CREATE TABLE accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE,
    password TEXT,
    gm_level BIGINT DEFAULT 0,
    locked INTEGER DEFAULT 0,
    banned INTEGER DEFAULT 0,
    play_key_id INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    mute_expire BIGINT DEFAULT 0
);
```

### Character Info
```sql
CREATE TABLE charinfo (
    id BIGINT PRIMARY KEY,                    -- Character object ID
    account_id INTEGER REFERENCES accounts,
    name TEXT UNIQUE,
    pending_name TEXT,
    needs_rename INTEGER DEFAULT 0,
    prop_clone_id INTEGER UNIQUE,
    last_login BIGINT DEFAULT 0,
    permission_map BIGINT DEFAULT 0
);
```

### Character XML (Serialized State)
```sql
CREATE TABLE charxml (
    id BIGINT PRIMARY KEY REFERENCES charinfo,
    xml_data TEXT                             -- Inventory, missions, level, etc.
);
```

### Friends
```sql
CREATE TABLE friends (
    player_id BIGINT REFERENCES charinfo,
    friend_id BIGINT REFERENCES charinfo,
    best_friend INTEGER DEFAULT 0,
    PRIMARY KEY (player_id, friend_id)
);
```

### Properties (Player Housing)
```sql
CREATE TABLE properties (
    id BIGINT PRIMARY KEY,                    -- Property object ID
    owner_id BIGINT REFERENCES charinfo,
    template_id INTEGER,
    clone_id BIGINT REFERENCES charinfo(prop_clone_id),
    name TEXT,
    description TEXT,
    rent_amount INTEGER,
    rent_due BIGINT,
    privacy_option INTEGER,
    mod_approved INTEGER DEFAULT 0,
    last_updated BIGINT
);
```

### Property Contents (Items in Property)
```sql
CREATE TABLE property_contents (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    property_id BIGINT REFERENCES properties,
    item_id BIGINT,
    x FLOAT,
    y FLOAT,
    z FLOAT,
    rx FLOAT,
    ry FLOAT,
    rz FLOAT,
    rw FLOAT,
    model_type INTEGER
);
```

### Mail
```sql
CREATE TABLE mail (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender_id INTEGER DEFAULT 0,
    sender_name TEXT DEFAULT '',
    receiver_id BIGINT REFERENCES charinfo,
    receiver_name TEXT,
    time_sent BIGINT,
    subject TEXT,
    body TEXT,
    attachment_id BIGINT DEFAULT 0,
    attachment_lot INTEGER DEFAULT 0,
    attachment_subkey BIGINT DEFAULT 0,
    attachment_count INTEGER DEFAULT 0,
    was_read INTEGER DEFAULT 0
);
```

### Leaderboards
```sql
CREATE TABLE leaderboard (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id INTEGER DEFAULT 0,
    last_played DATETIME DEFAULT CURRENT_TIMESTAMP,
    character_id BIGINT REFERENCES charinfo,
    primaryScore DOUBLE DEFAULT 0,
    secondaryScore DOUBLE DEFAULT 0,
    tertiaryScore DOUBLE DEFAULT 0,
    numWins INTEGER DEFAULT 0,
    timesPlayed INTEGER DEFAULT 1
);
```

### Behaviors (User-Generated Content)
```sql
CREATE TABLE behaviors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    behavior_id BIGINT,
    character_id BIGINT REFERENCES charinfo,
    behavior_data TEXT
);
```

---

## Summary

DarkflameServer's data persistence uses:
1. **Dual-backend database** (MySQL/MariaDB for production, SQLite for dev)
2. **Abstraction layer** for backend-agnostic code
3. **CDClient SQLite database** for read-only game data (loaded into memory)
4. **Character XML serialization** for player state (missions, inventory, level)
5. **Binary file formats** (LUZ, LVL) for zone data and object placement
6. **INI configuration files** for server settings
7. **Migration system** for schema versioning
8. **Periodic saves** (every 10 minutes) for character state
9. **Lazy loading** of assets via AssetManager with packing support

The system emphasizes **immutable asset data** (CDClient) and **volatile game state** (character XML in game database), enabling scalability and quick iteration on gameplay.

