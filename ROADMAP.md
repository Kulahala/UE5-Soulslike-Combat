# Test Long-Term Roadmap

## Purpose And Scope

This document records the long-term direction and adoption gates for `Test`. It is not a source of truth for implemented behavior: live source/assets and `ARCHITECTURE.md` take precedence. Each accepted stage still needs a concrete, short-term implementation plan in `plan.md` before code or Unreal assets change.

The current product direction is a **single-player, level-based action RPG**. The project should grow from its existing Soulslike combat prototype into complete playable levels with systematic equipment and loot, without treating multiplayer or a run-loop-first roguelite structure as architecture requirements.

## First Demo Target

The first complete target is a **20-30 minute single-player Soulslike level RPG demo**. A first-clear playthrough must be able to follow this loop:

```text
Main Menu -> New Game -> Checkpoint -> Explore -> Two normal encounters
-> Elite encounter -> Loot and equip a weapon -> Shortcut -> Boss fog gate
-> Boss -> Completion reward -> Completion screen -> Main Menu
```

The player must be able to die, restart from the last checkpoint, quit, and continue later with the expected level state. This is the definition of "complete" for v1; it is not a promise of a full multi-level game, procedural world, or every Soulslike subsystem.

### Required Demo Content

- A minimal main menu: `New Game`, `Continue`, `Settings`, and `Quit`. `Continue` only becomes available after a valid save exists. v1 Settings must include master/music/SFX volume, look sensitivity, invert-Y, and supported display-mode/resolution controls; it must not be a placeholder screen.
- One hand-authored level with a start checkpoint, two distinct normal combat spaces, one elite encounter, one optional reward branch or shortcut, and one Boss arena.
- One player combat baseline: sword plus shield. The second playable weapon family is a bow; staff/magic is deliberately later.
- Two normal enemy families: melee and archer. The initial elite is a shield melee variant; a caster is later content after the projectile foundation is proven. Normal enemies remain deliberately placed for encounter readability.
- One checkpoint before the Boss, one Boss fog gate, a fixed Boss reward of Gold plus a persistent Boss Remnant/Emblem, and a completion screen. The reward is first-clear proof for v1, not a reason to add a downstream progression system before a second level exists.
- Gold, item pickup, equipment choice, and persistence across a quit/reload. v1 gold persists through death; corpse-recovery punishment is deferred.
- Baseline feedback: checkpoint activation, pickup, encounter start/clear, Boss music, hit feedback, death, and completion feedback.

### Confirmed Checkpoint, Death, And Continue Rules

- v1 has one automatic save slot. `New Game` initializes or replaces that slot; `Continue` is enabled only when the slot contains a valid save.
- `ACheckpointActor` is a Soulslike bonfire/rest point. The player activates it through an explicit interaction, which saves immediately, sets the respawn anchor, restores health/stamina/potions, and resets ordinary enemies.
- On player death, reload to the most recently activated checkpoint. Ordinary placed enemies and uncleared encounters reset; cleared encounters, dead Bosses, opened chests, collected fixed rewards, opened shortcuts, gold, owned items, and equipped items persist.
- Rest points are the only v1 respawn and world-reset boundary, not the only time durable progress is written. Gold, owned items, shortcuts, fixed rewards, encounter clears, and Boss completion write through immediately; quitting from an active or uncleared encounter still does not save transient combat state. `Continue` returns to the latest checkpoint with that encounter restored to `Idle` and its encounter boundaries open, just as after a death.
- Boss completion saves the completed Boss state and its fixed reward while retaining the existing final checkpoint as the respawn anchor. After the completion screen returns to the main menu, `Continue` returns the player to that checkpoint with the Boss dead and its arena boundary open.
- Gold remains the current temporary currency/reward value in v1. Do not add a second Soul currency, corpse-recovery loop, or bonfire stat-upgrade UI until the Demo loop is stable and their roles are deliberately defined.

### First Level Art And Teaching Direction

- The first level is a dark, ruined cathedral-fortress: weathered outer walls and courtyards lead into a Gothic nave, side chapels, and a final altar/crypt Boss arena. This uses the castle as the level shell and the cathedral as its visual identity rather than mixing unrelated themes.
- The immediate player objective is simple: reach and defeat the Boss who holds the cathedral-fortress. A formal quest system is not part of the Demo.
- The intended first-clear route is start bonfire -> outer approach normal melee space -> archer pressure space -> shield elite/crypt encounter -> persistent shortcut back toward the bonfire route -> Boss fog boundary and altar arena.
- Each combat space teaches one already-supported or planned response: ordinary melee teaches timing and stamina, archers teach ranged pressure and target control, the shield elite teaches guard break, enemy stance break, and front criticals, and the Boss combines those lessons without introducing an unrelated mechanic.

### Confirmed Encounter Rules

- Normal patrol and ambient combat enemies are hand-placed. Their reset behavior is driven by rest/death, not by the encounter system.
- Sealed encounters, ambushes, waves, and Boss fights are owned by `AEncounterController`. The player cannot leave an active sealed encounter.
- On safe inner-region entry, the controller spawns or activates the configured enemies; after they visibly appear, it closes its own temporary encounter boundaries. Defeating every participant grants the configured fixed reward and reopens those boundaries.
- Encounter failure means player death before `Cleared`. On the subsequent checkpoint reload, the encounter returns to `Idle`, its boundaries open, and it can be triggered again. A cleared encounter remains cleared across rest, death, and reload.
- Spawn strategy is per encounter: a single wave is a multi-wave definition with one wave; pre-placed enemies can be activated; ambushes and Bosses can spawn from placed `AEncounterSpawnPoint` actors.

### Target Architecture For The Demo

- `USoulslikeGameInstance`: current save-slot selection and map/menu transition context.
- `ATestGameMode`: single-player game flow, save loading, checkpoint respawn, and level completion.
- `UTestSaveGame`: versioned persistent state containing level ID, checkpoint ID, gold, owned item instances, equipped slots, opened shortcuts, cleared encounter IDs, opened chest/reward IDs, and Boss completion state.
- `ACheckpointActor`: placed visual/interactable checkpoint that writes a respawn anchor; it does not become a second save manager.
- `AEncounterController`: 当前是负责安全内区激活、预放置参与者、`Idle -> Active -> Cleared` 和 Controller-owned 临时碰撞/材质边界段的摆放遭遇 Owner。波次生成、奖励、持久化恢复和最终 Boss 表现仍属于后续阶段。
- `AEncounterSpawnPoint`: 当前是只提供稳定 `SpawnPointId` 和变换的小型摆放 Actor。波次成员、敌人类选择和实际生成是未来 encounter-wave 的职责。
- Future fog-wall Mesh, Niagara, audio, and opening presentation extend Controller-owned boundary segments. They are used for Bosses and explicit sealed encounters, never as generic level decoration.

Do not add a global level-flow manager, a universal enemy director, or a Boss-specific C++ base class for the first Boss. `ATestGameMode` plus placed checkpoint and encounter Actors are enough. The first Boss should begin as a configured `AEnemy` Blueprint; introduce a dedicated Boss class or component only when phase state, arena rules, encounter UI, or lifecycle logic no longer fits `AEnemy`.

For the first Demo, pre-placed enemy count is small enough that full map streaming is unnecessary. Enemies that should not act before the player approaches use an explicit activation volume or encounter activation path. True distance-based memory loading/unloading is a later Level Streaming / World Partition decision, not a responsibility of `AEncounterController`.

## Current Architectural Defaults

- Keep the existing C++ combat pipeline, `EActionState`, Montage delegates, AnimNotifies, and DataAsset-driven action configuration as the default foundation. Do not introduce a full general-purpose HFSM while the current state and recovery boundaries remain understandable.
- C++ owns gameplay state, combat resolution, item ownership, and equipment application. Blueprint and UMG own authored presentation, layout, animation, and visual hooks; they must not become the source of truth for inventory, attributes, or damage.
- Keep UMG HUD presentation-only. Widgets may subscribe to component delegates such as `OnHealthChanged`, `OnStaminaChanged`, and `OnGoldChanged`, but must not own or mutate gameplay state.
- Reuse one explicit implementation path when replacing a prototype system. Do not retain a hidden legacy and replacement path merely as insurance.
- Do not adopt multiplayer replication, client authority, or GAS as speculative infrastructure. Each needs a concrete gameplay trigger and an approved stage plan.
- Keep the current `AEnemy` local HFSM for melee, archer, and first caster behavior. Extend its combat range, line-of-sight, retreat, and attack-delivery configuration before considering StateTree or Behavior Tree migration.

## Done Milestones

Completed roadmap work is retained here as a compact, durable record. Do not return it to the active queue or repeat its planning discussion unless a new requirement explicitly reopens it.

- [x] **TODO-01: Game Flow Foundation v1**
  Delivered the single-slot save contract, `MainMenu`, checkpoint interaction and respawn, automatic death reload, Gold write-through, settings, and the first complete New Game / Continue loop. Completed in commit `9119bc2`.

- [x] **TODO-02A1: Encounter Core v1**
  Delivered the reusable C++ encounter lifecycle, pre-placed participant ownership/dormancy, safe rectangle/radial boundary activation, one-shot death completion, and the transform-only spawn-point contract. No formal map encounter, waves, rewards, or persistence behavior was authored.

- [x] **TODO-02A2: Spline Boundary Authoring v1**
  Delivered editable planar linear Spline boundaries, safe-region validation, matching runtime collision segments, a gray-white material boundary prototype, and dormant-hit-path hardening. No formal `TestMap` Controller remains after temporary fixture validation.

- [x] **TODO-03A: Item Definition And Ownership v1**
  Delivered static item definitions, per-Pawn owned-instance and equipment-slot caches, GameInstance-mediated transactional SaveGame writes, and semantic restore on every spawned Pawn. User validation covered Grant/Equip, rest reload, death reload, `Continue`, a fresh PIE session, and New Game reset. This data-only stage intentionally does not materialize, attach, or remove weapon/shield Actors.

- [x] **TODO-03B-A: World Pickup Conversion v1**
  Delivered persistent authored world pickup conversion for the fixed `TestMap` DarkKnight sword and shield. Successful `E` collection atomically writes one owned item instance plus its claimed-reward ID, then removes the world Actor; death reload, fire reload, `Continue`, and New Game reset were user-validated. The stage deliberately leaves equipment slots, materialized weapon/shield Actors, pickup UI, and audio unchanged.

- [x] **TODO-03B-B1: Bonfire Activation And Services v1**
  Delivered SaveGame v2's separate writable-progress and respawn-anchor contracts, first-use checkpoint rest/activation, pre-bonfire PlayerStart death recovery, and a non-pausing protected bonfire menu with Rest and Leave. Gold and fixed-item persistence remain immediate before the first fire; New Game/Continue failure now returns safely to the menu instead of retaining a replacement session or black screen. Completed in commit `b80a017`.

- [x] **TODO-03B-B2: Bonfire Loadout And Materialization v1**
  Delivered fire-only MainHand/OffHand selection, stable `InstanceId` UI mapping, transactional immediate slot writes, and transient sword/shield materialization. Active changes replace the visible equipment without resting or reloading; empty slots naturally disable the matching combat action. Death, rest, Continue, and a new PIE session restore the saved selection. User compilation and PIE validation passed; the post-fix strict review also verified the shared runtime weapon Owner-before-`BeginPlay` contract. Completed in this commit.

- [x] **TODO-03B-C1: Item Claim Transaction Failure Injection v1**
  Delivered a development-only, one-shot save-failure injection scoped to the fixed world-item claim transaction, plus claimed-reward evidence in `ItemDebugDump`. User compilation and PIE validation confirmed that an injected shield claim left the sword instance, MainHand selection, and sword reward ID unchanged across a map reload; an unarmed retry then persisted both reward IDs. Normal and adversarial review found no scope leak into Gold, checkpoints, equipment-slot writes, or runtime loadout materialization. Completed in this commit.

- [x] **TODO-03B-C2: Pickup Feedback And Empty-Slot Auto-Equip v1**
  Delivered definition-owned pickup audio and a candidate-first fixed-world claim that atomically persists the new instance, reward ID, and only an empty compatible equipment slot. Automatic first-equip is silent apart from the pickup sound, never replaces an occupied slot, and leaves the world Actor intact when candidate preparation or persistence fails. User compilation and PIE validation passed; normal and adversarial review found no transaction, sound-routing, or materialization lifecycle defect.

- [x] **TODO-04A: Projectile Delivery Core v1**
  Delivered the pure C++ `FCombatHitRequest` / `FCombatHitResult` resolver shared by melee and projectile delivery, plus a zero-gravity, first-blocking-hit `ACombatProjectile` with immutable launch configuration, deferred Owner/Instigator setup, bounded lifetime and non-Shipping PIE commands. The resolver preserves current team, block/parry, damage, poise, hit-context, `GetHit` and stance-break ordering; `AWeapon` retains CameraShake, HitStop and per-swing de-duplication. Projectiles resolve only `IHitInterface` recipients, so ordinary blocking Actors only stop and destroy them. User compilation and PIE validation covered enemy hit, wall priority, block, same-team feedback, timeout, reload cleanup, the capsule-overlap spawn fix and the non-combat-Actor gate. No bow, ranged input, archer AI, assets, map content or persistence was added. Completed in this commit.

- [x] **TODO-04B-A: Bow Core And Arrow Economy v1**
  Delivered native `ABow` loadout compatibility, right-click aim plus normal left-release firing, persistent finite arrow quantities, scoped quantity write-failure injection, and the `Projectile` Object Channel that makes simultaneous projectiles pass through one another. `AMyCharacter::TryStartAction()` remains the action arbiter; no formal bow/arrow world pickup, HUD, mesh, animation, backpack, field swapping, target correction, or ranged AI was added. User compilation and PIE validation covered quantity persistence, zero-ammo and write-failure rejection, simultaneous shots, wall priority, reload restoration, focus-loss cancellation, sword charge cancellation, and the post-review forced-interrupt aim fix. Normal and adversarial review passed with two bounded follow-up risks. Completed in this commit.

## TODO Queue

These TODOs are accepted future work, not permission to start implementation immediately. A queued item must be small enough to produce one independently verifiable result. Queue position follows prerequisites, validation dependencies, and the player-facing loop rather than numeric ID or append order. When an item becomes the next stage, move only that item out of this queue and write its complete implementation plan in `plan.md` first. On completion, record stable facts in `ARCHITECTURE.md` and move the compact result into `Done Milestones`; do not turn this roadmap into a stage log.

**Encounter authoring decision:** reusable Controller behavior and Spline authoring were already proven by `TODO-02A1/A2`. The former standalone `TODO-02B` is deliberately not marked Done and is absorbed as a mandatory adoption condition of `TODO-06A`: do not place a permanent `TestMap` encounter solely to repeat core validation. `TODO-02C` waits until that first permanent map-owned Controller exists.

### Ranged Combat

- [ ] **TODO-04B-A2: Ammo Containers And Bonfire Refill v1**
  Prerequisite: `TODO-04B-A` is complete. Split arrow ownership into a persistent loaded-ammo container and reserve/backpack quantity with authored per-ammo capacity and stack limits. Shots consume only loaded arrows; Debug grants, future arrow bundles, drops, and shops add reserve arrows. Rest performs one rollback-safe transfer from reserve into the loaded container up to its capacity, never creates arrows from nothing, and does not reload or swap equipment. The quantity transaction must consume only the Pawn-validated instance records, never a merely matching raw SaveGame definition record. Keep this as data and checkpoint-service work: no generic backpack UI, field reloading, battle-time inventory access, or broader item sorting. `TODO-04B-B` must use this boundary for its arrow bundle and HUD.

- [ ] **TODO-04B-B: Bow Presentation And World Pickup v1**
  Prerequisite: `TODO-04B-A2` is validated and a legal DarkKnight-compatible bow mesh plus aim/release animations are selected. Author the visible bow, visible arrow, persistent `TestMap` bow and 20-arrow bundle pickups, permanent loaded-arrow HUD, and aim-only dot reticle. Do not substitute sword animations as a final bow presentation.

- [ ] **TODO-04C: Lock Target Switching v1**
  While locked on, use horizontal mouse movement on the existing `LookAction` to select the nearest valid target on the requested screen side. Include a configurable swipe threshold, re-arm threshold, and cooldown; preserve the current target when no eligible target exists. This stage changes targeting only, not projectile behavior.

- [ ] **TODO-04D: Archer Enemy v1**
  Configure an archer `AEnemy` variant using the projectile core, readable line-of-sight behavior, and the existing local HFSM. Verify mixed melee/archer pressure without a Behavior Tree, StateTree, or caster system.

### Combat Punish And Criticals

- [ ] **TODO-05A: Player Guard Break v1**
  Add `EAS_GuardBroken` and its dedicated montage, recovery window, input lock, and state-guarded cleanup. A shield hit that breaks stamina resolves as blocked damage, consumes remaining stamina, drops guard, then enters GuardBroken; an unblocked hit received while already `EAS_Exhausted` enters the same state. Do not add a player Poise HUD.

- [ ] **TODO-05B: Front Critical v1**
  Complete the existing enemy `EES_StanceBreak` loop with one front critical interaction. Align a valid stance-broken enemy via Motion Warping and apply configured critical damage from an AnimNotify. Do not add backstab, generic finisher infrastructure, or Boss criticals.

### Level And Boss Slice

- [ ] **TODO-06A: First Level Route v1**
  Block out and author the critical route: start checkpoint, two normal combat spaces, one elite space, and one optional shortcut/reward branch. This stage absorbs the former `TODO-02B`: author the first permanent `AEncounterController` in a normal combat space with a stable `EncounterId`, closed Spline boundary, and pre-placed participants, then validate `Idle -> Active -> Cleared` in the real level. Use the completed encounter, equipment, ranged, and guard-break systems; do not add waves, encounter persistence, Boss content, a completion screen, or a new framework here.

### Encounter Persistence And Rewards

- [ ] **TODO-02C: Encounter Waves, Clear Persistence, And Restore v1**
  Prerequisite: `TODO-06A` has authored and validated its first permanent Controller-owned encounter. Add configured spawn waves, active-participant tracking, `EncounterId -> ClearedEncounterIds` write-through, and reload/Continue restoration. Verify a one-wave definition, a multi-wave definition, uncleared death reset to `Idle` with boundaries open, and cleared reload/Continue restoration. Fixed rewards remain an equipment-loot stage concern.

`TODO-02C` owns only Controller-managed encounter persistence. It is not a final catch-all migration for pickups, equipment, shortcuts, rewards, or Boss state. Each durable feature must add its own read/write path when its stable ID and reset contract are implemented, so that feature can be validated in the player loop that introduces it. `TODO-07B` is the final cross-system Demo regression route; it verifies completed persistence paths together and fixes regressions, but does not defer their original implementation.

- [ ] **TODO-03C: Authored Rewards And Drops v1**
  Prerequisite: `TODO-02C` has a durable clear event in the first authored level. Add one authored reward/drop-table path shared by encounter completion and fixed world rewards. Verify a single equipment reward and Gold write-through. Do not add random affixes, durability, crafting, merchants, or inventory sorting.

- [ ] **TODO-06B: First Boss Encounter v1**
  Add one Boss fog-boundary presentation, configured first Boss as an `AEnemy` variant, fixed Gold plus Remnant/Emblem reward, and Boss completion persistence. Do not introduce a Boss-specific C++ base unless the existing enemy and encounter boundaries demonstrably fail.

- [ ] **TODO-06C: Completion Flow v1**
  Add a completion screen and return to `MainMenu`. `Continue` must return to the final checkpoint with the Boss defeated, reward retained, and arena boundary open.

### Polish And Regression

- [ ] **TODO-07A: Demo Feedback Polish v1**
  Tune combat readability, navigation, landmarks, sound/music, VFX, UI feedback, checkpoint/reload feedback, and visible error recovery using the completed level loop. Avoid introducing new gameplay systems.

- [ ] **TODO-07B: Demo Regression Route v1**
  Establish the repeatable manual PIE matrix for the full demo: New Game, Continue, checkpoint, normal encounter reset, cleared encounter restore, equipment persistence, bow/archer, guard break/critical, Boss completion, and completion Continue. Fix regressions only; this is not a feature stage.

## Known Risks And Validation Debt

This is a small durable register for non-blocking review findings that can affect a later stage or release decision. Each entry states the affected boundary, current evidence, and the condition that requires resolution. It is not a second TODO queue: a blocker must be fixed in its current stage, transient working notes stay in `plan.md`, and an entry becomes a TODO only when its resolution has a defined implementation or validation stage.

- **Scalar Gold and generic persistent-marker writers do not yet expose write failure as a failed operation.** `UpdateGold()` changes the in-memory Gold value before ignoring `SaveNow()`'s result; `AddPersistentId()` adds its `FName` then returns success without checking that result. Fixed world-pickup, equipment-slot, and checkpoint transactions have dedicated rollback paths, so this does not invalidate the current B1 flow or the existing equipment transaction boundary that B2 will reuse. **Resolution condition:** before `TODO-02C` or `TODO-03C` makes a `MarkShortcutOpened()` / `MarkEncounterCleared()` / `MarkBossCompleted()` result gate a world Actor removal, boundary opening, reward, or other irreversible presentation, standardize these APIs on explicit failure results and defined rollback/retry behavior, then cover the branch with controlled save-write failure injection.

- **World-pickup invalid-authoring downgrade is source-reviewed, not fixture-validated.** Missing/duplicate `PersistentId` and unknown `ItemDefinitionId` paths warn and prevent an invalid reward from being granted; the valid sword/shield path is user-validated. **Resolution condition:** before `TODO-03C` adds another persistent fixed reward or drop source, create an unsaved temporary fixture for missing ID, duplicate ID, and unknown definition cases, then verify warnings, no duplicate item record, and no erroneous Actor removal.

- **A prepared projectile's activation-failure refund is still two durable writes.** `ReleaseBowArrow()` currently consumes and saves an arrow before `ActivateConfiguredProjectile()`; the current native prepared projectile should activate synchronously, but a future custom projectile class could make that branch reachable. If the separate refund write also fails, the player loses one arrow without firing one. **Resolution condition:** before `TODO-04B-B` introduces `BP_PlayerArrow` or any non-native projectile class, make prepared activation an enforced non-failing invariant after preparation, or replace the flow with a real reservation/commit transaction and failure-injection coverage for both writes.

- **Quantity consumption targets raw SaveGame order rather than the Pawn-validated cache.** Restore ignores malformed or duplicate item records for the live Pawn, while `ConsumeOwnedItemQuantity()` currently finds matching `DefinitionId` records in the original saved array. A pre-existing malformed arrow record placed before a valid one can therefore make the current session appear to spend an arrow that returns after reload. Clean current saves do not trigger this. **Resolution condition:** `TODO-04B-A2` must make quantity transactions consume an explicit set of validated instance IDs while introducing loaded and reserve ammo containers, then verify a deliberately malformed unsaved fixture cannot alter the valid-arrow count.

## Equipment And Loot Direction

- Use DataAssets for static authored item definitions that reference meshes, icons, equip visuals, action sets, sounds, VFX, rarity policy, and other Unreal assets.
- Use a runtime item-instance data structure for ownership, quantity, rolled affixes, durability or upgrade level if those become real gameplay. Do not encode mutable item state back into a shared definition DataAsset.
- World pickup Actors are presentation and interaction endpoints. They carry or resolve a drop result, validate collection, then transfer the item record into player-owned gameplay state. They are not the permanent inventory database.
- Keep inventory/equipment APIs narrow. Start with actual player needs such as equip, unequip, add/remove a pickup, and query an equipped item. Add save/restore only with the checkpoint and persistence stage. Avoid a generic item service or universal interface until multiple unrelated systems truly need the same operation.
- Manual weapon and shield loadout changes remain restricted to resting at a checkpoint. The completed C2 first-equip rule is a narrow exception: a newly claimed fixed world pickup may fill only its empty compatible slot, without replacing an existing item or exposing field selection. Do not add pause-menu or encounter-time hot-swapping before a real backpack stage.
- Gold remains a scalar player attribute/reward value. It should not be forced into the item-instance model merely because both are rewards.
- Every drop source should use one authored table or explicit reward definition. Do not scatter weighted random selection through enemy, chest, and level Blueprints.

## Adoption Gates

### GAS

Recommendation: retain the current combat pipeline until a real ability/status system needs it.

Adoption conditions: evaluate GAS only when several independently authored skills, stackable buffs/debuffs, elemental status effects, reusable cooldown/cost rules, or designer-authored effect combinations cause the existing C++ and DataAsset model to duplicate gameplay-rule code. A GAS stage must define migration ownership and remove the replaced path; it must not leave two damage/effect pipelines active.

### Backpack, Full Inventory, Crafting, And Merchants

Recommendation: preserve checkpoint-limited manual loadout changes and the narrow empty-slot first-equip rule; do not add a backpack or field loadout UI merely to expose the current fixed sword and shield.

Adoption conditions: create a backpack/loadout stage when the player carries multiple meaningful unequipped items, needs comparison or sorting, has stackable consumables, or has a player-visible reason to change equipment away from a bonfire. That stage must decide in-combat restrictions, input, UI focus, selection confirmation, and persistence before it moves manual loadout access out of fire services. Crafting, vendors, selling, storage, or trading each need their own gameplay reason; do not add grid inventory UI merely to display a small fixed equipment set.

### Save And Checkpoint Expansion

Recommendation: establish the one-slot checkpoint model in `TODO-01`, then keep later persistence additions versioned and narrow.

Adoption conditions: expand the initial SaveGame schema only when another persistent system has a defined reset contract, stable IDs, and a player-visible reason to survive rest/death/reload. Store stable IDs and runtime item instance data, not raw pointers to world Actors or Widget state.

### Persistent World IDs

Recommendation: 在首个单地图 Demo 中保留作者填写、全局命名空间化的 `FName` 持久世界 ID，例如 `TestMap_CryptEliteEncounter`、`TestMap_NaveShortcut` 和 `TestMap_AltarBossEncounter`。项目确实扩展为多地图前，应优先将存档契约迁移为结构化 map-scoped 身份，例如 `{ PersistentMapId, LocalPersistentId }` 或按地图分组的持久状态，而不是无限期把两个 scope 拼进一个字符串。

Adoption conditions: 在加入第二张 gameplay map 前，或在同一存档可以写入来自多张地图的 encounter/shortcut/reward/Boss 状态前，必须作出并版本化这项决策。`PersistentMapId` 必须是作者填写的稳定内容 ID，不是可变的 map package path。迁移必须更新 `SaveVersion` 并显式映射 legacy ID；绝不能从 Actor name/label、指针、运行时 GUID 或 External Actor package path 推导持久 ID。

### Enemy Profiles And Boss Boundaries

Recommendation: keep the existing `AEnemy` local HFSM for melee, archer, and first caster behavior. Use attack/profile data to vary range, line-of-sight, retreat distance, attack delivery, cooldown, and presentation before splitting C++ classes.

Adoption conditions: introduce enemy profile DataAssets when three or more enemy variants repeat the same AI, stat, reward, sensing, and attack setup with only configuration differences. Consider StateTree or Behavior Tree only when the local HFSM becomes unreadable across multiple enemy families, repeated tactical branches dominate `AEnemy`, or future enemies need multi-step plans that cannot remain clear in the existing flow. Introduce a Boss-specific class or component only when a Boss needs persistent phase state, encounter-owned UI/objectives, arena rules, or lifecycle logic outside normal enemies.

### PCG

Recommendation: keep current levels hand-authored until production pressure is visible.

Adoption conditions: evaluate PCG when multiple levels repeat the same dressing/encounter work, themes need reusable generation rules, or manual placement becomes the bottleneck. Keep gameplay-critical objectives, pickups, checkpoint placement, and encounter authority explicit even if environmental dressing becomes procedural.

### Staff, Magic, And Focus Resource

Recommendation: ship the bow as the first non-melee weapon family before adding a staff or spell system.

Adoption conditions: evaluate staff/catalyst equipment only after bow projectiles, ranged input, equipment ownership, and save/load all work in the complete Demo loop. A staff stage must decide the Focus/Mana resource, spell-slot policy, cast interruption rules, elemental effects, and whether spells use projectiles or area effects. Do not use stamina as a temporary magic resource merely to avoid making that decision.

### Caster Enemy

Recommendation: add a caster only after `TODO-04` validates projectile delivery with the player bow and archer.

Adoption conditions: introduce the first caster when the Demo needs a third combat pressure pattern beyond melee pursuit and direct ranged fire. It should begin as an `AEnemy` configuration with a readable cast wind-up and one distinct behavior such as a slow projectile, tracking projectile, or marked area effect. Do not add a separate AI framework, player Focus resource, or elemental status system merely because an enemy casts magic.

### Soul Progression And Corpse Recovery

Recommendation: keep current gold behavior through the first complete Demo loop, then decide whether it becomes the single progression currency or remains a shop currency separate from Souls/Essence.

Adoption conditions: evaluate bonfire stat upgrades, enemy Soul drops, or corpse recovery only after the Demo has stable checkpoint persistence, equipment ownership, and a concrete long-term progression need. A progression stage must define its currency role, death-loss/recovery rule, save fields, and bonfire UI before any new counter is added to the HUD.

### Quest And Currency-Recovery Loops

Recommendation: use one clear level objective and a single Boss reward for the first Demo rather than adding formal quest, vendor, crafting, or corpse-recovery systems.

Adoption conditions: revisit these systems only after the Demo has at least one complete saved level loop and player feedback shows a concrete need for longer-term goals, currency sinks, equipment progression, or death-risk stakes. Each must identify the persistent state it owns before adding UI.

## Explicit Non-Goals For The Current Direction

- Multiplayer, replication, client-side gameplay authority, Listen Server validation, and rollback/prediction work.
- A generic framework for every future item, weapon, enemy, or quest before a current gameplay case needs it.
- A full HFSM, Behavior Tree/StateTree migration, or GAS conversion merely for architectural fashion.
- Bulk PCG generation or importing all external marketplace assets into active gameplay before a level-production need exists.

## Stage Completion Standard

Each future stage must define success in `plan.md` before implementation: the gameplay loop it proves, affected source/assets, validation steps, documentation impact, and commit boundary. After implementation, update `ARCHITECTURE.md` only with stable implemented facts; move future decisions or optional ideas back into this roadmap with their adoption conditions.
