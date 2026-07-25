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
- Keep `AEnemy` as the shared local HFSM for common melee and projectile delivery. Use `ARangedEnemy : AEnemy` only for enemies that share the proven pure-ranged spacing, Escape and pre-release LOS-cancellation contract; do not migrate to StateTree or Behavior Tree before the local boundaries become demonstrably unreadable.

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

- [x] **TODO-04B-A2: Ammo Containers And Bonfire Refill v1**
  Delivered persistent loaded-ammo containers plus stable reserve stacks for ammo Definitions. `DA_Item_DarkKnightArrow` now authors `20` loaded capacity and `99` per-reserve-stack; quantity grants add reserve only, shots consume loaded arrows only, and an actual bonfire Rest atomically writes checkpoint progress plus the validated reserve-to-loaded transfer before reload. User compilation and PIE validation covered `0/20 + 25 -> 20/20 + 5`, single-arrow consumption and refill, multi-stack `99 + 21`, failed loaded-consume and refill writes, malformed raw-record fixture isolation, death/Continue/new PIE non-refill, and matching runtime/saved dumps. Normal and adversarial review found no A2 blocker; the future non-ammo generic quantity API risk remains recorded below. Completed in this commit.

- [x] **TODO-04B-B0: Prepared Projectile Commit And Refund Atomicity v1**
  Replaced Bow's post-consume activation/refund branch with a prepare-then-commit contract: all fallible Bow, camera, deferred-spawn, `FinishSpawning`, `BeginPlay`, and native readiness checks complete before the one loaded-arrow write; successful consumption immediately performs a native no-return Commit. A Pawn-local non-Shipping prepare-failure injection proves that candidate rejection leaves Runtime/Saved ammo unchanged and does not enter sound or cooldown. User compilation and PIE validation covered normal consumption, prepared rejection, consume-write failure, death/reload cleanup, wall blocking, `IHitInterface` enemy hit, and empty-space expiry; normal and adversarial review found no B0 blocker. Completed in this commit.

- [x] **TODO-04B-B1: Aim Reticle v1**
  Delivered an asset-free, viewport-centered hollow-cross reticle through `UPlayerHUDWidget::NativePaint()`. `AMyCharacter` pushes only the effective `IsBowAiming()` visibility after aim entry, unified cancellation and HUD creation; the HUD is paint-only, remains visible across a successful or empty release while aim persists, and paints above existing UMG, damage vignette and player Debug text. Default Bow aim now uses `WalkSpeed * AimMoveSpeedMultiplier` with multiplier `1.0`, preserving existing lock-on direction multipliers while avoiding sprint speed and sprint-stamina debit. User manually compiled and PIE-validated reticle visibility/cancellation, release semantics, HUD overlays, viewport centering, and `200 cm/s` aim speed; normal and adversarial review found no B1 blocker. Completed in this commit.

- [x] **TODO-04B-B2: Bow Two-Hand Occupancy And Attachment Contract v1**
  Delivered the runtime-only left-hand Bow / right-hand draw contract without changing persistent OffHand selection: `AWeapon` provides the default player attachment socket, `ABow` uses `LeftHandSocket`, and a live Bow suppresses only the transient shield Actor while preserving the selected shield `InstanceId` for restoration after a non-Bow MainHand is selected. Bow sets passive `EWS_TwoHandEquipped` animation input and independently rejects Block, Parry and late Parry activation so no stale shield state can bypass the two-hand rule. `DA_Item_DarkKnightBow` is intentionally included because its `Item_DarkKnightBow` MainHand Definition must resolve to native `ABow` for catalog registration and runtime materialization. User manually compiled and PIE-validated Bow/Shield suppression, defensive gates, restoration and existing Bow firing behavior; normal and adversarial review found no B2 blocker. Completed in commit `506f9da`.

- [x] **TODO-04B-B2a: Shared Bow Identity And Attachment Defaults v1**
  Delivered the common physical Bow hierarchy `AWeapon -> ABowBase -> ABow`, where abstract `ABowBase` owns only the `LeftHandSocket` default. `AWeapon::DefaultEquipSocketName` replaces the player-only field through a Core Redirect: ordinary weapons retain `RightHandSocket`, while both player MainHand materialization and `AEnemy::WeaponInit()` read the weapon's default directly. The legacy `AEnemy::WeaponAttachSocketName` override was removed rather than redirected across classes; affected enemy Blueprints were recompiled/saved so no character-level Socket path remains. `BP_ErikaBow` now derives from `ABowBase`, preserving its visual authorship while Erika's attack DataAsset/HFSM and player `ABow` ammo/aim/Prepared-Commit behavior remain separate. User manually compiled and PIE-validated Erika, Paladin and existing player Bow behavior; live CDO readback confirmed the removed enemy property, Erika's left default and Paladin's right default. Normal and adversarial review found no B2a blocker. Completed in this commit.

- [x] **TODO-04B-B: Bow Presentation And World Pickup v1**
  Delivered `BP_DarkKnightBow` visual presentation, collision-free nocked-arrow components, authored Bow locomotion/Aim locomotion plus Draw/Release assets, and a `Loaded / Capacity` HUD readout driven one-way from `AMyCharacter`. `AAmmoPickup` adds a persistent 20-arrow TestMap bundle whose Reserve grant and `ClaimedRewardIds` update are one durable transaction; real Rest remains the only Reserve-to-Loaded transfer. Player firing now requires a successfully startable Release Montage: `Prepare -> Release Montage -> Consume -> Commit`, with Montage physical lifetime as the sole re-fire gate and no numeric ShotCooldown. Missing/playing/failed Release presentation, prepared-projectile failure and loaded-ammo save failure cannot launch, consume or sound; consume failure blends out only the new Release, restores the nocked arrow and keeps aim active. User manually compiled, PIE-validated pickups, persistence, Rest refill, HUD, bow presentation, interruption cleanup, P2 rapid re-aim and both debug-failure paths; normal and adversarial review found no B-B blocker. Completed in this commit.

- [x] **TODO-04B-B3: Player Bow Aim Orientation And Grip Presentation v1**
  Delivered DarkKnight's shared left-palm Bow grip and a dedicated left-hand shield-back socket, with Bow mesh, nocked-arrow visual and projectile spawn point aligned from the Bow actor. Valid `EAS_Aiming` now makes the character follow reticle yaw in both free and locked play, while locked Aim retains target/marker/validity/screen-switch behavior but temporarily frees the camera and body from Lock-On retargeting. One absolute right-shoulder SpringArm target avoids Lock-On offset stacking; the authored upper-body Aim Offset reads native Bow Aim yaw/pitch over Bow locomotion. The post-review fixes stop stale failed-lock camera recentering at Aim entry and preserve a deliberately aim-away lock target across pause/resume. User manually compiled and PIE-validated free and locked Aim, target recovery, pause, sprint tuning, shoulder camera, Socket presentation, normal Bow release and interruption regressions. Normal and adversarial review found no B3 blocker. Completed in this commit.

- [x] **TODO-04B-C0: Skeletal Bow String And Nocked Arrow Socket Presentation v1**
  Delivered the player-only Skeletal Bow render contract without changing Bow gameplay delivery. `ABow` now retains its inherited static `Mesh` as a non-rendering attachment/trace compatibility anchor and adds collision-free `BowSkeletalVisual`; `SK_Bow` provides a Mesh-only `BowArrowSocket` on `Bow_Arrow_Slot`, so the nocked-arrow anchor, visual and projectile origin follow the animated string together. Transient `Relaxed / Aiming / Releasing` presentation state is written one-way by existing `AMyCharacter` aim, release, rollback, Montage-end and cancel paths; `UBowAnimInstance` and `ABP_DarkKnightBow` only consume it for the Bow's visual state machine. The C0 visual layer owns no input, ammo, prepare/consume/commit transaction, projectile collision, damage or enemy Bow behavior. `BP_DarkKnightBow` moved to `/Game/_GAME/BP/Items/Weapons/Bow/`; Redirector fix-up updated the Bow Definition and actual TestMap Bow Pickup to direct references. User manually compiled and PIE-validated string/arrow/spawn tracking, normal release and rollback, cancellation/interruption/death/EndPlay cleanup, sword-and-shield and enemy regressions; normal and adversarial review found no C0 blocker. Completed in this commit.

- [x] **TODO-04B-C: Player Bow Charge, Nocked Projectile Handoff And Confirmed Hit Feedback v1**
  Delivered player-only RMB Aim plus LMB full-draw firing: press creates exactly one collision-disabled prepared player arrow, nocks its native root to `BowArrowSocket`, and Draw Montage natural end is the only full-draw gate. Early release and every interrupt destroy the uncommitted candidate without ammo loss; full-draw release refreshes its current reticle launch context, then preserves `Release Montage -> durable Loaded Ammo consume -> CommitPreparedLaunch` on that same Actor. Release and the new Load Montage physically gate re-fire without a numeric cooldown; Load only performs the held-RMB return-to-Aim presentation. Charge contracts FOV and reticle through presentation-only HUD input, and a player-arrow resolved hostile hit or shield block emits one short HUD marker while world, same-team, Dormant and parried outcomes do not. Player and enemy arrow presentation/configuration remain separate. User manually compiled and PIE-validated charge cancel, full release, debug prepare/save failures, Release/Load gating, presentation cleanup, hit marker and sword/shield/enemy regressions; post-fix normal and adversarial review found no B-C blocker. Completed in this commit.

- [x] **TODO-04B-C1: Bow Reload Input Buffer And Re-fire Reticle v1**
  Delivered a Bow-local, one-intent Reload handoff without reusing melee Combo state: while the physical Release/Load Montage gate is active, held LMB records only a raw input and one pending Draw intent. Natural Load completion validates the current Bow, Aim, held RMB/LMB, life and Loaded ammo, then starts the existing Draw path exactly once; no candidate arrow, ammo mutation or automatic release occurs during the gate. Cancellation, abort, interruption, hit, Guard Break, death, equipment change, Bonfire and EndPlay clear the state idempotently. `UPlayerHUDWidget` receives the real gate result one-way and paints only the existing reticle inner lines red until re-fire is legal. User manually compiled and PIE-validated repeated buffered firing, released input, red-reticle cleanup and regressions; normal and adversarial review found no C1 blocker. Completed in this commit.

- [x] **TODO-04B-D: Shared Bow Runtime And Bow Profiles v1**
  Promoted `ABowBase` into the shared physical Bow runtime: collision-free Skeletal visual, fixed `BowArrowSocket`, presentation-state bridge, prepared-projectile nocking and the sole launch-transform query. The inherited static Mesh remains an attachment/BoxTrace compatibility anchor and is hidden whenever a valid Profile renders the Skeletal Bow. `ABow` keeps only player ammo, projectile, Montage, sound and loaded-arrow behavior; `AMyCharacter` still owns input, camera trace, HUD, charge and the existing `Prepare -> Consume -> Commit` transaction. Player candidate nocking, committed launch and equipped enemy Bow LOS/release now derive from the same Socket; invalid Bow Profile/Socket rejects that Bow path without a character-Mesh fallback, while non-Bow enemy projectile attacks retain their existing fallback. `UBowPhysicalProfileDataAsset` is shared as a type and Socket contract, with intentional per-Bow assets: DarkKnight uses `DA_BowPhysical_Archer` (`SK_Bow` / `ABP_DarkKnightBow`) and Erika uses `DA_BowPhysical_Erika` (`SKM_ErikaBow` / `ABP_ErikaBow`) for her distinct scale, grip and always-held locomotion presentation. User manually compiled and PIE-validated player nock-to-flight, rollback/gate regressions, Erika attachment/LOS/release, and melee regressions; normal and adversarial review found no runtime blocker. Completed in this commit.

- [x] **TODO-04E: Third-Person Camera Experience Polish v1**
  Delivered one state-aware third-person camera path without cinematic lag systems: `GetCameraTargets()` remains the sole target resolver with `Bow Aim > Lock-On Free-Run / Lock-On > Default` precedence, and Lock-On validity/retargeting completes before that target is resolved. Nonzero raw Look input immediately cancels failed-lock recentering; Lock-On, Bow Aim, Bonfire, hard interrupts and teardown clear stale recenter/recovery state. The same native `SpringArm` now uses `UObstructionRecoverySpringArmComponent` to consume its existing Sphere Sweep inside `BlendLocations(...)`: wall hits remain immediate, while a full-path clear recovers from a current safe arm fraction without shortening `TargetArmLength` or creating a second trace. The player debug panel emits camera evidence only during physical obstruction or active recovery. User manually compiled and PIE-validated direct walls, corners, narrow passages, free/locked Bow Aim, target switching, action interruptions, death/Rest/EndPlay, and 30/60 FPS behavior; normal and adversarial review found no remaining 04E blocker. Completed in this commit.

- [x] **TODO-05C0: Player Action Intent Arbitration v1**
  Replaced player action numeric Priority and special-case cancellation selection with one private `AMyCharacter` intent resolver. Each Attack/Block/Dodge/Parry/Potion input now resolves exactly once to `StartNow`, `BufferOnce`, `Reject`, or `EndHeld` before `TryStartAction()` executes an already-resolved target. Attack Startup/Active retains only its Combo Window buffer; authored CancelWindow can start a preflight-valid Potion/Block/Dodge/Parry; Block and Bow Aim remain reversible held states; Dodge, Parry, successful Potion, and Bow Draw/Release/Load remain committed or locally owned. Target Montage/resource/config checks occur before cleanup, Combo and Bow retain separate buffer owners, and normal/sprint/charged/continuation attack Montage end callbacks use a current Montage plus playback-token guard. The C++ `UPlayerActionConfigDataAsset` contract no longer exposes Priority-only fields/API; the existing Potion WIP asset remains deliberately unsaved. A strict review found and fixed the Block + LMB gap before release; user then recompiled and PIE-validated the repair, and main plus fresh delta-only adversarial review found no remaining blocker. Completed in this commit.

- [x] **TODO-05C: Player Combo Per-Entry Montage Handoff v1**
  Replaced the shared `ComboMontage + SectionName` contract with a linear `ComboChain` whose each entry owns `Montage + EntrySection` plus its existing damage, stamina, poise and Motion Warping data. The three project-owned DarkKnight Combo Montages use `Entry -> Recovery -> End`; `ComboWindow` remains the only one-input LMB buffer, while `ComboBranchWindow` consumes early input or accepts late LMB in the authored post-strike pause. Same-Montage continuation rebinds its current token and jumps to `Entry`; cross-Montage continuation marks the old Montage/token as a planned handoff, establishes a target token/delegate, then commits the target segment only after target playback starts. Action CancelWindow overlaps the BranchWindow/Recovery and still starts Dodge/Parry/Block/Potion only through the C0 resolver and successful preflight. User manually compiled and PIE-validated single-segment recovery, `01 -> 02 -> 03` handoff, collision/notify behavior, cancellation and existing combat regressions. Main and fresh adversarial review found no blocking defect; two unforced Montage-boundary P2 checks are recorded below. Completed in this commit.

- [x] **TODO-04D-A: Enemy Ranged Delivery And Local HFSM v1**
  Delivered one authored Projectile attack path inside the existing `AEnemy` local HFSM, without introducing a ranged C++ base class or moving Paladin behavior. `FEnemyAttackEntry` now distinguishes `Melee` from validated `Projectile` entries; the latter use an immutable attack snapshot, effective AI range, `ECC_Visibility` LOS, retreat/diagonal-retreat when too close, LOS repositioning, and a state-guarded one-shot `UAnimNotify_EnemyProjectileRelease` route into the existing `ACombatProjectile`. Dynamic `AAIController` possession now refreshes movement-completion bindings, and all projectile timers/snapshots clear on interruption, dormancy, death and `EndPlay`. The non-Shipping `EnemyRangedDebugProbe` validates this core without assets or a map fixture. User compilation and PIE validation covered open-space delivery, shield block, wall-first LOS rejection/reposition, minimum-range retreat, delayed interruption/PIE cleanup, Paladin regression, and 04A projectile regression; normal and adversarial review found no D-A blocker. Formal Erika assets, Montage timing, sockets and TestMap placement remain D-B.

- [x] **TODO-04D-B0: Shared Character Animation Data v1**
  Delivered Skeleton-independent `UBaseCharacterAnimInstance` for root AnimBPs owned by `ABaseCharacter`. It reads authoritative `GroundSpeed`/`Direction` plus CharacterMovement falling and vertical-speed data; `USlashAnimInstance` now contains only player weapon/block/stun state. `ABP_Paladin` and `ABP_ErikaArcher` use the native base, while DarkKnight's linked MainState/IK graphs keep their explicit Exposable Properties inputs. No enemy state, death, hit, AI, or Skeleton-specific asset state was duplicated. User compilation and PIE validation covered player movement/jump, sword/shield combat and hit/death, Paladin locomotion/hit/death, and Erika locomotion preview. Normal and adversarial review found no B0 blocker.

- [x] **TODO-04D-B: Erika Archer Authoring And TestMap v1**
  Delivered the first authored archer on `SK_ErikaArcher`: dedicated locomotion, a one-shot `Draw -> AimHold -> Release` Montage, state-guarded projectile Release, hit/death presentation, left-hand bow attachment, release socket, visible arrow presentation, and one deliberate `TestMap` placement. `AEnemy` remains the sole enemy base: the Projectile entry's immutable snapshot now includes a shared LOS/launch chest-height target offset and permits montage-period facing only for active Projectile attacks, while Melee/Paladin behavior remains unchanged. Reciprocal Owner/Instigator movement ignores prevent a firing actor from blocking itself on its own arrow without relaxing normal projectile collisions. User compilation and PIE validation covered visual attachment/release, range/LOS/retreat, shield blocking, interruption/death cleanup, self-arrow movement, chest aiming, and Paladin regression; normal and adversarial review found no D-B blocker.

- [x] **TODO-04D-C: Archer Spacing And Escape v1**
  Delivered the first pure-Projectile `RangedEscape` tactical state, initially inside the then-current `AEnemy` local HFSM. Erika now uses a continuous authored distance contract with Escape, Retreat, BackDiag, safe Strafe/fire and Press bands; a fixed navigation leg plus dedicated request IDs prevents player pursuit from continuously cancelling and reissuing the Escape path, while real navigation failure falls back to BackDiag. Combat arbitration is explicit: committed attack, then Escape, then pending AttackIntent, then ordinary MovementIntent. Projectile attack start keeps its tactical range/LOS gate; a legally committed Release instead uses physical flight reach plus current state and LOS guards, preventing a short retreat from producing an empty release. User compilation and PIE validation covered close-range escape, exit hysteresis, fixed-leg navigation, fallback behavior, cooldown/Pending preemption, wall-first LOS, committed release after a target retreats, lifecycle cleanup and Paladin regressions. Normal and adversarial review found no D-C blocker. D-D later migrated the runtime state into `ARangedEnemy` and resolved the deferred pre-release LOS-cancel presentation debt.

- [x] **TODO-04D-D: Ranged Enemy Specialization And Attack Cancellation v1**
  Delivered `ARangedEnemy : AEnemy` as the narrow pure-ranged specialization. `AEnemy` remains the shared owner of perception, target validity, hit/death and encounter lifecycle, common navigation, attack-entry execution, Projectile delivery and the debug probe; Paladin keeps the `200 / 290 / 330 cm/s` base defaults while Erika receives `220 / 300 / 330` plus the migrated Escape, safe-distance and range defaults from the ranged CDO. During a Draw/AimHold Projectile attack, Erika now aborts only an unreleased attack after continuous LOS loss for `0.15 s`: it closes the one-shot Release guard before a `0.12 s` Montage blend-out, then recovers through the existing idempotent cooldown path. Released arrows, melee attacks, hit reactions, death and the debug probe remain unaffected. User manually compiled and PIE-validated speed roles, Escape, LOS cancellation, late Notify rejection, lifecycle cleanup, projectile/Paladin regressions, and completed normal plus adversarial review with no D-D blocker. `BP_Paladin` corrects the actual asset name while `BP_Pladin` remains an intentional redirector for existing map and Encounter references; no Fix Up Redirectors or Encounter WIP mutation was performed. Completed in this commit.

- [x] **TODO-04C: Scroll-Wheel Lock Target Switching v1**
  Delivered a dedicated `IA_LockTargetSwitch` Axis1D mapping on `MouseWheelAxis`: wheel down changes to the nearest valid target on the current target's screen-right side and wheel up selects the screen-left side. `UPlayerLockOnComponent` filters candidates by life, `LockOnRadius` and player viewport projection; no same-side candidate preserves the current target without wrapping or unlocking. `ACharacterController` owns axis threshold/re-arm/cooldown and pause/bonfire/focus-loss gates, while `AMyCharacter` performs marker-only handoff without recaching rotation state. A dead locked enemy now retargets through the existing front-facing `FindBestTarget()` selection or unlocks when none exists; out-of-range, invalid-object, player-death and player-stun paths preserve normal unlock behavior. User manually compiled and PIE-validated scroll direction, no-candidate behavior, input throttling, combat-state switching, ordinary death retarget, standard lock-on and enemy regressions. The hard-stun death branch was statically reviewed after its narrow gate fix and accepted by the user because the current Demo has no practical fixture to kill the locked target during player stun. Normal and adversarial review found no remaining 04C blocker. Completed in this commit.

- [x] **TODO-05A: Player Guard Break v1**
  Delivered `EAS_GuardBroken` as a dedicated player-only hard-stun state. A valid depleted block still resolves shield damage reduction and consumes the remaining stamina before entering Guard Break; an unblocked hit while already Exhausted keeps full damage and enters the same state. `AM_GuardBreak_DKM` recovers through a state-guarded Montage End Delegate, clearing the exhaustion gate and restoring at least one stamina without replaying or extending on later nonlethal hits. Successful jumps and positive-cost blocks now refresh stamina regeneration delay, and Exhausted recovery is `3 s` rather than `5 s`. The serialized enum compatibility review found and fixed the only blocker by appending the new state instead of inserting it. User manually compiled and PIE-validated the completed behavior; normal and adversarial review found no remaining 05A blocker. Completed in this commit.

- [x] **TODO-05A1: Combat-Aware Sprint Stamina v1**
  Delivered runtime-only Combat Presence for `AMyCharacter`. It refreshes while an existing active enemy-engagement query identifies the player as a current combat target, or after a confirmed hostile `AMyCharacter <-> AEnemy` resolver hit; a `4 s` timestamp tail prevents immediate stamina-rule flicker when the final source clears. `TickSprintStamina()` is the sole consumer: Shift preserves its existing speed, Free-Run, input and hearing-noise behavior everywhere, but only active Presence permits the pre-existing `12/s` debit and stamina-recovery-delay reset. Presence is not saved, clears on death, bonfire service protection and `EndPlay`, and its tail is deliberately excluded from the checkpoint gate. Normal and adversarial review found no remaining 05A1 blocker. Completed in this commit.

- [x] **TODO-05A2: Enemy Stance-Break Montage v1**
  Replaced the legacy Timer-driven slow-motion reuse of enemy directional `HitReact` with a dedicated enemy-only `StanceBreak.Montage` in `UHitReactionConfigDataAsset`. `AEnemy` now commits `EES_StanceBreak` only after the dedicated Montage plays successfully, and a state/life/Dormant-guarded Montage End Delegate recovers through `CheckCombatTarget()`. Missing Reaction DataAsset, Montage or AnimInstance warns, clears pending poise and resets poise without faking StanceBreak through ordinary HitReact. Subsequent nonlethal hits retain damage, knockback and impact feedback but cannot enter `EES_Stunned`, replay or extend the dedicated Montage; repeated poise depletion/parry, death, Dormant and teardown retain the same cleanup boundary. Paladin and Erika each received a dedicated authored Montage and Reaction DataAsset assignment. User manually compiled and PIE-validated the full behavior; normal and adversarial review found no remaining 05A2 blocker. Completed in this commit.

## TODO Queue

These TODOs are accepted future work, not permission to start implementation immediately. A queued item must be small enough to produce one independently verifiable result. Queue position follows prerequisites, validation dependencies, and the player-facing loop rather than numeric ID or append order. When an item becomes the next stage, move only that item out of this queue and write its complete implementation plan in `plan.md` first. On completion, record stable facts in `ARCHITECTURE.md` and move the compact result into `Done Milestones`; do not turn this roadmap into a stage log.

**Encounter authoring decision:** reusable Controller behavior and Spline authoring were already proven by `TODO-02A1/A2`. The former standalone `TODO-02B` is deliberately not marked Done and is absorbed as a mandatory adoption condition of `TODO-06A`: do not place a permanent `TestMap` encounter solely to repeat core validation. `TODO-02C` waits until that first permanent map-owned Controller exists.

### Combat Punish And Criticals

- [ ] **TODO-05B: Front Critical v1**
  Prerequisite: `TODO-05A2` has authored and validated the dedicated enemy Stance-Break presentation and protected recovery window, and a DarkKnight-compatible front-critical Montage plus its intended AnimNotify timing are available. Complete the existing enemy `EES_StanceBreak` loop with one front critical interaction. Align a valid stance-broken enemy via Motion Warping and apply configured critical damage from an AnimNotify. Do not add backstab, generic finisher infrastructure, or Boss criticals. Deferred behind the player Bow presentation route until those dedicated Critical assets are ready.

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
  Add one Boss fog-boundary presentation, configured first Boss as an `AEnemy` variant, fixed Gold plus Remnant/Emblem reward, and Boss completion persistence. The first Boss must escalate through `Phase 1 -> Transition -> Phase 2` and include at least one authored complete attack-chain pattern: AI selects that pattern once rather than re-rolling every hit, so a shared prefix may branch as `M1 -> M2 -> M3 -> M4` or `M1 -> M2 -> M3 -> M5`. The later 06B plan must choose from the real animation assets whether those nodes use explicit separate-Montage handoffs or Sections inside one Pattern Montage, while preserving pose continuity and interrupt cleanup. Do not introduce a Boss-specific C++ base unless the existing enemy and encounter boundaries demonstrably fail.

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

- **Generic non-ammo quantity consumption still targets raw SaveGame order.** A2 moves arrows onto validated reserve selections and loaded-container consumption, so the prior arrow path is resolved; current CodeGraph evidence finds no runtime caller of `TryConsumeDefinitionQuantity()`. A future non-ammo stackable consumable using that old API could consume a malformed raw record before the valid cached record, making an item appear spent until reload restores it; removal before a reserve stack can also leave the component's source-index cache stale until restore, causing a later refill to be rejected. **Resolution condition:** before the first non-ammo stackable consumable, drop, or backpack route calls `TryConsumeDefinitionQuantity()`, replace that DefinitionId scan with Pawn-validated instance selections and rebuild the local cache after a successful write; add a malformed-record and shifted-index fixture in that owning stage.

- **TODO-05C Montage-boundary handoff has not been forced through its two narrow adversarial timings.** User compilation and normal PIE cover the authored three-Montage route, one-shot buffering, recovery, cancellation and interruption. Static review identified two unproven boundaries: a same-Montage continuation deliberately retains its playback token, so an already-queued old end delegate near the BranchWindow/end edge could theoretically look current; and a target Montage could theoretically end immediately after target startup but before the handoff call returns, leaving target combat values or `ComboCounter` committed. **Player impact if real:** a just-started continuation could reset unexpectedly, or an instantly interrupted target could consume stamina/advance the chain without a sustained attack. **Resolution condition and owner:** before a future player-melee stage adds a reused Montage entry, a fourth ComboChain entry, or branching continuation, force both timings with a temporary test Montage/debug route and prove no recovery, stamina, Motion Warping or ComboCounter leak; otherwise fix the affected `AMyCharacter` handoff guard in that stage. This is not a current blocker.

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

Recommendation: retain `AEnemy` as the shared enemy core and use the completed `ARangedEnemy : AEnemy` only for a genuinely shared ranged tactical contract. Do not extract `ABaseEnemy` or add an `AMeleeEnemy` merely to make the hierarchy symmetric: the existing `AEnemy` already is the common runtime base, and Paladin currently has no melee-only behavior to share with a second concrete subclass. Attack entries remain DataAsset-authored; an archetype class owns behavior policy and safe default values, not an alternate attack database.

Adoption conditions: introduce a separate enemy-profile DataAsset when three or more variants repeat the same sensing, spacing, speed, stat and reward setup across otherwise different Blueprint/C++ descendants. Add a caster child of `ARangedEnemy` only when it inherits the same LOS, spacing, escape and interruption contract but needs new runtime mechanics; a different mesh, Montage or attack DataAsset remains a Blueprint/data change. Consider StateTree or Behavior Tree only when the local HFSM becomes unreadable across multiple enemy families, repeated tactical branches dominate the shared core, or future enemies need multi-step plans that cannot remain clear in the existing flow. Introduce a Boss-specific class or component only when a Boss needs persistent phase state, encounter-owned UI/objectives, arena rules, or lifecycle logic outside normal enemies.

### Combat Intent Arbitration

Recommendation: keep the current local hard-priority order: committed action state, then `ARangedEnemy` safety Escape when applicable, then legal attack intent, then ordinary movement intent. `PendingAttack` owns a selected attack and its entry-specific positioning; `FEnemyCombatMovePlan` owns generic Retreat, BackDiag, Strafe, and Press. Do not score Escape against damage opportunities.

Adoption conditions: introduce a focused selector or Utility scoring stage only when an enemy has three or more simultaneous, equally legal tactical choices such as cover, flank, multiple attack families, or several escape routes, and `OnCombating()` can no longer express their priority and tie-breaks clearly. Keep death, hit stun, committed montage safety, and emergency escape as hard guards above any score; score only peer choices after those guards pass.

### Enemy Vertical Aim Offset

Recommendation: retain the D-B authored one-shot `Draw -> AimHold -> Release` Montage as the initial archer aim presentation; do not add `Aim Offset 1D` merely because a bow exists. The current projectile still uses its authoritative Socket-to-target launch direction even without a matching upper-body pitch pose.

Adoption conditions: add a focused Mesh Space additive `Aim Offset 1D` stage only after D-B proves a visible vertical-aim mismatch on stairs, slopes, elevated targets, or a long aim hold. That stage must use a real held-bow Center pose rather than the Skeleton reference/idle pose, add authored Up/Center/Down poses, derive one transient `AimPitch` snapshot from the same launch direction used by `AEnemy`, and apply it only to the archer's ranged upper body. Keep `UBaseCharacterAnimInstance` generic; do not duplicate target, AI, hit, or death state into the shared base.

### Combo Montage Granularity

Recommendation: retain one project-owned Montage per linear ComboChain entry. Each entry owns `Entry -> Recovery -> End` and its local collision, sound, Motion Warping, ComboWindow, ComboBranchWindow, CancelWindow and recovery timing; `UComboDataAsset` owns only the entry order and combat values. Do not split an entry into disconnected wind-up/strike/recovery asset fragments or restore a shared timeline solely for convenience.

Adoption conditions: consider a branch graph or broader combo framework only when the player needs two or more mutually exclusive legal continuations from the same authored branch point, or when future weapons need a shared selector beyond this linear chain. That stage must retain one-input LMB ownership, explicit planned cross-Montage handoff, current playback identity guards and real interruption cleanup. It must not reinterpret Dodge/Parry/Block/Potion as global pre-input buffers.

### PCG

Recommendation: keep current levels hand-authored until production pressure is visible.

Adoption conditions: evaluate PCG when multiple levels repeat the same dressing/encounter work, themes need reusable generation rules, or manual placement becomes the bottleneck. Keep gameplay-critical objectives, pickups, checkpoint placement, and encounter authority explicit even if environmental dressing becomes procedural.

### Staff, Magic, And Focus Resource

Recommendation: ship the bow as the first non-melee weapon family before adding a staff or spell system.

Adoption conditions: evaluate staff/catalyst equipment only after bow projectiles, ranged input, equipment ownership, and save/load all work in the complete Demo loop. A staff stage must decide the Focus/Mana resource, spell-slot policy, cast interruption rules, elemental effects, and whether spells use projectiles or area effects. Do not use stamina as a temporary magic resource merely to avoid making that decision.

### Caster Enemy

Recommendation: add a caster only after `TODO-04` validates projectile delivery with the player bow and archer.

Adoption conditions: introduce the first caster when the Demo needs a third combat pressure pattern beyond melee pursuit and direct ranged fire. It may inherit `ARangedEnemy` only when it keeps the same pure-ranged LOS, spacing, Escape and pre-release cancellation contract; otherwise begin from `AEnemy` and add a dedicated class only for real new runtime policy. Give it a readable cast wind-up and one distinct behavior such as a slow projectile, tracking projectile, or marked area effect. Do not add a separate AI framework, player Focus resource, or elemental status system merely because an enemy casts magic.

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
