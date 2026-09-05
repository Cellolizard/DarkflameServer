# dGame/ TODO/FIXME Triage

Snapshot of every `TODO` / `FIXME` / `XXX` / `HACK` marker found under `dGame/`
as of **2026-05-25**, with a per-item triage decision. The goal is to avoid
re-litigating these comments on the next audit pass: if a marker appears here
and the surrounding code is unchanged, the decision below still stands.

This is **not** a live issue tracker. File:line citations were accurate on
2026-05-25 and have already drifted (example: GameMessages reputation TODO was
`:5769`, now `:5611` after inventory-handler extract). Re-grep before acting.

Spot-check 2026-09-05 on this tip: the regenerate command produced **48** hits
(51 at audit, minus the two fix-now items and the `Entity.h` `GetComponents`
`TODO: Remove` that the later flat-array commit deleted). Do not treat 51 as
the current count.

## How to regenerate the inventory

```sh
grep -rn --include="*.cpp" --include="*.h" --include="*.hpp" \
  -E '\b(TODO|FIXME|XXX|HACK)\b' dGame/ | sort
```

At the time of this audit the command produced **51** hits.

## Summary

| Bucket | Count (2026-05-25) | Action |
|---|---:|---|
| Fix now | 2 | Addressed in the same PR as this document |
| Subsumed by Phase 2 / Phase 3 | 15 | Leave in place; will be rewritten by larger refactors |
| Out of scope | 34 | Leave in place; needs gameplay/design work, missing systems, or stand-alone investigation |

"Phase 2" and "Phase 3" named the April 2026 roadmap in
[`context-files/14_SYNTHESIS_AND_RECOMMENDATIONS.md`](../context-files/14_SYNTHESIS_AND_RECOMMENDATIONS.md).
**That document is retired.** Do not execute its Phase 1–3 as a plan. Living
map: [`context-files/00_INDEX.md`](../context-files/00_INDEX.md) and
[`context-files/STATUS.md`](../context-files/STATUS.md). Live backlog:
`/workspace/lu-status/project-bearing.md` §6.

On this tip, several "subsumed" items have already *started* (not finished):
flat-array component storage landed (`533f0fcb`); `MessageHandlerRegistry` +
four inventory handlers extracted; `GetEntitiesByComponent` is indexed. The
leftover GameMessages switch, `InventoryComponent` 3-class split, and most
TODO text in the tables below are still present. Ghosting TODOs in
`GhostComponent` overlap `#1947` on `main` — still not a finished system.

## Fix-now items

These were addressed alongside this document.

| File:Line | Marker | Resolution |
|---|---|---|
| `dGame/dBehaviors/PropertyTeleportBehavior.cpp:64` | `// TODO unused` on `m_CancelIfInteracting` read | Field removed (also from `.h:20`). The boolean was written once and never read; no consumer existed. |
| `dGame/dMission/Mission.cpp:35` | `// TODO Figure out why these missions are broken sometimes` | Comment rewritten. `g_TestedMissions = {773..777}` is a debug-logging gate consumed by `MissionTask` to emit extra `LOG()` calls for a developer-selected sample; it is not a list of bugs. |

## Subsumed by Phase 2 / Phase 3

Each of these will be rewritten or eliminated by a larger refactor on the
roadmap. Fixing them in isolation would create churn the bigger change has to
re-touch.

### GameMessages.cpp monolith decomposition (was: Phase 2 IMessageHandler split)

Doc 14 (retired) called for splitting `dGame/dGameMessages/GameMessages.cpp`.
On this tip the file is **6287** lines (`main` 6469). `MessageHandlerRegistry`
exists; Equip/Unequip/Move/Remove are extracted. Combat/property/mission/movement
handlers and the leftover switch are not. Line numbers below are 2026-05-25.

| File:Line | Marker |
|---|---|
| `dGame/dGameMessages/GameMessages.cpp:1031` | `// FIXME: this is a bad place to need to do a conversion because we have no clue whether data is utf8 or plain ascii` |
| `dGame/dGameMessages/GameMessages.cpp:2469` | `TODO Apparently the bricks are supposed to be taken via MoveInventoryBatch?` |
| `dGame/dGameMessages/GameMessages.cpp:4889` | `//TODO: If targetID != 0, and we have one of the "perform emote" missions, complete them.` |
| `dGame/dGameMessages/GameMessages.cpp:5111` | `// FIXME: only really need utf8 conversion for the name, so move that up?` |
| `dGame/dGameMessages/GameMessages.cpp:5769` | `// TODO This needs to be implemented when reputation is implemented for getting hot properties.` |

### InventoryComponent 3-class split (was: Phase 3)

Doc 14 (retired) called for splitting `InventoryComponent` into separate
equipment, inventory, and item-set responsibilities. That split has **not**
landed. Handler extract off `GameMessages` is a different change.

| File:Line | Marker |
|---|---|
| `dGame/dComponents/InventoryComponent.cpp:1067` | `// TODO Something needs to send the remove buff GameMessage as well when it is unequipping items that would remove buffs.` |
| `dGame/dComponents/ModelComponent.cpp:243` | `// TODO move to the inventory` |
| `dGame/dInventory/Item.h:18` | `* TODO: ideally this should be a component` |

### Component flat-array overhaul (was: Phase 2)

Doc 14 (retired) called for replacing `unordered_map<eReplicaComponentType, Component*>`
with a flat array. **The storage change landed** (`533f0fcb`, `Entity::m_ComponentArray`).
The two remaining markers below were *not* folded into that commit.

| File:Line | Marker |
|---|---|
| `dGame/Entity.h:219` (2026-05-25) | `// TODO: Remove` on `GetComponents()` getter. **Gone** on this tip — the getter remains as a template over the flat array, without that TODO. |
| `dGame/dComponents/PetComponent.cpp:50` | `// TODO: Make reference when safe` on `m_PetInfo` value copy. Safe today because CDClient is read-only post-load, but the conversion is small enough to fold into the component overhaul rather than do separately. |
| `dGame/dComponents/BuffComponent.cpp:406` | `// TODO: change this if to if (buff.cancelOnZone || buff.cancelOnLogout) handling at some point.  No current way to differentiate between zone transfer and logout.` Needs a context parameter from the caller, which the handler split will provide. |

### Property behaviors subsystem rework

The `dPropertyBehaviors/` tree has a recurring TODO theme around serializing
in-progress behavior state. The whole subsystem would need to be redesigned
together rather than patched per-call-site.

| File:Line | Marker |
|---|---|
| `dGame/dPropertyBehaviors/ControlBehaviors.cpp:64` | `// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet` |
| `dGame/dPropertyBehaviors/ControlBehaviors.cpp:118` | `// TODO` |
| `dGame/dPropertyBehaviors/PropertyBehavior.cpp:144` | `// TODO Serialize the execution state of the behavior` |
| `dGame/dPropertyBehaviors/Strip.cpp:172` | `// TODO replace with switch case and nextActionType with enum`. Requires a full inventory of `Action::m_Type` string values across CDClient + runtime; not commit-sized. |

## Out of scope

Items left as-is because they require gameplay/design knowledge, depend on
unimplemented systems, or are stand-alone investigations that don't fit a
mechanical cleanup pass.

### Behavior stubs and unimplemented checks

| File:Line | Marker | Why deferred |
|---|---|---|
| `dGame/dBehaviors/JetPackBehavior.cpp:47` | `// TODO: Implement proper jetpack checks` | Needs jetpack-mechanics domain knowledge |
| `dGame/dBehaviors/SwitchMultipleBehavior.cpp:36` | `// TODO` (bare stub) | No context to act on |
| `dGame/dBehaviors/TacArcBehavior.cpp:174` | `const auto blocked = false; // TODO` | Line-of-sight check needs gameplay decision |
| `dGame/dBehaviors/TacArcBehavior.cpp:212` | `// FIXME: use bounding spheres at some point` | Depends on a bounding-sphere system that doesn't exist yet |
| `dGame/dBehaviors/VerifyBehavior.cpp:32` | `// TODO` (bare stub) | No context to act on |

### Preconditions stubs

`Preconditions::Check` has seven enum cases that always return `false` pending
implementation of the underlying system (racing licenses, Lego Club, pet
taming, etc.). Each is a distinct missing feature.

| File:Line | Precondition |
|---|---|
| `dGame/dUtilities/Preconditions.cpp:155` | `PetDeployed` |
| `dGame/dUtilities/Preconditions.cpp:163` | `TeamCheck` |
| `dGame/dUtilities/Preconditions.cpp:165` | `IsPetTaming` |
| `dGame/dUtilities/Preconditions.cpp:183` | `HasRacingLicence` |
| `dGame/dUtilities/Preconditions.cpp:185` | `DoesNotHaveRacingLicence` |
| `dGame/dUtilities/Preconditions.cpp:187` | `LegoClubMember` |
| `dGame/dUtilities/Preconditions.cpp:189` | `NoInteraction` |

### Missing features and protocol unknowns

| File:Line | Marker |
|---|---|
| `dGame/dComponents/GhostComponent.cpp:92` | `// TODO: disabled for now while bugs are fixed` |
| `dGame/dComponents/GhostComponent.cpp:123` | `// TODO: disabled for now while bugs are fixed` |
| `dGame/dComponents/HavokVehiclePhysicsComponent.cpp:79` | `local_space_info. TODO: Implement this` (network field) |
| `dGame/dComponents/HavokVehiclePhysicsComponent.cpp:90` | `remote_input_ping TODO: Figure out how this should be calculated` (magic constant from captures) |
| `dGame/dComponents/MovingPlatformComponent.cpp:113` | `// TODO` (bare stub) |
| `dGame/dComponents/MovingPlatformComponent.cpp:275` | `// TODO: Send event?` |
| `dGame/dComponents/PetComponent.cpp:955` | `// TODO: Go to player` (movement state) |
| `dGame/dComponents/PhysicsComponent.cpp:71` | `// TODO Fix physics simulation to do simulation at high velocities due to bullet through paper problem...` |
| `dGame/dComponents/QuickBuildComponent.cpp:436` | `// TODO: fix?` (question, not action) |
| `dGame/dComponents/SkillComponent.cpp:204` | `// TODO: need to check immunities on the destroyable component, but they aren't implemented` |
| `dGame/dComponents/SkillComponent.cpp:377` | `// TODO There is supposed to be an implementation for homing projectiles here` |
| `dGame/dComponents/TriggerComponent.cpp:183` | `/*TODO*/` |
| `dGame/dComponents/TriggerComponent.cpp:450` | `// TODO add physics entity if there isn't one` |
| `dGame/dComponents/TriggerComponent.cpp:452` | `// TODO remove Phsyics entity if there is one` |
| `dGame/dMission/Mission.cpp:639` | `// TODO` (bare stub in `Mission` body) |
| `dGame/dUtilities/Loot.cpp:334` | `// TODO should be scene based instead of radius based` (design change) |
| `dGame/dUtilities/Mail.cpp:373` | `// TODO: Echo to chat server` |
| `dGame/Entity.cpp:1280` | `// TODO: Implement BBB Component` |
| `dGame/Entity.cpp:484` | `// TODO also split on space here however we do not have a general util for splitting on multiple characters yet.` |
| `dGame/UserManager.cpp:243` | `//TODO: Pick the most recent played index.` (character list ordering) |

### Dev-only / low-value nits

| File:Line | Marker | Why skipped |
|---|---|---|
| `dGame/dUtilities/SlashCommands/DEVGMCommands.cpp:487` | `// FIXME: use fallible ASCIIToUTF16 conversion` | Dev-only `/playeffect` command path; would require adding a new util. Low value. |
| `dGame/dUtilities/SlashCommands/DEVGMCommands.cpp:1114` | `// FIXME: unnecessary utf16 re-encoding just for error` | Dev-only error path. Negligible cost. |
