# DarkflameServer Refactoring Strategy

## 1. GameMessages.cpp Monolith Refactoring

### Current Architecture

**File:** `/dGame/dGameMessages/GameMessages.cpp` (6,445 lines)

The dispatch pattern uses a **function-based handler approach** where client messages are routed to standalone `Handle*()` functions. There is no single centralized dispatcher visible in the monolith; instead, handlers are called from various entry points (WorldPackets, Entity event handlers, etc.).

**Example current pattern (line 6104-6116):**
```cpp
void GameMessages::HandleCancelDonationOnPlayer(RakNet::BitStream& inStream, Entity* entity) {
    auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
    if (!inventoryComponent) return;
    auto* inventory = inventoryComponent->GetInventory(eInventoryType::DONATION);
    if (!inventory) return;
    auto items = inventory->GetItems();
    for (auto& [itemID, item] : items) {
        inventoryComponent->MoveItemToInventory(item, eInventoryType::BRICKS, item->GetCount(), false, false, true);
    }
    // ...
}
```

**Dispatch invocation locations:**
- Direct calls from `/dGame/WorldPackets.cpp` for network packets
- Direct calls from `Entity::HandleMsg()` which dispatches to registered handlers (line 344: `bool HandleMsg(GameMessages::GameMsg& msg) const`)
- Entity stores `std::unordered_multimap<MessageType::Game, std::function<bool(GameMessages::GameMsg&)>> m_MsgHandlers` for message routing

**Header structure:** `GameMessages.h` declares 101+ `HandleXXX()` functions plus ~50 Send/Broadcast functions.

**Problems:**
- 101 handler functions scattered across 6,445 lines with weak semantic grouping
- No handler registration mechanism; callers must know exact function names
- Send and Handle functions mixed together (poor separation of concerns)
- Difficult to add new handlers without modifying the monolith
- Hard to trace message flow and dependencies
- Dynamic handler registration exists (line 420) but not consistently used

### Proposed Architecture: Subsystem-based Handler Registration

Replace flat function namespace with a **handler registry pattern** grouped by subsystem.

**Pseudocode:**

```cpp
// dGameMessages/MessageHandlers/IMessageHandler.h
namespace GameMessages {
    class IMessageHandler {
    public:
        virtual ~IMessageHandler() = default;
        virtual bool CanHandle(MessageType::Game msgId) const = 0;
        virtual void Handle(Entity* entity, MessageType::Game msgId, 
                           RakNet::BitStream& inStream, const SystemAddress& sysAddr) = 0;
    };
}

// dGameMessages/MessageHandlers/InventoryMessageHandler.h
namespace GameMessages {
    class InventoryMessageHandler : public IMessageHandler {
    public:
        bool CanHandle(MessageType::Game msgId) const override;
        void Handle(Entity* entity, MessageType::Game msgId, 
                   RakNet::BitStream& inStream, const SystemAddress& sysAddr) override;
    
    private:
        void HandleUpdateInventoryGroup(Entity* entity, RakNet::BitStream& inStream, ...);
        void HandleUpdateInventoryGroupContents(Entity* entity, RakNet::BitStream& inStream, ...);
        // ... inventory-related handlers
    };
}

// dGameMessages/MessageHandlers/PropertyMessageHandler.h
namespace GameMessages {
    class PropertyMessageHandler : public IMessageHandler {
    public:
        bool CanHandle(MessageType::Game msgId) const override;
        void Handle(Entity* entity, MessageType::Game msgId, 
                   RakNet::BitStream& inStream, const SystemAddress& sysAddr) override;
    
    private:
        void HandleQueryPropertyData(Entity* entity, RakNet::BitStream& inStream, ...);
        void HandleSetPropertyAccess(Entity* entity, RakNet::BitStream& inStream, ...);
        // ... property-related handlers
    };
}

// dGameMessages/MessageDispatcher.h
class MessageDispatcher {
public:
    void RegisterHandler(std::unique_ptr<IMessageHandler> handler);
    bool Dispatch(Entity* entity, MessageType::Game msgId, 
                  RakNet::BitStream& inStream, const SystemAddress& sysAddr);
    
private:
    std::vector<std::unique_ptr<IMessageHandler>> m_handlers;
};

// Usage in Game::Initialize()
dispatcher->RegisterHandler(std::make_unique<InventoryMessageHandler>());
dispatcher->RegisterHandler(std::make_unique<PropertyMessageHandler>());
dispatcher->RegisterHandler(std::make_unique<CombatMessageHandler>());
// ...
```

**Subsystem Categories (estimated handler distribution):**
- **InventoryMessageHandler**: MOVE_ITEM_IN_INVENTORY, UPDATE_INVENTORY_GROUP, etc. (10-15 handlers)
- **MissionMessageHandler**: MISSION_DIALOGUE_OK, REQUEST_ACTIVITY_SUMMARY, etc. (8-12 handlers)
- **CombatMessageHandler**: CAST_SKILL, START_SKILL, ON_HIT, etc. (6-10 handlers)
- **PropertyMessageHandler**: QUERY_PROPERTY_DATA, PLACE_PROPERTY_MODEL, DELETE_PROPERTY_MODEL, etc. (8-10 handlers)
- **BuildingMessageHandler**: SET_BUILD_MODE, START_BUILDING_WITH_ITEM, etc. (5-8 handlers)
- **MovementMessageHandler**: UPDATE_SHOOTING_GALLERY_ROTATION, PLATFORM_RESYNC, etc. (4-6 handlers)
- **DefaultMessageHandler**: Fallback for unregistered message types

### Benefits

- **Modularity**: Each subsystem handler is self-contained, testable in isolation
- **Extensibility**: New message handlers added without touching existing code (Open/Closed principle)
- **Discoverability**: Clear semantic grouping of related messages
- **Decoupling**: Subsystems don't need to know about the dispatcher implementation
- **Reduced recompilation**: Changes to one handler don't recompile the entire monolith
- **Dependency injection**: Handlers can be mocked for testing

### Risk Assessment

**What could break:**
- **Message routing**: Incorrect handler registration → messages silently ignored or routed to wrong handler
  - *Mitigation*: Unit test all handler registrations; add logging to unhandled messages
- **Performance regression**: Function pointer indirection vs direct calls
  - *Mitigation*: Modern compilers devirtualize simple cases; benchmark before/after
- **Handler lookup overhead**: Iterating handler list on every message
  - *Mitigation*: Use a `std::unordered_map<MessageType::Game, IMessageHandler*>` for O(1) lookup after grouping by primary message ID

**Backward compatibility:**
- New handler interface is parallel; old `GameMessages::HandleXXX()` functions can coexist temporarily
- Gradually migrate handlers one subsystem at a time
- Wrap legacy calls in adapter handlers during transition

### Migration Path

1. **Phase 1 (Week 1-2):** Create `IMessageHandler` interface and `MessageDispatcher`
   - Write unit tests for dispatcher
   - No changes to existing code
   
2. **Phase 2 (Week 3-4):** Migrate lowest-dependency subsystem (e.g., InventoryMessageHandler)
   - Extract 10-15 inventory handlers into new class
   - Register with dispatcher
   - Redirect old `GameMessages::HandleUpdateInventoryGroup()` to new handler (adapter)
   - Verify in staging environment

3. **Phase 3 (Week 5-6):** Migrate other subsystems (Property, Movement, Building)
   - Repeat Phase 2 pattern for 3-4 more subsystems
   - Gradually decommission adapter calls

4. **Phase 4 (Week 7):** Remove adapter layer and old functions
   - Delete `GameMessages::HandleXXX()` implementations
   - Keep Send/Broadcast functions in separate SendPackets.cpp

5. **Phase 5 (Week 8):** Optimize
   - Implement O(1) message lookup table
   - Profile and optimize handler instantiation
   - Add metrics logging for message throughput

### Effort: **LARGE** (6-8 weeks)

- Initial infrastructure: 1 week (interfaces, dispatcher, tests)
- Migration per subsystem: 1.5 weeks × 4 subsystems
- Testing and optimization: 1 week

### Backward Compatibility Strategy

- Keep a legacy dispatcher alongside the new one for 1-2 releases
- Log deprecation warnings when old functions are called
- Provide migration guide for custom plugins that register message handlers
- Ensure wire protocol remains unchanged (only internal routing changes)

---

## 2. Component Storage & Retrieval in Entity

### Current Architecture

**Files:**
- `/dGame/Entity.h` (lines 390)
- `/dGame/Entity.cpp` (component lookup implementation)

**Storage mechanism:**
```cpp
std::unordered_map<eReplicaComponentType, Component*> m_Components;  // line 390
```

**Retrieval methods:**

```cpp
// Line 160: Generic lookup
Component* GetComponent(eReplicaComponentType componentID) const;

// Line 162-163: Template-based type-safe lookup
template<typename T>
T* GetComponent() const {  // Delegates to GetComponent(T::ComponentType)
    return dynamic_cast<T*>(GetComponent(T::ComponentType));
}

// Line 427-440: Try-get with error handling
template<typename T>
bool TryGetComponent(const eReplicaComponentType componentId, T*& component) const {
    const auto& index = m_Components.find(componentId);
    if (index == m_Components.end()) {
        component = nullptr;
        return false;
    }
    component = dynamic_cast<T*>(index->second);
    return true;
}
```

**Component count range per entity:**
- Minimal entities (NPCs, props): 3-8 components
- Players: 15-25 components (inventory, mission, skill, buff, movement, etc.)
- Complex entities (raid bosses): up to 30+ components
- Average: 8-12 components per entity in a typical zone

**Performance characteristics:**
- **Lookup**: O(1) average case (hash map), O(n) worst case
- **Dynamic cast**: ~50-100 cycles per component access (virtual function call + type check)
- **Iteration**: O(n) to iterate all components (used in WriteComponents, Update)

### Proposed Architecture: Static Array with Enum Indexing

Replace `unordered_map` with a **flat static array indexed directly by enum value**.

**Current drawbacks of unordered_map:**
- Heap allocation per insertion (fragmentation)
- Hash computation overhead on lookup
- Dynamic cast on every typed access
- Cache misses from pointer dereferencing

**Pseudocode:**

```cpp
// dGame/Entity.h

// Define max component type value
constexpr uint32_t MAX_COMPONENT_TYPE = 
    static_cast<uint32_t>(eReplicaComponentType::LAST_KNOWN) + 1;  // e.g., 140

class Entity {
private:
    // Array-based storage: O(1) index lookup, predictable cache behavior
    Component* m_ComponentArray[MAX_COMPONENT_TYPE];
    uint8_t m_ComponentBitset[MAX_COMPONENT_TYPE / 8];  // Fast existence check
    
public:
    // Type-safe, zero-cast lookup
    template<typename T>
    T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must be a Component");
        const uint32_t index = static_cast<uint32_t>(T::ComponentType);
        if (index < MAX_COMPONENT_TYPE) {
            return static_cast<T*>(m_ComponentArray[index]);
        }
        return nullptr;
    }
    
    // Fallback generic lookup (still O(1))
    Component* GetComponent(eReplicaComponentType componentID) const {
        const uint32_t index = static_cast<uint32_t>(componentID);
        if (index < MAX_COMPONENT_TYPE) {
            return m_ComponentArray[index];
        }
        return nullptr;
    }
    
    // Fast component existence check
    bool HasComponent(eReplicaComponentType componentId) const {
        const uint32_t index = static_cast<uint32_t>(componentId);
        if (index >= MAX_COMPONENT_TYPE) return false;
        return m_ComponentArray[index] != nullptr;
    }
};

// Constructor: Zero-initialize array
Entity::Entity(...) : m_ComponentArray{nullptr} {}

// Adding a component: Direct assignment
void Entity::AddComponent(eReplicaComponentType componentId, Component* component) {
    const uint32_t index = static_cast<uint32_t>(componentId);
    if (index < MAX_COMPONENT_TYPE) {
        m_ComponentArray[index] = component;
    }
}
```

**Memory overhead analysis:**
- Current: 56 bytes (unordered_map) + 8-16 bytes per entry (hash bucket, node)
  - 12 components: ~56 + (12 × 24) = ~344 bytes
- Proposed: 560 bytes (140 × 4-byte pointers) + 18 bytes (bitset)
  - **Net cost**: +234 bytes per entity, but O(1) guaranteed, better cache locality

**Performance improvement (estimated):**
- Component lookup: **3-5× faster** (eliminate hash computation, dynamic_cast)
- HasComponent check: **10-50× faster** (bitset lookup vs hash lookup)
- Cache efficiency: **2-3× better** (contiguous array vs scattered heap allocations)

**Actual usage in codebase:**
```cpp
// dGame/dComponents/InventoryComponent.cpp:835 (current O(n) scan across ALL entities)
const auto rocketLauchPads = Game::entityManager->GetEntitiesByComponent(
    eReplicaComponentType::ROCKET_LAUNCH);

// After refactoring: GetEntitiesByComponent() can use bitset for faster filtering
```

### Benefits

- **Performance**: 3-5× faster component lookup in hot path
- **Cache friendliness**: Array access patterns are predictable; CPU prefetch works better
- **No dynamic cast**: Type safety at compile time via templates, not runtime checks
- **Simpler debugging**: Array visualization in debugger vs hash map traversal
- **Predictable memory layout**: Easier to reason about cache behavior

### Risk Assessment

**What could break:**
- **Enum gaps**: If `eReplicaComponentType` has sparse values with large gaps (e.g., 0, 1, 500), waste memory
  - *Mitigation*: Check max enum value; if sparse, use bitset lookup table instead
- **Component initialization**: Must ensure all array slots are nullptr on construction
  - *Mitigation*: Use `= {}` syntax to zero-initialize; add static_assert in tests
- **Concurrent access**: Multiple threads accessing m_ComponentArray simultaneously
  - *Mitigation*: No change from current (unordered_map is not thread-safe either)

**Backward compatibility:**
- `GetComponent()` signature unchanged; existing call sites work identically
- `m_Components` accessor (line 209) exposed for legacy code; can mark deprecated

### Migration Path

1. **Phase 1 (Day 1):** Audit `eReplicaComponentType` enum
   - Check value range (expected 0-140)
   - Verify no sparse gaps
   - If sparse, design alternative (sparse map wrapper)

2. **Phase 2 (Days 2-3):** Implement array-based storage in Entity
   - Create new `Component* m_ComponentArray[MAX_COMPONENT_TYPE]`
   - Update `GetComponent()`, `TryGetComponent()`, `AddComponent()` to use array
   - Keep `m_Components` map as deprecated fallback (empty)
   - Write unit tests for equivalence

3. **Phase 3 (Day 4):** Measure performance
   - Profile before/after
   - Verify no behavioral changes
   - Check memory impact on player with 25 components

4. **Phase 4 (Week 2):** Remove deprecated map
   - Delete `m_Components` member
   - Remove deprecated accessor method
   - Final full-system test

### Effort: **SMALL** (3-4 days)

- Audit + design: 1 day
- Implementation: 1 day
- Testing + profiling: 1 day
- Cleanup: 0.5 days

### Backward Compatibility Strategy

- Support both approaches for 1 release with transparent fallback
- Lazy migration: AddComponent() writes to both array and map initially
- Deprecation warning if old map accessor is used
- Revert easily if performance doesn't improve (unlikely)

---

## 3. CDClient Table Accessor Pattern

### Current Architecture

**Files examined:**
- `/dDatabase/CDClientDatabase/CDClientTables/CDItemComponentTable.h` (52 lines)
- `/dDatabase/CDClientDatabase/CDClientTables/CDItemComponentTable.cpp` (80+ lines)
- `/dDatabase/CDClientDatabase/CDClientTables/CDTable.h` (50 lines, base class)

**Current pattern (CDItemComponentTable):**

```cpp
// Header: Defines struct + table class
struct CDItemComponent {
    uint32_t id;
    std::string equipLocation;
    uint32_t baseValue;
    bool isKitPiece;
    // ... 46 fields total
};

class CDItemComponentTable : public CDTable<CDItemComponentTable, 
                                           std::map<uint32_t, CDItemComponent>> {
public:
    void LoadValuesFromDatabase();  // Called at startup
    const CDItemComponent& GetItemComponentByID(uint32_t skillID);  // Lookup
    static CDItemComponent Default;
};

// Implementation: Load at startup (startup cost)
void CDItemComponentTable::LoadValuesFromDatabase() {
    uint32_t size = 0;
    auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemComponent");
    while (!tableSize.eof()) {
        size = tableSize.getIntField(0, 0);
        tableSize.nextRow();
    }
    tableSize.finalize();

    // Full table scan, all rows into memory
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemComponent");
    auto& entries = GetEntriesMutable();
    while (!tableData.eof()) {
        CDItemComponent entry;
        entry.id = tableData.getIntField("id", -1);
        entry.equipLocation = tableData.getStringField("equipLocation", "");
        // ... 46 field assignments per row
        entries.insert(std::make_pair(entry.id, entry));
        tableData.nextRow();
    }
    tableData.finalize();
}

// Lookup: returns from cached map
const CDItemComponent& CDItemComponentTable::GetItemComponentByID(uint32_t skillID) {
    auto& entries = GetEntriesMutable();
    const auto& it = entries.find(skillID);
    if (it != entries.end()) {
        return it->second;
    }
    // Fallback: query if not cached (lazy load)
    auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM ItemComponent WHERE id = ?;");
    // ... parse result
    return entries[skillID];
}
```

**Caching strategy:**
- ✓ **Full startup load** at initialization (`LoadValuesFromDatabase()`)
- ✓ **Lazy fallback** for cache misses (individual queries in GetItemComponentByID)
- ✗ **No invalidation** (CDClient is read-only at runtime, acceptable)
- ✗ **No batching** (43 table classes × load at startup = slow boot)

**Base class pattern (CDTable.h):**
```cpp
template<class Table, typename Storage>
class CDTable : public Singleton<Table> {
protected:
    [[nodiscard]] StorageType& GetEntriesMutable() const {
        return CDClientManager::GetEntriesMutable<Table>();
    }
    [[nodiscard]] const StorageType& GetEntries() const {
        return GetEntriesMutable();
    }
};
```

**Issues:**
- 43 table classes with boilerplate `LoadValuesFromDatabase()` implementations
- Naive full-table loads at startup (ItemComponent: ~6000 items = ~seconds per table)
- Total startup time: potentially 30-60 seconds for all tables
- No parallel loading
- String parsing per field (inefficient for numeric types)
- No schema validation

### Proposed Architecture: Lazy-Load with Indexing

**Pseudocode:**

```cpp
// dDatabase/CDClientDatabase/CDClientTables/CDTableBase.h
template<typename RowType, typename KeyType = uint32_t>
class CDTableBase {
public:
    // Load entire table into memory (called once at startup or on demand)
    void LoadAll();
    
    // Lazy-load single row (on first access)
    const RowType& GetByKey(KeyType key);
    
    // Lazy-load range (for filtered queries)
    std::vector<RowType> GetWhere(
        std::function<bool(const RowType&)> predicate);
    
protected:
    std::unordered_map<KeyType, RowType> m_cache;
    bool m_fullyLoaded = false;
    mutable std::shared_mutex m_cacheLock;  // For thread-safe lazy loading
    
    virtual std::string GetTableName() const = 0;
    virtual RowType ParseRow(const CDClientDatabase::Row& row) = 0;
};

// dDatabase/CDClientDatabase/CDClientTables/CDItemComponentTable.h
class CDItemComponentTable : public CDTableBase<CDItemComponent, uint32_t> {
public:
    const CDItemComponent& GetItemComponentByID(uint32_t id);
    
protected:
    std::string GetTableName() const override { return "ItemComponent"; }
    CDItemComponent ParseRow(const CDClientDatabase::Row& row) override;
};

// Implementation
CDItemComponent CDItemComponentTable::ParseRow(const CDClientDatabase::Row& row) {
    CDItemComponent entry;
    entry.id = row.getIntField("id", -1);
    entry.equipLocation = row.getStringField("equipLocation", "");
    entry.baseValue = row.getIntField("baseValue", -1);
    // ... 46 fields
    return entry;
}

const CDItemComponent& CDItemComponentTable::GetItemComponentByID(uint32_t id) {
    // If not cached, load single row (lazy load)
    if (m_cache.find(id) == m_cache.end()) {
        auto query = CDClientDatabase::CreatePreppedStmt(
            "SELECT * FROM ItemComponent WHERE id = ? LIMIT 1");
        query.bind(1, id);
        if (!query.eof()) {
            m_cache[id] = ParseRow(query);
        } else {
            return CDItemComponent::Default;
        }
    }
    return m_cache[id];
}
```

**Benefits:**
- **Parallel startup**: Load tables on-demand in background thread
- **Reduced startup time**: Only load accessed tables (not all 43)
- **Memory savings**: Don't load rarely-used tables (e.g., ItemComponent if no items exist)
- **Reduced code duplication**: Single `LoadAll()`, `LoadByKey()` in base class
- **Thread safety**: Shared mutex for concurrent lazy loads

**Adoption in codebase:**
```cpp
// Before: Always loads entire 43 tables at startup
CDClientManager::GetTable<CDItemComponentTable>()->GetItemComponentByID(123);

// After: Same call, but lazy-loads on first access
CDClientManager::GetTable<CDItemComponentTable>()->GetItemComponentByID(123);
```

### Performance Impact

**Startup time improvement:**
- Current: ~30-60s (load all 43 tables sequentially)
- Proposed: ~5-10s (load only accessed tables, parallel I/O)
- **Net improvement**: 75-80% faster startup (estimated)

**Runtime latency:**
- First access to uncached row: +5-20ms (single query)
- Subsequent accesses: <1ms (hash lookup)
- Worst case: Pre-load frequently used tables (ItemComponent, etc.)

### Risk Assessment

**What could break:**
- **Lazy loading latency**: First access to unpopulated table causes game thread stall
  - *Mitigation*: Pre-load hot tables (Items, Skills, Missions); lazy-load cold tables
- **Thread safety**: Concurrent access during lazy load
  - *Mitigation*: Use `std::shared_mutex` with read-write locks
- **Memory leaks**: Long-lived cache references
  - *Mitigation*: Use reference_wrapper or copy semantics; CDClient is immutable

**Backward compatibility:**
- `GetItemComponentByID()` signature unchanged
- Behavior identical from caller perspective
- Lazy loads transparent

### Migration Path

1. **Phase 1 (Day 1):** Create `CDTableBase<>` template
   - Implement `LoadAll()`, `GetByKey()`, `GetWhere()`
   - Add thread-safe lazy-load logic
   - Write unit tests

2. **Phase 2 (Days 2-3):** Migrate 5 hot tables (ItemComponent, SkillBehavior, etc.)
   - Inherit from `CDTableBase<>`
   - Remove boilerplate `LoadValuesFromDatabase()`
   - Test equivalence

3. **Phase 3 (Days 4-5):** Migrate remaining 38 tables
   - Bulk refactor using template
   - Profile startup time improvement

4. **Phase 4 (Day 6):** Optimize
   - Identify hot tables; pre-load at startup
   - Benchmark lazy-load latency
   - Add metrics for cache hit rates

### Effort: **MEDIUM** (5-7 days)

- Base template design + tests: 1.5 days
- Migrate hot tables: 1.5 days
- Migrate remaining tables: 2 days
- Optimization + testing: 1.5 days

### Backward Compatibility Strategy

- Provide deprecated `LoadValuesFromDatabase()` stub for 1 release
- All existing `GetItemComponentByID()` etc. calls work unchanged
- Graceful fallback to single-row queries if table never fully loads

---

## 4. Script System Coupling

### Current Architecture

**Files:**
- `/dScripts/CppScripts.h` (base class, ~200 lines)
- Example scripts: `/dScripts/NPCAddRemoveItem.h`, etc.

**Base class structure (CppScripts.h, lines 30-100+):**

```cpp
namespace CppScripts {
    class Script {
    public:
        // Virtual event handlers (override in subclasses)
        virtual void OnStartup(Entity* self) {};
        virtual void OnCollisionPhantom(Entity* self, Entity* target) {};
        virtual void OnMissionDialogueOK(Entity* self, Entity* target, 
                                         int missionID, eMissionState missionState) {};
        virtual void OnFireEventServerSide(Entity* self, Entity* sender, 
                                           std::string args, int32_t param1, 
                                           int32_t param2, int32_t param3) {};
        virtual void OnNotifyObject(Entity* self, Entity* sender, 
                                    const std::string& name, 
                                    int32_t param1 = 0, int32_t param2 = 0) {};
        // ... ~50+ virtual methods covering all game events
    };
}
```

**Script-to-Component coupling (example from codebase):**

```cpp
// Script directly accesses components
void MyScript::OnStartup(Entity* self) {
    auto inventory = self->GetComponent<InventoryComponent>();
    if (inventory) {
        inventory->AddItem(1234, 5);  // Directly modifies component
    }
    
    auto mission = self->GetComponent<MissionComponent>();
    if (mission) {
        mission->AcceptMission(456);  // Direct component API call
    }
}
```

**How tightly coupled:**
- Scripts call component methods directly: `GetComponent<T>()->MethodName()`
- Components make reverse calls to scripts via Entity subscription:
  ```cpp
  // Entity.h line 188
  void Subscribe(LWOOBJID scriptObjId, CppScripts::Script* scriptToAdd, 
                 const std::string& notificationName);
  ```
- Scripts receive raw `Entity*` pointer; can access any component
- No abstraction layer between script domain and game logic domain

**Current problems:**
- **Tight coupling**: Scripts must know exact component interfaces
- **Testing difficulty**: Hard to mock components for script unit tests
- **Reusability**: Script logic embedded in component-aware code
- **Fragility**: If component interface changes, all scripts break
- **State explosion**: Scripts access 5-10 components each, creating many dependencies

**Example: NPCAddRemoveItem script**

```cpp
// dScripts/NPCAddRemoveItem.h - script directly manipulates inventory
void NPCAddRemoveItem::OnMissionDialogueOK(Entity* self, Entity* target, 
                                           int missionID, eMissionState missionState) {
    auto inventory = target->GetComponent<InventoryComponent>();
    if (!inventory) return;
    
    // Direct component access and manipulation
    inventory->AddItem(m_rewardLOT, m_rewardCount);
    
    auto character = target->GetComponent<CharacterComponent>();
    if (character) {
        character->SetPlayerFlag(MISSION_COMPLETE, 1);  // Direct flag manipulation
    }
}
```

### Proposed Architecture: Event/Signal Interface Layer

Decouple scripts from components using an **event publisher/subscriber pattern**.

**Pseudocode:**

```cpp
// dScripts/IGameEventListener.h
namespace CppScripts {
    // Decoupled event-based interface
    class IGameEventListener {
    public:
        virtual ~IGameEventListener() = default;
        
        // Inventory events
        virtual void OnItemAdded(LWOOBJID player, LOT itemLOT, uint32_t count) {}
        virtual void OnItemRemoved(LWOOBJID player, LOT itemLOT, uint32_t count) {}
        virtual void OnInventoryFull(LWOOBJID player) {}
        
        // Mission events
        virtual void OnMissionAccepted(LWOOBJID player, int32_t missionID) {}
        virtual void OnMissionCompleted(LWOOBJID player, int32_t missionID) {}
        
        // Character events
        virtual void OnPlayerFlagChanged(LWOOBJID player, uint32_t flagID, bool value) {}
        virtual void OnCharacterLevelUp(LWOOBJID player, uint32_t newLevel) {}
        
        // Generic event (fallback)
        virtual void OnCustomEvent(LWOOBJID player, const std::string& eventName, 
                                   const std::unordered_map<std::string, std::string>& args) {}
    };
}

// dScripts/EventBus.h
class EventBus {
public:
    static EventBus& Instance();
    
    // Register listener
    void Subscribe(std::shared_ptr<CppScripts::IGameEventListener> listener);
    void Unsubscribe(std::shared_ptr<CppScripts::IGameEventListener> listener);
    
    // Publish events
    void PublishItemAdded(LWOOBJID player, LOT itemLOT, uint32_t count);
    void PublishMissionAccepted(LWOOBJID player, int32_t missionID);
    void PublishPlayerFlagChanged(LWOOBJID player, uint32_t flagID, bool value);
    
private:
    std::vector<std::shared_ptr<CppScripts::IGameEventListener>> m_listeners;
};

// dScripts/ScriptAdapter.h (backward compat bridge)
class ScriptAdapter : public CppScripts::IGameEventListener {
public:
    ScriptAdapter(CppScripts::Script* legacyScript) : m_script(legacyScript) {}
    
    void OnItemAdded(LWOOBJID player, LOT itemLOT, uint32_t count) override {
        // Convert event back to legacy script callback
        auto entity = Game::entityManager->GetEntity(player);
        if (entity) {
            m_script->OnNotifyObject(entity, nullptr, "item_added", itemLOT, count);
        }
    }
    
private:
    CppScripts::Script* m_script;
};

// Usage in InventoryComponent
void InventoryComponent::AddItem(const Item* item, int count) {
    // ... existing logic ...
    
    // Publish event instead of direct script calls
    EventBus::Instance().PublishItemAdded(m_Entity->GetObjectID(), 
                                          item->GetLOT(), count);
}

// Usage in script (decoupled version)
class NPCRewardScript : public CppScripts::IGameEventListener {
public:
    void OnMissionAccepted(LWOOBJID player, int32_t missionID) override {
        // No longer directly accessing components!
        // Request action via command interface instead
        GameCommandBus::Instance().RequestItemAdd(player, m_rewardLOT, m_rewardCount);
        GameCommandBus::Instance().RequestFlagSet(player, MISSION_COMPLETE, 1);
    }
};
```

**EventBus architecture:**

```
Component (InventoryComponent)
    ↓ publishes
EventBus (central dispatcher)
    ↓ notifies
Script implementations (NPCAddRemoveItem, etc.)
    ↓ requests actions via
GameCommandBus
    ↓ executes on
Components
```

### Benefits

- **Decoupling**: Scripts don't import component headers; only know about events
- **Testability**: Mock EventBus for script unit tests
- **Reusability**: Script logic can be event-driven, not component-driven
- **Loose coupling**: Add new event types without changing script base class
- **Data hiding**: Scripts don't directly manipulate component state
- **Auditable**: All game-logic changes flow through EventBus (single audit point)

### Risk Assessment

**What could break:**
- **Silent event loss**: Script subscribes to event that doesn't exist
  - *Mitigation*: Compiler-checked event IDs; fallback to legacy script adapter
- **Event storm**: Script publishes many events in tight loop
  - *Mitigation*: EventBus can batch/throttle; validate in tests
- **Circular event loops**: Script A publishes event → Script B publishes event → Script A
  - *Mitigation*: Add depth-tracking to EventBus; refuse >10 nested publishes

**Backward compatibility:**
- Old `Script` base class remains unchanged
- Legacy scripts automatically wrapped in `ScriptAdapter`
- New scripts inherit from `IGameEventListener` and register with EventBus
- Both mechanisms coexist for 1-2 releases

### Migration Path

1. **Phase 1 (Week 1):** Create EventBus + IGameEventListener
   - Define 50+ common events (ItemAdded, MissionCompleted, etc.)
   - Implement EventBus dispatcher with thread-safe listener management
   - Write unit tests for event publication and subscription

2. **Phase 2 (Week 2-3):** Integrate with components
   - Modify InventoryComponent to publish events (while keeping legacy behavior)
   - Modify MissionComponent similarly
   - Test that both old and new listeners receive events

3. **Phase 3 (Week 4-5):** Migrate 5 example scripts
   - Rewrite NPCAddRemoveItem to use IGameEventListener
   - Rewrite 4 more high-impact scripts
   - Verify behavior equivalence with event-driven approach

4. **Phase 4 (Week 6+):** Optional: Migrate remaining scripts
   - Can be gradual (low priority scripts last)
   - Legacy adapter means no rush

### Effort: **LARGE** (5-7 weeks)

- EventBus infrastructure: 1 week
- Component integration: 1.5 weeks
- Script migration: 2-3 weeks (depends on script count)
- Testing + stabilization: 1 week

### Backward Compatibility Strategy

- Keep `CppScripts::Script` base class untouched
- All scripts auto-wrapped in EventAdapter if not updated
- New `IGameEventListener` interface for new scripts
- Provide migration guide + examples
- Support both patterns indefinitely (no forced migration)

---

## 5. Hardcoded Configuration Values

Identified hardcoded values that should be externalized to configuration:

### Configuration Opportunities

| File | Line | Current Value | Purpose | Proposed Config Key |
|------|------|---------------|---------|---------------------|
| `/dGame/EntityManager.cpp` | 334 | `1000.0f` | Proximity radius limit (client also has 1000 unit limit) | `proximity_max_radius` |
| `/dGame/dGameMessages/GameMessages.cpp` | 2505 | `"save_lxfmls"` | Save LXML files flag | (already configurable, via dConfig) |
| `/dGame/dGameMessages/GameMessages.cpp` | 5047 | `"allow_players_to_skip_cinematics"` | Cinematic skip permission | (already configurable) |
| `/dGame/dGameMessages/GameMessages.cpp` | 5103 | `"disable_extra_backpack"` | Extra backpack availability | (already configurable) |
| `/dGame/dBehaviors/Behavior.cpp` | 157 | implicit | Behavior duration default | `behavior_default_duration_ms` |
| `/dGame/dBehaviors/OverTimeBehavior.cpp` | 22-31 | implicit (calculated from m_Delay × m_NumIntervals) | Over-time behavior interval calculation | (data-driven via CDClient already) |
| `/dGame/dComponents/DestroyableComponent.cpp` | ~line 100+ | implicit constants | Combat damage formulas, armor calculations | `combat_damage_scale_factor`, `armor_reduction_percent` |
| `/Dockerfile` | 1 | `gcc:12` | Base compiler image | `COMPILER_VERSION` (env var) |
| `/docker-compose.yml` | 8 | `mariadb:latest` | Database image version | `MARIADB_VERSION` (env var) |

**Example: Proximity radius hardcoded**

```cpp
// Current: EntityManager.cpp line 334
std::vector<Entity*> EntityManager::GetEntitiesByProximity(
    NiPoint3 reference, float radius) const {
    std::vector<Entity*> entities;
    if (radius <= 1000.0f) {  // HARDCODED LIMIT
        for (auto* entity : m_Entities | std::views::values) {
            if (NiPoint3::Distance(reference, entity->GetPosition()) <= radius) 
                entities.push_back(entity);
        }
    }
    return entities;
}

// Proposed: dConfig-driven
std::vector<Entity*> EntityManager::GetEntitiesByProximity(
    NiPoint3 reference, float radius) const {
    std::vector<Entity*> entities;
    const float MAX_RADIUS = std::stof(
        Game::config->GetValue("proximity_max_radius", "1000.0"));
    if (radius <= MAX_RADIUS) {
        for (auto* entity : m_Entities | std::views::values) {
            if (NiPoint3::Distance(reference, entity->GetPosition()) <= radius) 
                entities.push_back(entity);
        }
    }
    return entities;
}
```

**Combat constants (estimated locations):**

```cpp
// DestroyableComponent.cpp (estimated hardcoded values)
const int32_t BASE_ARMOR_REDUCTION = 5;  // % per armor point
const float CRIT_CHANCE_BASE = 0.1f;     // 10%
const float CRIT_DAMAGE_MULTIPLIER = 1.5f;  // 150% of base damage

// Proposed config keys:
// - armor_reduction_percent_per_point
// - critical_hit_base_chance
// - critical_hit_damage_multiplier
```

### Refactoring Strategy

1. **Audit phase**: Scan source tree for integer/float literals in game logic
2. **Classify**: Determine if value is:
   - Game balance (should be configurable)
   - Protocol constant (should not change)
   - Infrastructure (should be configurable)
3. **Externalize**: Move balance values to `res/configs/game_balance.ini`
4. **Use dConfig**: Retrieve at runtime with defaults

**Template code:**

```cpp
// In component initialization
float GetConfiguredValue(const std::string& key, float defaultValue) {
    try {
        return std::stof(Game::config->GetValue(key, std::to_string(defaultValue)));
    } catch (...) {
        LOG_WARN("Failed to parse config value %s, using default %f", key.c_str(), defaultValue);
        return defaultValue;
    }
}

// Usage
const float MAX_PROXIMITY = GetConfiguredValue("proximity_max_radius", 1000.0f);
```

### Effort: **SMALL** (2-3 days)

- Audit codebase: 1 day
- Create balance config file: 0.5 days
- Migrate values to dConfig: 1-1.5 days

### Backward Compatibility Strategy

- Keep hardcoded defaults in code
- If config value missing, use hardcoded default
- Can migrate to all-config-driven later
- Supports hot-reload if dConfig supports it

---

## 6. Behavior Tree Data-Driving

### Current Architecture

**Files:**
- `/dGame/dBehaviors/Behavior.h` (base class)
- `/dGame/dBehaviors/SpawnBehavior.h`, `/OverTimeBehavior.h`, etc. (implementations)

**Current pattern (SpawnBehavior.h):**

```cpp
class SpawnBehavior final : public Behavior {
public:
    LOT m_lot;           // Data member
    float m_Distance;    // Data member
    
    explicit SpawnBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}
    
    void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, 
                BehaviorBranchContext branch) override;
    void Calculate(...) override;
    void Timer(...) override;
    void End(...) override;
    void Load() override;  // Loads m_lot, m_Distance from CDClient
};
```

**Load implementation pattern (OverTimeBehavior.cpp line 40-49):**

```cpp
void OverTimeBehavior::Load() {
    m_Action = GetInt("action");
    
    CDSkillBehaviorTable* skillTable = CDClientManager::GetTable<CDSkillBehaviorTable>();
    m_ActionBehaviorId = skillTable->GetSkillByID(m_Action).behaviorID;
    
    m_Delay = GetFloat("delay");
    m_NumIntervals = GetInt("num_intervals");
}
```

**Issues:**
- **Class per behavior**: 100+ behavior classes, many with similar logic
- **Hardcoded data fields**: Each behavior has `m_X`, `m_Y` members for parameters
- **Load() methods**: Boilerplate parsing from CDClient table
- **Logic duplication**: Similar behaviors (e.g., SpawnBehavior, DespawnBehavior) replicate logic

### Proposed Architecture: Data-Driven Configuration

Replace behavior class hierarchy with **data-driven behavior instances** loaded from CDClient.

**Pseudocode:**

```cpp
// dGame/dBehaviors/BehaviorConfig.h
struct BehaviorParameter {
    std::string name;
    enum class Type { INT, FLOAT, STRING, VECTOR3 };
    Type type;
    std::string stringValue;
    int32_t intValue;
    float floatValue;
};

struct BehaviorTemplate {
    uint32_t behaviorId;
    std::string behaviorType;  // e.g., "spawn", "overtime", "projectile"
    std::vector<BehaviorParameter> parameters;
    
    template<typename T>
    T GetParameter(const std::string& name, T defaultValue) const {
        for (const auto& param : parameters) {
            if (param.name == name) {
                if constexpr (std::is_same_v<T, int32_t>) {
                    return param.intValue;
                } else if constexpr (std::is_same_v<T, float>) {
                    return param.floatValue;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return param.stringValue;
                }
            }
        }
        return defaultValue;
    }
};

// dGame/dBehaviors/GenericBehavior.h
class GenericBehavior : public Behavior {
public:
    explicit GenericBehavior(const uint32_t behaviorId) 
        : Behavior(behaviorId), m_config(nullptr) {}
    
    void Handle(BehaviorContext* context, RakNet::BitStream& bitStream,
                BehaviorBranchContext branch) override;
    void Load() override;
    
private:
    std::shared_ptr<BehaviorTemplate> m_config;
    
    // Delegate to appropriate handler based on behavior type
    void HandleSpawn(BehaviorContext* context);
    void HandleOverTime(BehaviorContext* context);
    void HandleProjectile(BehaviorContext* context);
    // ... one handler per behavior type
};

// Usage: Same interface, but no subclassing
auto behavior = new GenericBehavior(123);  // Loads config from CDClient
behavior->Handle(context);  // Dispatches to appropriate handler
```

**CDClient schema for behavior parameters (pseudo-SQL):**

```sql
-- Current: Scattered across dozens of behavior-specific tables
-- Proposed: Single unified table
CREATE TABLE BehaviorParameter (
    behaviorId INT,
    parameterName VARCHAR(64),
    parameterType ENUM('int', 'float', 'string', 'vector3'),
    intValue INT,
    floatValue FLOAT,
    stringValue VARCHAR(256),
    vector3X FLOAT,
    vector3Y FLOAT,
    vector3Z FLOAT,
    PRIMARY KEY (behaviorId, parameterName)
);

-- Example data
INSERT INTO BehaviorParameter VALUES
(123, 'spawn_lot', 'int', 456, NULL, NULL, NULL, NULL, NULL),
(123, 'spawn_distance', 'float', NULL, 50.0, NULL, NULL, NULL, NULL),
(456, 'action_skill_id', 'int', 789, NULL, NULL, NULL, NULL, NULL),
(456, 'delay_ms', 'float', NULL, 1000.0, NULL, NULL, NULL, NULL);
```

**Benefits:**
- **Reduced code size**: 100 behavior classes → 1 GenericBehavior + 10 handlers
- **Configuration-driven**: Change behavior by editing CDClient, no code recompile
- **Unified interface**: All behaviors use same `GenericBehavior` class
- **Easier testing**: Test configuration validation separately from execution
- **Runtime tweaking**: Game designers can experiment without programmer involvement (if CDClient is editable in game)

### Risk Assessment

**What could break:**
- **Type safety**: Parameters are weakly typed (string-based lookup)
  - *Mitigation*: Provide type-safe wrappers; validate parameter presence at behavior load time
- **Missing parameters**: Behavior expects parameter that's not in config
  - *Mitigation*: Provide sensible defaults; log warnings on missing parameters
- **Logic errors**: Behavior handler code has bugs
  - *Mitigation*: Unit test handlers independently from configuration

**Backward compatibility:**
- Old behavior subclasses remain; GenericBehavior is new code path
- Gradual migration: Create GenericBehavior for new behaviors; old ones unchanged
- Can deprecate old subclasses after stabilization

### Migration Path

1. **Phase 1 (Week 1):** Design unified parameter table schema
   - Create BehaviorTemplate struct
   - Create BehaviorParameter struct
   - Write CDClient schema migration (add table)

2. **Phase 2 (Week 2):** Implement GenericBehavior dispatcher
   - Create GenericBehavior class
   - Implement 10 behavior type handlers (most common)
   - Write unit tests for each handler

3. **Phase 3 (Week 3):** Migrate CDClient data
   - Export existing behavior data from old tables
   - Transform into unified parameter table
   - Validate equivalence

4. **Phase 4 (Week 4+):** Migrate behavior subclasses
   - Remove old behavior subclass
   - Create parameter set in BehaviorParameter table
   - Verify in staging environment

### Effort: **VERY_LARGE** (4-6 weeks)

- Schema design + tests: 1 week
- GenericBehavior implementation: 1.5 weeks
- CDClient migration: 1.5 weeks
- Parameter validation + bug fixes: 1 week

### Backward Compatibility Strategy

- Keep old behavior subclasses for 1 release
- Provide deprecated warning if old behavior is instantiated
- Support both old and new mechanism in parallel
- Provide migration tool to convert old behaviors to new config format

---

## Summary Table

| Refactoring | Effort | Risk | Impact | Priority |
|-------------|--------|------|--------|----------|
| **1. GameMessages Monolith** | LARGE | MEDIUM | High (6,445-line maintainability) | HIGH |
| **2. Component Storage** | SMALL | LOW | High (perf on hot path) | HIGH |
| **3. CDClient Tables** | MEDIUM | LOW | Medium (startup time) | MEDIUM |
| **4. Script Coupling** | LARGE | MEDIUM | Medium (testability) | MEDIUM |
| **5. Hardcoded Values** | SMALL | VERY LOW | Low (flexibility) | LOW |
| **6. Behavior Data-Driving** | VERY_LARGE | MEDIUM | Low (code cleanup) | LOW |

**Recommended execution order:**
1. Component Storage (quick win: 3-4 days, high impact)
2. GameMessages refactoring (long-term: 6-8 weeks)
3. CDClient Tables (medium: 5-7 days)
4. Script Coupling (medium: 5-7 weeks, parallel with GameMessages)
5. Hardcoded Values (opportunistic: 2-3 days, low priority)
6. Behavior Data-Driving (future: 4-6 weeks, post-stabilization)
