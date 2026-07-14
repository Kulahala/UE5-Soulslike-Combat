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
- Rest points are the only v1 respawn and world-reset boundary, not the only time durable progress is written. Gold, owned items, shortcuts, fixed rewards, encounter clears, and Boss completion write through immediately; quitting from an active or uncleared encounter still does not save transient combat state. `Continue` returns to the latest checkpoint with that encounter restored to `Idle` and its gates open, just as after a death.
- Boss completion saves the completed Boss state and its fixed reward while retaining the existing final checkpoint as the respawn anchor. After the completion screen returns to the main menu, `Continue` returns the player to that checkpoint with the Boss dead and its arena gate open.
- Gold remains the current temporary currency/reward value in v1. Do not add a second Soul currency, corpse-recovery loop, or bonfire stat-upgrade UI until the Demo loop is stable and their roles are deliberately defined.

### First Level Art And Teaching Direction

- The first level is a dark, ruined cathedral-fortress: weathered outer walls and courtyards lead into a Gothic nave, side chapels, and a final altar/crypt Boss arena. This uses the castle as the level shell and the cathedral as its visual identity rather than mixing unrelated themes.
- The immediate player objective is simple: reach and defeat the Boss who holds the cathedral-fortress. A formal quest system is not part of the Demo.
- The intended first-clear route is start bonfire -> outer approach normal melee space -> archer pressure space -> shield elite/crypt encounter -> persistent shortcut back toward the bonfire route -> Boss fog gate and altar arena.
- Each combat space teaches one already-supported or planned response: ordinary melee teaches timing and stamina, archers teach ranged pressure and target control, the shield elite teaches guard break, enemy stance break, and front criticals, and the Boss combines those lessons without introducing an unrelated mechanic.

### Confirmed Encounter Rules

- Normal patrol and ambient combat enemies are hand-placed. Their reset behavior is driven by rest/death, not by the encounter system.
- Sealed encounters, ambushes, waves, and Boss fights are owned by `AEncounterController`. The player cannot leave an active sealed encounter.
- On inner trigger entry, the controller spawns or activates the configured enemies; after they visibly appear, it closes all referenced `AEncounterGate` fog barriers. Defeating every participant grants the configured fixed reward and opens the gates.
- Encounter failure means player death before `Cleared`. On the subsequent checkpoint reload, the encounter returns to `Idle`, its gates open, and it can be triggered again. A cleared encounter remains cleared across rest, death, and reload.
- Spawn strategy is per encounter: a single wave is a multi-wave definition with one wave; pre-placed enemies can be activated; ambushes and Bosses can spawn from placed `AEncounterSpawnPoint` actors.

### Target Architecture For The Demo

- `USoulslikeGameInstance`: current save-slot selection and map/menu transition context.
- `ATestGameMode`: single-player game flow, save loading, checkpoint respawn, and level completion.
- `UTestSaveGame`: versioned persistent state containing level ID, checkpoint ID, gold, owned item instances, equipped slots, opened shortcuts, cleared encounter IDs, opened chest/reward IDs, and Boss completion state.
- `ACheckpointActor`: placed visual/interactable checkpoint that writes a respawn anchor; it does not become a second save manager.
- `AEncounterController`: placed trigger that coordinates gates, fog collision, spawn points, alive combatants, completion reward, and an `EncounterId` for persistence. Its minimal state machine is `Idle -> Active -> Cleared`.
- `AEncounterSpawnPoint`: small placed actor that supplies a spawn transform and wave membership to an encounter; it does not own encounter state or reward decisions.
- `AEncounterGate`: visible gate/fog-wall presentation and collision controlled by its encounter. A fog wall is used for Bosses and explicit sealed encounters, never as generic level decoration.

Do not add a global level-flow manager, a universal enemy director, or a Boss-specific C++ base class for the first Boss. `ATestGameMode` plus placed checkpoint and encounter Actors are enough. The first Boss should begin as a configured `AEnemy` Blueprint; introduce a dedicated Boss class or component only when phase state, arena rules, encounter UI, or lifecycle logic no longer fits `AEnemy`.

For the first Demo, pre-placed enemy count is small enough that full map streaming is unnecessary. Enemies that should not act before the player approaches use an explicit activation volume or encounter activation path. True distance-based memory loading/unloading is a later Level Streaming / World Partition decision, not a responsibility of `AEncounterController`.

## Current Architectural Defaults

- Keep the existing C++ combat pipeline, `EActionState`, Montage delegates, AnimNotifies, and DataAsset-driven action configuration as the default foundation. Do not introduce a full general-purpose HFSM while the current state and recovery boundaries remain understandable.
- C++ owns gameplay state, combat resolution, item ownership, and equipment application. Blueprint and UMG own authored presentation, layout, animation, and visual hooks; they must not become the source of truth for inventory, attributes, or damage.
- Keep UMG HUD presentation-only. Widgets may subscribe to component delegates such as `OnHealthChanged`, `OnStaminaChanged`, and `OnGoldChanged`, but must not own or mutate gameplay state.
- Reuse one explicit implementation path when replacing a prototype system. Do not retain a hidden legacy and replacement path merely as insurance.
- Do not adopt multiplayer replication, client authority, or GAS as speculative infrastructure. Each needs a concrete gameplay trigger and an approved stage plan.
- Keep the current `AEnemy` local HFSM for melee, archer, and first caster behavior. Extend its combat range, line-of-sight, retreat, and attack-delivery configuration before considering StateTree or Behavior Tree migration.

## TODO Queue

These TODOs are accepted future work, not permission to start implementation immediately. When a TODO becomes the next stage, move it out of this queue and write its complete implementation plan in `plan.md` first. On completion, record only durable facts in `ARCHITECTURE.md`; do not turn this roadmap into a stage log.

- [ ] **TODO-02: Encounter And Gate Foundation v1**
  Add `AEncounterController`, `AEncounterSpawnPoint`, `AEncounterGate`, and persistent encounter IDs. Normal patrol enemies remain placed in the level; the controller only owns sealed fights, ambushes, wave events, and Boss activation. Spawn or activate participants before closing fog gates, forbid leaving an active sealed encounter, and reopen/reset an uncleared encounter after player death. Verify single-wave, multi-wave, pre-placed activation, completion, player death, reload, and already-cleared restore paths.

- [ ] **TODO-03: Equipment And Loot Foundation v1**
  Introduce static item definitions, runtime item instances, narrow player equipment ownership, world item pickups, and authored drop tables. Begin with sword/shield ownership, a small item pool, gold, and one equipment reward. Do not add random affixes, durability, crafting, merchants, or a grid inventory.

- [ ] **TODO-04: Projectile And Ranged Combat Foundation v1**
  Add the player bow, an aim/release input path, an archer enemy, and projectiles that reuse the same team, block, damage, line-of-sight, collision, and hit-feedback rules as melee. Keep weapon switching outside active combat for v1. While locked on, horizontal mouse movement on the existing `LookAction` must switch to the nearest valid target on the requested screen side; it uses a configurable swipe threshold, re-arm threshold, and cooldown so a continuous mouse movement cannot cycle every target. If no eligible target exists on that side, preserve the current lock. Extract only the shared hit-resolution boundary required by melee and projectile delivery; retain `AMyCharacter::TryStartAction()` as the central action arbitrator.

- [ ] **TODO-05: Combat Punish And Defense Feedback v1**
  Add one player imbalance state, `EAS_GuardBroken`, without adding a separate player Poise HUD. A shield hit that reduces stamina to zero or cannot pay its guard cost must consume the remaining stamina, resolve with the shield's configured blocked damage, drop guard, and enter `EAS_GuardBroken`. A normal, unblocked hit while the player is already `EAS_Exhausted` must enter that same GuardBroken state; this rule is keyed to the action state, not the current stamina value. Spending the last stamina without receiving a hit remains ordinary `EAS_Exhausted` until recovery. GuardBroken has a dedicated montage, fixed recovery window, input lock, and state-guarded timer/Notify/delegate cleanup. Complete the existing enemy `EES_StanceBreak` loop with a front critical interaction: valid stance-broken targets can be aligned through Motion Warping and take configured critical damage from an AnimNotify. Do not add backstab, a generic finisher framework, a player Poise gauge, or Boss criticals in this stage.

- [ ] **TODO-06: Boss And Level Slice v1**
  Build the level route, two normal encounters, one elite encounter, shortcut, Boss fog gate, configured first Boss, completion reward, and completion screen. The first Boss begins as an `AEnemy` variant with an attack profile and encounter configuration; a dedicated Boss architecture is not part of this TODO.

- [ ] **TODO-07: Demo Polish And Regression v1**
  Tune combat readability, navigation, level landmarks, sound/music, VFX, UI feedback, checkpoint/reload behavior, and visible error recovery. Establish the repeatable manual PIE regression route for the complete Demo loop.

## Equipment And Loot Direction

- Use DataAssets for static authored item definitions that reference meshes, icons, equip visuals, action sets, sounds, VFX, rarity policy, and other Unreal assets.
- Use a runtime item-instance data structure for ownership, quantity, rolled affixes, durability or upgrade level if those become real gameplay. Do not encode mutable item state back into a shared definition DataAsset.
- World pickup Actors are presentation and interaction endpoints. They carry or resolve a drop result, validate collection, then transfer the item record into player-owned gameplay state. They are not the permanent inventory database.
- Keep inventory/equipment APIs narrow. Start with actual player needs such as equip, unequip, add/remove a pickup, and query an equipped item. Add save/restore only with the checkpoint and persistence stage. Avoid a generic item service or universal interface until multiple unrelated systems truly need the same operation.
- In v1, weapon and shield loadout changes happen only while resting at a checkpoint. Pickups enter ownership immediately, but equipment is not hot-swapped from the pause menu or during an encounter. Revisit broader equipment access only with a real inventory stage.
- Gold remains a scalar player attribute/reward value. It should not be forced into the item-instance model merely because both are rewards.
- Every drop source should use one authored table or explicit reward definition. Do not scatter weighted random selection through enemy, chest, and level Blueprints.

## Adoption Gates

### GAS

Recommendation: retain the current combat pipeline until a real ability/status system needs it.

Adoption conditions: evaluate GAS only when several independently authored skills, stackable buffs/debuffs, elemental status effects, reusable cooldown/cost rules, or designer-authored effect combinations cause the existing C++ and DataAsset model to duplicate gameplay-rule code. A GAS stage must define migration ownership and remove the replaced path; it must not leave two damage/effect pipelines active.

### Full Inventory, Crafting, And Merchants

Recommendation: implement equipment and drops first, then widen into a full inventory only when player choices require it.

Adoption conditions: revisit slots, sorting, storage, crafting, vendors, selling, or trading when the player can carry multiple unequipped items, stack consumables, compare equipment, retain loot across checkpoints, or spend resources in more than one system. Do not add grid inventory UI merely to display a small fixed equipment set.

### Save And Checkpoint Expansion

Recommendation: establish the one-slot checkpoint model in `TODO-01`, then keep later persistence additions versioned and narrow.

Adoption conditions: expand the initial SaveGame schema only when another persistent system has a defined reset contract, stable IDs, and a player-visible reason to survive rest/death/reload. Store stable IDs and runtime item instance data, not raw pointers to world Actors or Widget state.

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
