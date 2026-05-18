# Gemini Project Context: Test (Unreal Engine 5 Action Game)

This project is an Unreal Engine 5.7 action-combat game demo featuring a state-driven character system, melee combat, AI enemies, shield blocking, and a lightweight runtime debug overlay.

## Project Overview

- **Engine Version:** Unreal Engine 5.7 (Targeting Windows)
- **Primary Technologies:**
    - **C++:** Core gameplay systems, combat logic, AI, and UI glue code.
    - **Blueprints:** Extensions of C++ classes, animation notifies, widget presentation, and asset configuration.
    - **Enhanced Input:** Modern input handling for movement, combat, and blocking.
    - **PCG:** Arena generation support through `AArenaGenerator`.
- **Architecture:**
    - The project follows a standard Unreal Engine module structure.
    - Gameplay code is organized into `Character`, `Enemy`, `Items`, `AttributeComponent`, `HUD`, `Interfaces`, and `PCG`.
    - Content is organized under `Content/_GAME` for project-specific assets.

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` files are the primary truth source.
- When markdown documents disagree, use this precedence: **source code > AGENTS.md > CLAUDE.md > GEMINI.md**.
- `AGENTS.md` should be treated as the maintained high-priority summary.

## Agent Coordination

- `plan.md` is the shared agent-to-agent communication and handoff file.
- Before continuing prior collaboration work, read the header rules in `plan.md`.
- Agent feedback and implementation plans should be written there as working context, not end-user documentation.

### Class Hierarchy
```text
AActor
 ├── Aitem (base: parabolic spawning, floating animation, overlap events)
 │    ├── AWeapon (box-trace sweep collision, hit-stop, camera shake)
 │    ├── AShield (offhand equip, block angle/damage/stamina config, block FX)
 │    └── ATreasure (gold value, initialized from UTreasureData asset)
 ├── ABreakAbleActor + IHitInterface (static mesh -> GeometryCollection swap on hit)
 ├── AArenaGenerator (USplineComponent + UPCGComponent for PCG-based arena spawning)
 └── ABird (APawn subclass, flyable spectator)

ACharacter
 ├── AMyCharacter + IBlockableInterface (UAttributeComponent, spring arm + camera, weapon/shield equipping, hold-to-block)
 └── AEnemy + IHitInterface (AI patrol/search/chase/combat state machine, directional hit react)

APlayerController -> ACharacterController (Enhanced Input: move/look/jump/equip/attack/sprint/walk/block, plus input debug snapshot ownership for HUD overlay)
UActorComponent -> UAttributeComponent (health, stamina, gold, delegates for health/stamina/exhausted)
UWidgetComponent -> UHealthBarComponent
UUserWidget -> UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars, buffer delay logic)
           └── UPlayerHUDWidget (player HP/stamina HUD + NativePaint debug text overlay)
UAnimInstance -> USlashAnimInstance (exposes GroundSpeed, Direction, and state enums to the anim graph)
UDataAsset -> UTreasureData (static mesh, gold value, pickup sound, scale)
UInterface -> UBlockableInterface (weapon hit interception before final damage application)
```

## Key Systems

### State Machine & AnimNotifies (`CharacterTypes.h`)
All gameplay states are defined as `UENUM` enums. This is the single source of truth for state flow.

**State transition pattern:** Mixed C++ + montage delegate + AnimNotify driven.
- Entry states are set directly in C++ (for example `Attack()`, `GetHit_Implementation()`, `Die()`, `SetEnemyState()`).
- Recovery transitions commonly use `FOnMontageEnded` delegates with `bInterrupted` guards.
- Animation notifies still control collision windows and some designer-timed recoveries.
- Do not hardcode recovery transitions in `Tick()`.

- `EWeaponState`: `EWS_Unequipped`, `EWS_OneHandEquipped`, `EWS_TwoHandEquipped`
- `EActionState`: `EAS_UnOccupied`, `EAS_Attacking`, `EAS_Stunning`, `EAS_Exhausted`, `EAS_Dead`
- `EEnemyState`: `EES_UnOccupied`, `EES_Patrolling`, `EES_Searching`, `EES_Chasing`, `EES_Combating`, `EES_Attacking`, `EES_Stunned`, `EES_Dead`

### Character System (`Source/Test/Public/Character/`)
- **`AMyCharacter`**: Main player class handling movement, combat, equipment, stamina exhaustion, and shield blocking.
- **Movement Logic**: Implements direction-based speed scaling and state-based speeds:
  - Walk: `150`
  - Default: `300`
  - Sprint: `450`
- **Weapon Flow**: Equip weapon to `RightHandSocket` -> attacks are allowed while `WeaponState != EWS_Unequipped` -> attack montage -> montage-end recovery.
- **Shield Flow**: Equip shield to the offhand -> hold block input -> angle/stamina-based block resolution -> reduced movement speed while blocking.

### Enemy System (`Source/Test/Public/Enemy/`)
- **`AEnemy`**: AI combatant controlled by `AAIController` via `EEnemyState` FSM.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid targets return to `EES_Patrolling`, targets inside `CombatingRadius` switch to `EES_Combating`, and targets inside `ChasingRadius` switch to `EES_Chasing`.
- **Patrolling / Searching**: `OnPatrolling()` moves between `PatrolTargets`; once inside `PatrolRadius`, the enemy switches to `EES_Searching`. `OnSearching()` stops movement, starts `PatrolTimer` plus repeating `LookTimer`, and rotates toward `GenerateNewLookRotation()`.
- **Chasing / Combating**: `OnChasing()` reissues `MoveToTarget()` if path-following falls back to idle. `OnCombating()` rotates toward the target until `DotProduct > AttackAngleThreshold`, then attacks.
- `PatrolTimer` and `LookTimer` are cleared through `ClearPatrolTimers()` on state transitions and death.

### Combat Pipeline (`AWeapon`, `IHitInterface`, `IBlockableInterface`)
1. `Input_Attack()` -> `AMyCharacter::Attack()`
2. `PlayAttackMontage()` with `UAnimNotifyState_WeaponCollision`
3. **NotifyBegin** -> `AWeapon::StartWeaponTrace()` (records old box positions)
4. **NotifyTick** -> `AWeapon::ExecuteWeaponTrace()` (sweeps from old -> new center to prevent ghost swings)
5. On hit:
   - optional `IBlockableInterface::TryBlockHit()` interception
   - `ApplyDamage()`
   - optional `IHitInterface::GetHit()`
   - hit-stop + camera shake
6. **NotifyEnd** -> clears `IgnoreActors`
7. Montage-end delegates recover state when not interrupted

### Block System
- `AMyCharacter` implements `IBlockableInterface`.
- `ACharacterController` binds `BlockAction` start/end to `StartBlockInput()` / `ReleaseBlockInput()`.
- `AShield` provides block tuning values:
  - `BlockHalfAngleDegrees`
  - `BlockedDamageMultiplier`
  - `BlockStaminaCostPerDamage`
  - `BlockMoveSpeedMultiplier`
- `AWeapon::ExecuteWeaponTrace()` checks `IBlockableInterface` before final damage application.
- Successful blocks reduce or redirect damage through `FBlockResult`, suppress normal hit react, and play shield-specific sound/particle feedback.
- Blocking is canceled when conditions become invalid (state, falling, or shield missing).

### Stamina & Exhaustion System
- `UAttributeComponent` manages stamina: `UseStamina()`, `AddStamina()`, `ResetStaminaRegenCooldown()`, `PauseStaminaRegen()`, and `ResumeStaminaRegen()`.
- Stamina can temporarily go negative to allow a "last action" before exhaustion.
- When stamina hits 0, `OnExhausted` broadcasts -> `HandleExhausted()` sets `EAS_Exhausted` and starts the recovery timer.
- During exhaustion, the player is restricted until `RecoverFromExhaustion()` resets the guard and restores a small amount of stamina.

### Attribute & UI System (`Source/Test/Public/AttributeComponent/` & `HUD/`)
- **`UAttributeComponent`**: Centralized health, stamina, and gold management. Broadcasts `OnHealthChanged`, `OnStaminaChanged`, and `OnExhausted`.
- **HUD**:
  - `UHealthBarComponent` delegates to `UBaseHealthBarWidget`
  - `UPlayerHUDWidget` owns player HP/stamina bars and also paints runtime debug text
- **Buffer Logic**: `PB_Health` updates instantly; `PB_Buffer` starts a `BufferDelayTime` delay before lerping toward `PB_Health` at `BufferInterpSpeed`.
- Enemy health bar visibility is timer-driven through `ShowHealthBar()` / `HideHealthBar()`.

### Debug Overlay
- Runtime debug text is gathered through `FDebugDrawHelper`.
- `UPlayerHUDWidget::NativePaint()` renders debug lines above the standard HUD.
- Debug entries are frame-scoped:
  - `Add()` resets the cache when frame number changes
  - `GetEntries()` returns an empty array when no entries were submitted this frame
- `ACharacterController` owns the player input debug snapshot through sampled move input, held-state flags, and short-lived action markers exposed by `GetDebugInputText()`.
- Console variables:
  - `test.Debug.Enable` -> master text/shape toggle
  - `test.Debug.Enemy` -> enemy text toggle
  - `test.Debug.Shapes` -> world debug sphere toggle
- Current player debug includes input snapshot text, HP, stamina, action state, montage, and movement speed.
- Current enemy debug includes enemy state / speed text plus optional distance text and chase/combat radius spheres.

### Breakable System (`Source/Test/Public/BreakAble/`)
- **`ABreakAbleActor`**: Environment interaction. Uses Geometry Collections for destruction and spawns treasures on hit.

## Building and Running

### Development Workflow
1. **Generate Project Files:** Right-click `Test.uproject` and select "Generate Visual Studio project files".
2. **Build:** Open `Test.sln` and build the `TestEditor` target in `Development Editor` configuration.
3. **Launch:** Open `Test.uproject` directly.

No automated tests or CI pipeline are configured in this repository.

## Development Conventions

- **Source Code:**
    - Headers in `Public/`, implementations in `Private/`.
    - Strict `UPROPERTY` / `UFUNCTION` / `GENERATED_BODY()` usage for reflection.
    - **Header Layout:** In the same access specifier block, functions usually appear above variables.
    - **Encapsulation:** Getter/setter helpers are commonly placed in the bottom-most `public:` block.
- **Content Organization:**
    - C++ source under `Source/Test/`.
    - Game assets under `Content/_GAME/` - **this is the only Content directory that should be modified.**
    - Marketplace and large reference assets should be treated as read-only.

## UE5 C++ & Anti-Hallucination Rules

1. **Reflection System:** Always include `GENERATED_BODY()`.
2. **Memory Management:** Follow the surrounding file style. This codebase mostly uses reflected raw pointers, with `TObjectPtr` only in a few places.
3. **Safety:** Use `check()` for mandatory invariants and `if` / `ensure` for dynamic references when appropriate.

---

## User Profile & Preferences

- UE 5.7 + C++ developer
- Prefers concise, accurate, source-grounded answers
- Chinese is acceptable for general communication; code and technical terms remain in English
