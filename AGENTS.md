# AGENTS.md

This file provides guidance to coding agents working in this repository.

## Project Overview

- **UE 5.7** project, **Windows only**, **Visual Studio 2022** required.
- Modules: `Test` (Runtime), `SmartBPCreator` (Editor plugin).
- Targets: `TestEditor` (Editor), `Test` (Game).
- No automated tests exist in this project.

## Build & Editor

```bash
# Generate VS project files
# Right-click Test.uproject → "Generate Visual Studio project files" → open Test.sln

# Compile via UBT (Development Editor)
UnrealBuildTool TestEditor Win64 Development Test.uproject

# Compile via IDE
# Open Test.sln in VS 2022, build TestEditor target (Development Editor configuration)

# Launch editor
# Open Test.uproject directly
```

No CI pipeline, no lint/test commands configured.

- The user compiles manually. Do not run local build/compile commands unless the user explicitly asks for it.

## Module Dependencies

`Test.Build.cs` — `PublicDependencyModuleNames`:
`Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `SlateCore`

`PrivateDependencyModuleNames`:
`Slate`

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` files are the primary truth source.
- When repository markdown documents disagree, use this precedence: **source code > AGENTS.md > CLAUDE.md > GEMINI.md**.
- `CLAUDE.md` and `GEMINI.md` are secondary summaries; verify behavior-critical claims against the actual C++ before editing gameplay logic.

## Agent Coordination

- `plan.md` is the shared agent-to-agent communication and handoff file.
- Before continuing prior collaboration work, read the header rules in `plan.md`.
- Write agent feedback and implementation plans there as working context, not end-user documentation.
- For code review and closeout passes, ignore `Content/*.uasset` by default unless the user explicitly asks to inspect or include asset changes.

## Architecture

### State Machine System (`Source/Test/Public/Character/CharacterTypes.h`)

All gameplay states are defined as `UENUM` enums in `CharacterTypes.h` — the single source of truth:

| Enum | C++ Values | Used By |
|------|-----------|---------|
| `EWeaponState` | `EWS_Unequipped`, `EWS_OneHandEquipped`, `EWS_TwoHandEquipped` | `ABaseCharacter`, `AMyCharacter`, `USlashAnimInstance` |
| `EActionState` | `EAS_UnOccupied`, `EAS_Attacking`, `EAS_Stunning`, `EAS_Exhausted`, `EAS_Parrying`, `EAS_Dodging`, `EAS_Dead` | `AMyCharacter` |
| `EEnemyState` | `EES_UnOccupied`, `EES_Patrolling`, `EES_Searching`, `EES_Chasing`, `EES_Combating`, `EES_Attacking`, `EES_Stunned`, `EES_Dead` | `AEnemy` |
| `EItemState` | `EIS_Spawning`, `EIS_Dropped`, `EIS_Equipped` | `Aitem` (in `item.h`) |

**Critical: state flow is mixed C++ + montage delegate + `AnimNotify` driven.** Entry states are often set directly in C++ (`Attack()`, `GetHit_Implementation()`, `Die()`, `Dodge()`, `SetEnemyState()`). Recovery commonly uses `FOnMontageEnded` delegates with `bInterrupted` guards, while `AnimNotify` classes handle collision windows and designer-timed recoveries (`UAnimNotifyState_WeaponCollision`, `UAnimNotifyState_DodgeInvulnerable`, `UAnimNotify_SetActionState`, `UAnimNotify_EnemyHitReactEnd`, `UAnimNotify_EnemyAttackEnd`, `UAnimNotify_CharacterHitReactEnd`). Do not hardcode recovery transitions in `Tick()`.

### Montage Helper Boundaries

- `ABaseCharacter::PlayMontageSection(UAnimMontage*, const FName&)` is intentionally a tiny helper that only does `Montage_Play()` + `Montage_JumpToSection()`.
- End-delegate binding stays in the semantic callsites (`PlayAttackMontage()`, `PlayHitReactMontage()`). Do not replace this with a "universal montage entry" unless those recovery semantics genuinely converge.

### Class Hierarchy

```
AActor
├── Aitem (parabolic spawning, floating animation, overlap events)
│   ├── AWeapon (box-trace sweep collision, hit-stop, camera shake)
│   ├── AShield (offhand equip, block angle/damage/stamina config, block FX)
│   └── ATreasure (gold value, UTreasureData asset)
├── ABreakAbleActor + IHitInterface (StaticMesh → GeometryCollection swap)
├── AArenaGenerator (USplineComponent + UPCGComponent)
└── ABird (APawn subclass, flyable spectator)

ACharacter
├── AMyCharacter + IBlockableInterface (UAttributeComponent, spring arm + camera, weapon/shield equipping, hold-to-block)
└── AEnemy + IHitInterface (AI patrol/search/chase/combat FSM, directional hit react)

APlayerController → ACharacterController (Enhanced Input, move/look/jump/equip/attack/sprint/walk/block/lock-on/dodge/parry bindings, input debug snapshot owner)
UActorComponent → UAttributeComponent (health, gold, OnHealthChanged delegate)
               └── UPlayerLockOnComponent (lock-on state, target selection, camera tunables)
UWidgetComponent → UHealthBarComponent
UUserWidget → UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars, delayed buffer logic)
           └── UPlayerHUDWidget (player HP/stamina HUD + NativePaint debug text overlay + damage vignette)
UAnimInstance → USlashAnimInstance (GroundSpeed, Direction, state enums)
UDataAsset → UTreasureData
           └── UComboDataAsset (light attack combo chain configurations)
UAnimNotifyState → UAnimNotifyState_WeaponCollision
                 ├── UAnimNotifyState_DodgeInvulnerable
                 └── UAnimNotifyState_ComboWindow (opens character combo buffer window)
UInterface → UBlockableInterface (weapon hit interception before final damage application)
```

### Combat Pipeline

1. `ACharacterController::Input_Attack()` → `AMyCharacter::Attack()` (guard order: `bIsBlocking` → `ShouldUseSprintAttack()` → `CanAttack()` → combo)
2. `PlayAttackMontage()` with `UAnimNotifyState_WeaponCollision`
3. **NotifyBegin** → `AWeapon::StartWeaponTrace()` (records old box positions)
4. **NotifyTick** → `AWeapon::ExecuteWeaponTrace()` (sweep old→new center)
5. On hit:
   - shared tags between attacker and target mean **no `ApplyDamage`**, but the target still enters the shared hit-feedback path (`GetHit`, knockback, hit-stop, camera shake)
   - cross-team hits may be intercepted by `IBlockableInterface::TryBlockHit()` before final damage is applied
   - for `ABaseCharacter` targets, `AWeapon::ExecuteWeaponTrace()` writes a per-hit `FPendingHitContext` (instigator, knockback scale, blocked flag, stun flag) before calling `IHitInterface::GetHit()`
   - `ABaseCharacter::GetHit_Implementation()` consumes that context for knockback / normal hit-react routing, and leaf classes (`AMyCharacter`, `AEnemy`) clear it after their own stun logic runs
   - `AWeapon::ExecuteWeaponTrace()` is intentionally decomposed into `BuildIgnoreList()`, `ResolveHit()`, and `DispatchHitFeedback()`. Keep `ResolveHit()` focused on same-team/block/damage math, and keep `DispatchHitFeedback()` focused on context write / `GetHit()` / camera shake / hit stop / ignore list; do not inflate it into a generic combat pipeline without a real new use case
6. **NotifyEnd** → clears `IgnoreActors`
7. `OnAttackMontageEnded` delegate (with `bInterrupted` guard) → restores `EAS_UnOccupied` and resumes stamina regen

### Hit Knockback

- `ABaseCharacter` owns the shared short-distance weapon-hit knockback system through `FPendingHitContext`, `BaseHitKnockbackDistance`, `HitKnockbackDuration`, and `TickHitKnockback()`.
- Knockback is intentionally **weapon-hit feedback**, not generic damage feedback: traps / DOT that only call `TakeDamage()` do not automatically trigger it.
- Current defaults are constructor-set per class: `AMyCharacter` uses **10 cm**, `AEnemy` uses **5 cm**.
- Motion is explicit **quadratic ease-out**, applied in `Tick()` through `AddActorWorldOffset(..., true, &Hit)` so the capsule sweeps and can be blocked by walls.
- A new hit always overrides the previous knockback state. A zero-scale hit (for example full block) clears any in-flight knockback instead of letting the old motion continue.
- Successful blocks scale knockback by final post-block damage ratio (`DamageAfterBlock / Damage`), so a 95% reduction produces a very short pushback.
- Friendly weapon hits are intentionally allowed to trigger knockback and hit feedback even though they do not deal damage.

### Block System

- `AMyCharacter` implements `IBlockableInterface` and owns hold-to-block state through `bBlockInputHeld` + `bIsBlocking`.
- `ACharacterController` binds `BlockAction` start/end to `StartBlockInput()` / `ReleaseBlockInput()`.
- `AShield` is equipped to the offhand via `EquipToOffhand()` and provides block tuning values:
  `BlockHalfAngleDegrees`, `BlockedDamageMultiplier`, `BlockStaminaCostPerDamage`, `BlockMoveSpeedMultiplier`.
- `AMyCharacter::CanStartBlock()` is intentionally independent of weapon equip state. Current block-start gates are: shield equipped, `EAS_UnOccupied`, and grounded.
- `AWeapon::ExecuteWeaponTrace()` checks `IBlockableInterface` on the hit actor before final damage application.
- Successful blocks reduce or redirect damage through `FBlockResult`, suppress shared `DirectionalHitReact()` / `PlayHitEffects()`, and play shield-specific sound/particle feedback.
- Blocked hits still flow through `GetHit` via `FPendingHitContext`, so scaled knockback and class-specific feedback can still happen even when normal hit react is suppressed.
- Blocking is canceled when shield/state/falling conditions become invalid; blocked movement speed is reduced through `UpdateMovementSpeed()`.
- When tuning block damage reduction, update both the C++ default (`AShield::BlockedDamageMultiplier`) and the actual `BP_Shield` asset if it overrides the value in Blueprint.

### Dodge System

- `ACharacterController` owns the dodge input binding through `DodgeAction` → `Input_Dodge()` → `AMyCharacter::Dodge()`.
- `AMyCharacter::CanDodge()` gates on `EAS_UnOccupied`, grounded, and stamina > 0. Dodge interrupts active block and parry.
- `ComputeDodgeDirection()` returns the world-space dodge direction: movement input if present, otherwise camera-backward (lock-on) or actor-backward (non-lock).
- **Directional dodge in lock-on**: `SelectDodgeSection()` converts `ComputeDodgeDirection()` output to actor-local space via `GetActorRotation().UnrotateVector()` and selects montage Section:
  - `|Y| > |X|` and `|Y| > 0.3` → `Dodge_L` / `Dodge_R` (side roll, no turn)
  - `X > 0.3` → `Dodge_F` (forward roll, no turn)
  - Otherwise → `Dodge_B` marker (triggers 180° turn + `Dodge_F` playback)
- **Critical ordering**: `SelectDodgeSection()` must run **before** `FaceDirection2D()`, because turning changes `GetActorRotation()` and would corrupt the `UnrotateVector` result.
- Non-lock-on dodge always turns toward input direction and plays `Dodge_F`.
- `SetMovementRotationMode(false, false)` locks rotation during the roll; `OnDodgeMontageEnded` calls `RestoreRotationMode()` to recover.
- `UAnimNotifyState_DodgeInvulnerable` toggles `bDodgeInvulnerable` during the roll; `GetHit_Implementation()` and `TakeDamage()` both early-return when the flag is set. `OnDodgeMontageEnded` provides a safety clear.
- Dodge costs `DodgeStaminaCost` (default 15) and pauses stamina regen; regen resumes in `OnDodgeMontageEnded` (non-interrupted path).
- `ApplyLockOnRotationMode()` skips `EAS_Dodging` to prevent Tick-time rotation from overwriting dodge facing.

### Combo System

- **Data-Driven Configuration**: A light attack combo chain is defined via `UComboDataAsset` (set on `AMyCharacter::LightAttackCombo`). Each segment configures a custom Montage Section, a damage multiplier (scales base weapon damage), and a stamina cost.
- **Input Buffering**: Checked during `ACharacterController::Input_Attack()`. If `AMyCharacter::IsComboWindowOpen()` is true, the input is buffered by setting `bComboInputReceived = true`. Otherwise, a normal attack is initiated.
- **Combo Window State**: Controlled by `UAnimNotifyState_ComboWindow` placed in the attack montage. It calls `OpenComboWindow()` and `CloseComboWindow()` to toggle the input window.
- **Combo Progression**: In `AMyCharacter::OnAttackMontageEnded()`, if `bComboInputReceived` is true and a next combo segment exists, the character increases the combo counter, temporarily marks the action state as `EActionState::EAS_UnOccupied` (to pass the `CanAttack()` gate), and immediately triggers `Attack()`.
- **Reset Guards & Interruption**: On attack finish (without buffered inputs), normal interruption (getting hit, death, dodge roll, or exhaustion), `ResetCombo()` is called to reset the combo counter, input flag, and restore the damage multiplier to `1.0f`.
- **Damage Multiplier Application**: Applied in `AWeapon::ResolveHit()`. The weapon queries the attacker's `GetAttackDamageMultiplier()` to scale the raw damage *before* passing it to `IBlockableInterface::TryBlockHit()`, ensuring block stamina costs and damage mitigation calculations scale accordingly.
- **Debug Visibility**: Rendered via `FDebugDrawHelper` in `DrawDebugInfo` when master debug rendering is active, printing combo index, input/window status, and current damage multiplier.

### Sprint Attack System

- Triggered via `ACharacterController::Input_Attack()` → `AMyCharacter::Attack()` → `ShouldUseSprintAttack()` → `PerformSprintAttack()`.
- **Priority**: sprint attack is checked **before** `CanAttack()` and the normal combo flow. Guard order: `bIsBlocking` → `ShouldUseSprintAttack()` → `CanAttack()` → combo logic.
- **Conditions** (`ShouldUseSprintAttack()`): `bIsSprinting`, movement input (`GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER`), weapon equipped (`WeaponState != EWS_Unequipped`), `EAS_UnOccupied`, grounded. No lock-on restriction — works in both lock-on and non-lock states.
- **Reuses `EAS_Attacking`**: no new `EActionState` enum value. No `bIsSprintAttack` flag — `OnAttackMontageEnded()` handles cleanup correctly without one: sprint attacks never open a combo window, so `bShouldContinueCombo` is always false, and `ResetCombo()` + `RestoreRotationMode()` + exhaustion check all work as-is.
- **Independent montage** (`SprintAttackMontage`): separate from the combo montage to avoid pollution. Requires `AnimNotifyState_WeaponCollision` for weapon trace windows. End delegate reuses `OnAttackMontageEnded()`.
- **Replaces lock-on free-run attack**: the old `ShouldUseLockOnFreeRun()` + `FaceDirection2D()` + `SetMovementRotationMode()` branch in `Attack()` is removed. `ShouldUseLockOnFreeRun()` itself is preserved — it's still needed for lock-on sprint **movement** behavior (speed, rotation mode, camera in `UpdateMovementSpeed()`, `ApplyLockOnRotationMode()`, `GetLockOnCameraTargets()`).
- **Allows stamina overspend**: uses `Attributes->UseStamina()` directly with no `CheckStamina()` gate, consistent with the "last action overspend" design shared by dodge, parry, and normal attacks.
- **Clears combo state**: calls `ResetCombo()` at the start of `PerformSprintAttack()` before setting `SprintAttackDamageMultiplier`, so stale combo state from a prior attack cannot contaminate the sprint hit.
- **Direction with fallback**: faces `GetLastMovementInputVector().GetSafeNormal2D()` direction, falling back to `GetActorForwardVector()` if input is nearly zero (defensive pattern borrowed from `ComputeDodgeDirection()`).
- **Rotation lock**: sets `bOrientRotationToMovement = false` and `bUseControllerRotationYaw = false` during the attack; `OnAttackMontageEnded()` calls `RestoreRotationMode()` to recover.
- **Stops sprinting**: calls `StopSprinting()` after rotation lock and before montage play, so the character returns to normal run speed when the attack montage ends.
- **Not in combo system**: sprint attack does not use `AnimNotifyState_ComboWindow` and does not advance `ComboCounter`. It is a single-hit attack that ends with `EAS_UnOccupied`.
- **Config** (all `EditDefaultsOnly` on `AMyCharacter` under `"Combat|Sprint Attack"`):
  - `SprintAttackMontage` — independent montage asset
  - `SprintAttackDamageMultiplier` — default `1.8f`
  - `SprintAttackStaminaCost` — default `25.f`
- **Implementation files**: `Source/Test/Public/Character/MyCharacter.h:34-35` (declaration), `Source/Test/Private/Character/MyCharacter.cpp:95-153` (implementation).

### Lock-On System

- `ACharacterController` owns the lock-on input binding through `LockOnAction` → `Input_LockOn()`; the current input assets are `Content/_GAME/BP/input/IA_LockOn.uasset`, `Content/_GAME/BP/input/IMC_CharacterInput.uasset`, and `Content/_GAME/BP/input/BP_CharacterController.uasset`.
- `UPlayerLockOnComponent` owns lock-on state (`LockedTarget`, `bIsLockingOn`), tunables (`LockOn*`, `LockOnCamera*`), target scoring, and player-targeted mark handoff to `AEnemy::SetTargetedByPlayer(bool)`.
- `AMyCharacter` keeps the facade (`ToggleLockOn()`, `ClearLockOn()`, `IsLockingOn()`), plus all direct writes to `CharacterMovement`, controller rotation, and `SpringArm`.
- `UPlayerLockOnComponent::FindBestTarget(...)` currently uses `GetAllActorsOfClass(AEnemy::StaticClass())`, filters by radius and camera-facing angle, then prefers the candidate closest to the camera forward axis.
- `UpdateLockOn()` is intentionally split: target validity / range cleanup always runs, while camera-facing rotation is skipped during `EAS_Stunning` and `EAS_Dead`.
- `ClearLockOn()` must be able to clear state after invalid / pending-kill targets. Target cleanup belongs to `UPlayerLockOnComponent`, while local rotation/camera recovery remains unconditional on `AMyCharacter`.
- Lock-on camera framing currently stays on **yaw-only** control rotation. If future work wants target-height framing to matter, that requires changing the camera-aim math; adding a height offset alone is a no-op while `LookAt.Pitch` is cleared back to zero.
- Lock-on Sprint free-run is a derived movement mode, **not** a separate `EActionState`. `ShouldUseLockOnFreeRun()` is based on lock-on, sprint intent, `EAS_UnOccupied`, grounded state, and live movement input.
- During lock-on free-run, the target remains locked and the controller / camera still yaw toward the enemy, but the character body temporarily uses `bOrientRotationToMovement=true` and `bUseControllerRotationYaw=false` so movement direction drives facing.
- **Sprint Attack replaces free-run attack**: the old free-run attack branch in `Attack()` (which played a normal combo segment at movement direction) has been removed. Sprinting + attack now triggers `PerformSprintAttack()` regardless of lock-on state, with a dedicated `SprintAttackMontage` and configurable damage multiplier. `ShouldUseLockOnFreeRun()` is still used for lock-on sprint **movement** (speed, rotation mode, camera offset).
- Lock-on Sprint free-run camera framing now adds a small movement-direction offset under `LockOnCamera`: strafe input shifts the shoulder laterally, while backward input adds extra camera height plus an optional `TargetArmLength` bonus. Keep the target locked; do not turn this into a free-look mode.

### Lock-On Camera

- The current over-shoulder lock-on camera is driven by `SpringArm->SocketOffset`, not by a separate camera actor or a second spring arm.
- `CachedSocketOffset` is initialized once from the live `SpringArm->SocketOffset` in `BeginPlay()`. This is intentional so Blueprint overrides remain the source of truth for the non-lock camera baseline; do not overwrite that baseline every time lock-on starts.
- `CachedTargetArmLength` is also initialized once from the live `SpringArm->TargetArmLength` in `BeginPlay()`, so optional free-run camera pull-back can always recover to the Blueprint-authored baseline.
- `Tick()` interpolates both `SpringArm->SocketOffset` and `SpringArm->TargetArmLength` before the `Stunning/Dead` early-return. Non-lock moves back toward `CachedSocketOffset` / `CachedTargetArmLength`, ordinary lock-on uses `LockOnSocketOffset`, and lock-on Sprint free-run uses `LockOnSocketOffset + DynamicOffset` plus an optional arm-length bonus.
- Current lock-on camera tunables now live on `UPlayerLockOnComponent`, but the actual `SpringArm` writes still stay on `AMyCharacter`.
- Free-run camera offset is driven by controller-yaw local movement input via `GetControlRotation().Yaw` + `UnrotateVector`, not Actor-local direction or velocity. Free-run rotates the body toward movement, so Actor-local space would lose left/right/back intent; velocity also drops to zero when the player pushes into walls.
- Current code defaults are `LockOnSocketOffset = (0, 80, 80)`, `LockOnSocketOffsetInterpSpeed = 6.f`, `LockOnFreeRunCameraSideOffset = 60.f`, `LockOnFreeRunCameraBackHeightOffset = 40.f`, `LockOnFreeRunCameraBackArmLengthBonus = 0.f`, and `LockOnFreeRunCameraInterpSpeed = 10.f`, but these are tuning values rather than architectural truth; feel free to retune them in Blueprint / defaults without changing the control flow.
- The current camera still computes `LookAt` from `PlayerLoc -> TargetLoc`. That is enough for the present "cheap fix" shoulder framing, but it does **not** mathematically guarantee that the enemy stays centered after socket offset. If future tuning still leaves the target drifting too far off the main viewing area, prefer compensating the aim from the offset camera position before adding arbitrary yaw-bias magic numbers.
- **Camera Recenter**: Triggered via `AMyCharacter::StartCameraRecenter()` when `Input_LockOn()` finds no valid target. It captures a snapshot of the current actor rotation (`RecenterTargetRotation`) and limits the pitch to `RecenterTargetPitch` (default `-10.f`). `Tick()` then interpolates the controller rotation towards this fixed snapshot via `UpdateCameraRecenter()`.
- Recentering uses `FMath::RInterpTo` and is resilient to `EAS_Stunning` / `EAS_Dead` (running before the early exit in `Tick()`).
- The recenter process is aborted via `StopCameraRecenter()` if the player moves the mouse (`Input_Look()`), successfully locks onto a target, or dies.
- There is intentionally no audio feedback during recenter, maintaining a 'Souls-like' minimalist feel.

### Shared Direction Helper

- `ABaseCharacter::CalcForwardDot2D(const FVector& WorldDirection)` is the shared 2D facing helper. Callers must pass a world-space direction vector, not a target location.
- Use it for cases like movement-vs-forward, attacker direction, or target direction. Keep `IHitInterface::GetHitDirection()` separate for signed-angle hit-react routing.
- Locomotion BlendSpaces in this project are driven by `Direction` on X (`-180..180`) and `GroundSpeed` on Y (`0+`). Do not convert Y into signed forward/back speed for the current setup; backward samples belong at `X = ±180` with positive Y speeds, and strafe samples belong at `X = ±90`.

### Health / Regen

- `UAttributeComponent` owns shared health/stamina state for both player and enemies; do not enable new behavior globally there unless every user of the component should get it.
- Health regeneration is opt-in: `UAttributeComponent` exposes regen support, but it should stay disabled by default.
- `AMyCharacter` explicitly enables health regeneration in `BeginPlay()`; enemies should not auto-enable it.
- Player regen currently targets **1 HP/s** through `HealthRegenRate`, driven in `TickComponent()` via `AddHealth(...)` and `OnHealthChanged`.

### Stamina / Exhaustion

- `UAttributeComponent::UseStamina()` intentionally allows a brief "last action" overspend by clamping to `[-Amount, MaxStamina]` before flooring back to `0`. Do not remove that transient negative window unless you also redesign exhaustion timing.
- `OnExhausted` is broadcast once per depletion window; `bStaminaJustDepleted` is the guard that suppresses repeat broadcasts while stamina is already empty.
- `AMyCharacter::HandleExhausted()` interrupts block, enters `EAS_Exhausted`, and starts the recover timer through `ExhaustedTime` (current default `5.f`).
- `AMyCharacter::RecoverFromExhaustion()` first checks the state guard, then calls `ResetExhaustionFlag()` before `AddStamina(1.f)` so the player cannot get stuck in a permanently exhausted gate after an overspend.
- Attack flow pauses stamina regen through `PauseStaminaRegen()`, and montage recovery paths resume it through `ResumeStaminaRegen()`.
- **Last Action Montages**: "Last action" overspend (like attack, dodge, parry) does **not** check for `EAS_Exhausted` before playing their montages. We intentionally allow the final montage to play to give the player visual feedback.
- **Exhaustion State Recovery**: Higher priority states (like `EAS_Stunning` from getting hit) can interrupt `EAS_Exhausted`. Because of this, montage end delegates (e.g., `OnAttackMontageEnded`) and anim notify state resets (e.g., `UAnimNotify_CharacterHitReactEnd`) must check `IsExhaustionTimerActive()` before blindly reverting to `EAS_UnOccupied`. If the timer is still active, they must correctly revert back to `EAS_Exhausted` instead, and still call `ResumeStaminaRegen()` if applicable.

### Movement Speed

- Player movement speed is updated continuously in `AMyCharacter::UpdateMovementSpeed()`; keep this logic on the character, not the controller.
- Current base speeds are walk `150`, run `300`, sprint `450`. Non-lock movement is treated as free movement and passes a forward dot into `CalcBaseSpeed()` so sprint intent can apply without fake side/back penalties.
- Ordinary lock-on movement uses directional speed interpolation from forward `1.0` through `LockOnStrafeSpeedMultiplier` to `LockOnBackSpeedMultiplier`; current defaults are strafe `0.95` and back `0.9`.
- Lock-on Sprint free-run bypasses lock-on directional slowdown: any movement-input direction can reach sprint speed while the locked camera remains on the enemy.
- The state multiplier is chosen before directional scaling: blocking uses `EquippedShield->BlockMoveSpeedMultiplier`, otherwise `1.0f`. Being armed / holding a weapon does not reduce normal movement speed by itself.
- `TickSprintStamina()` drains stamina while grounded, unblocked, and moving. In ordinary lock-on combat step it still uses the forward-dot gate, while lock-on free-run bypasses that gate so side/back sprint consumes stamina.

### Enemy AI (`AEnemy`)

- Controlled by `AAIController` through `SetEnemyState(EEnemyState)` — the real flow is Patrol/Search/Chase/Combat, not just chase/attack.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid target returns to patrol, non-combat states enter `EES_Combating` inside `CombatingRadius`, and combat-family states (`EES_Combating`, `EES_Attacking`, `EES_Stunned`) use `CombatingRadius + CombatExitBuffer` as their chase fallback threshold before dropping to `EES_Chasing`.
- `CheckCombatTarget()` validity is no longer just pointer validity: `IsValidCombatTarget()` treats dead `ABaseCharacter` targets as invalid, so corpse targets are cleared and the enemy returns to patrol instead of looping on attacks.
- `TargetPerceptionUpdated()` and `CanAttack()` also gate on `IsValidCombatTarget()`, preventing sight reacquire or attack start against a dead player even if the Actor still exists.
- `SetEnemyState(EES_Combating)` is an entry-action boundary: it disables orient-to-movement and resets combat reposition state. When entering from `EES_Chasing` while still outside `CombatAttackMaxRadius`, it intentionally skips `StopEnemyMovementIfPossible()` so the chase-to-combat handoff does not create a visible brake.
- `OnSearching()` stops movement, disables orient-to-movement, starts `PatrolTimer` and repeating `LookTimer`, then rotates toward `GenerateNewLookRotation()` using `PatrolRotationSpeed`.
- `ClearPatrolTimers()` must be called when leaving patrol/search states or on death to avoid stale timers firing after state changes.
- `OnChasing()` reissues `MoveToTarget()` if path-following falls back to idle. For `ChasingTarget`, `MoveToTarget()` explicitly disables agent/goal radius reach tests and stops at `CombatAttackMaxRadius - CombatPressMargin`, so chase handoff reaches attack-ready range instead of parking at the combat boundary.
- `OnCombating()` now separates **attack-ready gap close** from **cooldown spacing**:
  `CombatAttackMaxRadius` gates whether the enemy may start an attack, attack-ready gap close uses `MoveToCombatTarget()` to dynamically track `ChasingTarget` without `NextCombatRepositionTime` throttling, and cooldown-only spacing uses `CombatPreferredMinRadius` / `CombatPreferredMaxRadius` through `UpdateCombatMovement()` and `MoveToCombatLocation()`.
- The current extension seam for new enemy archetypes is intentionally narrow: override `OnChasing()`, `ShouldTriggerAttack(...)`, `HandleAttackReadyPositioning(...)`, and `HandleCooldownPositioning(...)` to change combat style, while keeping `SetEnemyState()`, `CheckCombatTarget()`, and the `MoveTo*()` execution helpers in the base class.
- Current code defaults are:
  `CombatTooCloseRadius(90) < CombatAttackMaxRadius(170) <= CombatPreferredMinRadius(210) <= CombatPreferredMaxRadius(270) < CombatingRadius(300) < ChasingRadius(1000)`.
  Keep this ordering intact when tuning; if `CombatPreferredMaxRadius >= CombatingRadius`, retreat/strafe targets will push the enemy out of `EES_Combating` and immediately back into `EES_Chasing`.
- Keep `CombatPressMargin(25)` greater than `CombatRepositionAcceptanceRadius(12)`. If the margin is smaller, press movement can be considered complete while the enemy is still just outside `CombatAttackMaxRadius`, producing a stand-still-at-edge bug.
- `OnAttackCooldownEnd()` must clear combat reposition state via `ResetCombatReposition()`. If the enemy is still in `EES_Combating`, it should reuse the same combat decision seam as Tick: `ShouldTriggerAttack(...)` decides whether to stop in place, otherwise `HandleAttackReadyPositioning(...)` drives the next move. Do not blindly stop navigation outside combat, because the cooldown timer can fire after the state already changed.
- During cooldown, `UpdateCombatMovement()` chooses `Retreat` / `BackDiag` / `Strafe` / `Press` based on current distance. `MoveToCombatLocation()` only marks `bRepositionInProgress` on `RequestSuccessful`, retries quickly on failure, and relies on `ReceiveMoveCompleted` → `OnRepositionMoveCompleted(...)` to clear the in-progress flag.
- Combat movement speed is intentionally tied to the existing state speeds: `Press` uses `ChaseSpeed`, while `Retreat` / `BackDiag` / `Strafe` use `PatrolSpeed` as their base. Do not reintroduce independent combat press/reposition speed knobs without a real design need.
- `Retreat` and `BackDiag` add a speed ease on top of path following: they start at `PatrolSpeed` and slow toward `PatrolSpeed * CombatRetreatMinSpeedRatio` as they approach the goal. This only changes `CharacterMovement->MaxWalkSpeed`; navigation and obstacle handling still belong to `MoveToCombatLocation()`.
- `MoveToCombatLocation()` intentionally uses `FAIMoveRequest` with `SetReachTestIncludesAgentRadius(false)` and `SetReachTestIncludesGoalRadius(false)`. Do not reintroduce manual `ProjectPointToNavigation(...)` here; `AAIController::MoveTo()` already performs goal projection with the AI's nav agent properties.
- `MoveToTarget()`, `MoveToLocation()`, `MoveToCombatLocation()`, and `MoveToCombatTarget()` are intentionally separate semantic wrappers. If you ever dedupe them, only extract the shared `FAIMoveRequest` boilerplate; do not collapse them into one flag-heavy helper.
- When planning extensibility for new enemy archetypes, prefer splitting **combat decision** from **navigation execution**. Current `AEnemy` seam is: `OnChasing()` is virtual, and combat behavior is split into `ShouldTriggerAttack(...)`, `HandleAttackReadyPositioning(...)`, and `HandleCooldownPositioning(...)`. Extend new archetypes by overriding those hooks rather than `MoveTo*()` helpers or copying the whole combat loop. Keep `SetEnemyState()` / `CheckCombatTarget()` as base-owned boundaries unless a variant truly needs a different state machine.
- `EES_Attacking`, `EES_Stunned`, and `EES_Dead` are hard-stop states for Tick-driven AI reactions.

### Health Bar Buffer System

- `UBaseHealthBarWidget` owns two `UProgressBar`: `PB_Health` for the immediate value and `PB_Buffer` for delayed catch-up.
- `SetHealthPercent()` updates `PB_Health` immediately; when health drops it resets `CurrentBufferDelay` using `BufferDelayTime` before `PB_Buffer` starts moving.
- `NativeTick()` lerps `PB_Buffer` toward `PB_Health` with `BufferInterpSpeed`; healing snaps the buffer bar upward immediately instead of animating.
- Shared buffer interpolation math lives in `UBaseHealthBarWidget::TickBufferDelayImpl(...)`; `UPlayerHUDWidget` reuses that helper while keeping player-only `DamageFlash` / `Vignette` behavior local.
- `UHealthBarComponent::SetHealthPercent()` delegates to the widget instance, and `UAttributeComponent::ReceiveDamage()` broadcasts `OnHealthChanged` for UI bindings.
- Enemy health bar visibility is timer-driven by `AEnemy::ShowHealthBar()` / `HideHealthBar()`, with Blueprint fade-out hooks through `UBaseHealthBarWidget::PlayFadeOutAnim()` and `CancelFadeOutAnim()`.
- `AEnemy::RevealHealthBar()` is the shared re-entry path used by both `ShowHealthBar()` and `SetTargetedByPlayer(true)`: it restores component visibility, widget visibility, render opacity, and cancels any in-flight fade-out animation before the normal timer/lock logic resumes.
- Lock-on keeps enemy health bars visible through `AEnemy::SetTargetedByPlayer(bool)`. When this path changes, verify both sides: C++ timer behavior and the actual Widget Blueprint implementation in `Content/_GAME/BP/HUD/WBP_EnemyHealthBar.uasset`.

### Player HUD

- `AMyCharacter::InitializePlayerHUD()` creates `UPlayerHUDWidget`, adds it to the viewport, and binds it to the live `UAttributeComponent`; HUD setup is not owned by the controller.
- `UPlayerHUDWidget::BindToAttributes()` immediately pushes current health/stamina values after binding, so UI initialization does not wait for the next attribute change event.

### Player Hit Feedback

- Player hurt feedback is split intentionally: **camera shake = hit reaction**, **red vignette = health loss**.
- `AMyCharacter::GetHit_Implementation()` triggers `HitReceivedCameraShake` for the local player whenever the weapon-hit feedback path reaches `GetHit`, including blocked hits and same-team weapon hits.
- `AMyCharacter::TryBlockHit()` writes `LastDamageFlashScale` from `AShield::BlockedDamageMultiplier`; `TakeDamage()` pushes that scale into `UPlayerHUDWidget::SetPendingDamageFlashScale(...)` before damage application, then immediately resets the local value so hits cannot leak state across frames.
- `UPlayerHUDWidget::SetHealthPercent()` consumes `PendingDamageFlashScale` only when health actually drops, so the vignette intensity tracks final post-block damage instead of raw incoming damage.
- Result: blocked or friendly hits may still shake the camera, but only real health loss drives the red vignette.
- The vignette mask is generated in C++ as an **edge-distance gradient**, not a radial center fade. `VignetteFadeWidth = 0.2` means roughly the outer 20% of the screen carries the red falloff while the center stays clear.

### Debug Overlay

- Runtime debug text is gathered through `FDebugDrawHelper` and rendered by `UPlayerHUDWidget::NativePaint()`.
- Debug text entries are frame-scoped: `Add()` resets the cache on frame change, and `GetEntries()` returns an empty array when no entries were submitted this frame.
- `ACharacterController` owns the player input debug snapshot through held-state flags, move vector sampling, and short-lived action markers exposed by `GetDebugInputText()`.
- `UPlayerHUDWidget::NativePaint()` must use the layer returned by `Super::NativePaint(...)` as its base. Do not draw from the incoming `LayerId` directly.
- Console variables:
  - `test.Debug.Enable` → master text/shape toggle
  - `test.Debug.Enemy` → enemy text toggle
  - `test.Debug.Shapes` → world debug sphere toggle
- Player debug currently includes input snapshot text, HP, stamina, action state, montage name, and movement speed.
- Player debug currently includes the short-lived `LockOn` marker from `Input_LockOn()` in addition to the existing input snapshot text.
- Enemy debug currently includes enemy state / ground speed text, distance text, optional chase/combat/attack radius spheres, and `CombatMove: Ready/Retreat/BackDiag/Strafe/Press/AlreadyAtGoal/MoveFail` while in `EES_Combating`.

### UE 5.7 Pitfalls

- `GetCurrentActiveMontage()` can be `nullptr`; current debug code null-checks the pointer before reading montage/section names.
- The current Slate geometry calls use `ToPaintGeometry(Size, FSlateLayoutTransform(...))`; do not reintroduce older deprecated signatures.
- `UPlayerHUDWidget`'s runtime vignette is a transient texture. When rebuilding it, clear the brush resource first, destroy the old texture with `ConditionalBeginDestroy()`, then recreate it with `UTexture2D::CreateTransient(...)`.
- `AWeapon::ResolveHit()` returning `FWeaponHitResult` is an established local pattern. Prefer a small result struct over piling on more out params when a combat helper needs to return several coupled values.

### Content Organization

- C++ source: `Source/Test/` (Public/Private mirrors UE convention).
- **Only modify assets under `Content/_GAME/`**.
- Reference assets (AncientContent, AnimalVarietyPack, ParagonAurora, Mixamo, etc.) are read-only marketplace content — do not touch them.

## Code Conventions

### Includes

```cpp
// System/engine first, then project — forward declare when possible
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CharacterTypes.h"   // project includes use full relative path from Public/
#include "MyCharacter.generated.h"        // .generated.h must be last include

class UAttributeComponent;               // forward declare in headers, include in .cpp
class AWeapon;
```

### UPROPERTY / UFUNCTION

- Components: `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` with `meta = (AllowPrivateAccess = "true")` when private.
- Editable config: `EditAnywhere` or `EditDefaultsOnly`.
- Runtime-only props: `VisibleInstanceOnly`.
- If the goal is a readable tooltip in Blueprint / Details panels, use `meta = (ToolTip = "...")`; line comments only help source readers.
- Functions exposed to BP: use `BlueprintCallable` or `BlueprintNativeEvent`.
- Interface functions: use `BlueprintNativeEvent` + `_Implementation` suffix.
- Any function bound through `AddDynamic(...)` must be marked with `UFUNCTION()`, even if it is not exposed to Blueprint. Current examples include `HandleExhausted`, `TargetPerceptionUpdated`, `SphereOverlap`, `SphereEndOverlap`, and HUD health/stamina update callbacks.
- UMG child widgets are bound with `UPROPERTY(meta = (BindWidget))`; Blueprint-owned presentation hooks use `BlueprintImplementableEvent` (for example `UBaseHealthBarWidget::PlayFadeOutAnim()`).
- Do not assume a repo-wide `TObjectPtr` migration. Follow the surrounding file style: this codebase mostly uses reflected raw pointers, with `TObjectPtr` appearing only in a few spots such as `AArenaGenerator`.

### Naming

| Type | Convention | Example |
|------|-----------|---------|
| Classes | `A`/`U` prefix, PascalCase | `AMyCharacter`, `UAttributeComponent` |
| Enums | `E` prefix, `ECS_`/`EAS_`/`EES_` prefixes for values | `EActionState::EAS_Attacking` |
| Interfaces | `I` prefix, PascalCase | `IHitInterface`, `IPickupInterface` |
| Members | PascalCase, no `m_` or `_` prefix | `OverLapItem`, `ActionState` |
| Booleans | `b` prefix | `bIsSprinting` |
| Methods | PascalCase | `GetCharacterState()`, `PlayAttackMontage()` |

### Accessors

Use `FORCEINLINE` inline in headers:

```cpp
FORCEINLINE EWeaponState GetCharacterState() const { return CharacterState; }
FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
```

### Virtual Functions

Override lifecycle functions (`BeginPlay`, `Tick`) and interface implementations in subclasses. Always call `Super::` in overrides unless explicitly avoiding it.

### Error Handling

Use `check()` / `ensure()` for invariants. No `try/catch` unless interacting with third-party code that throws.

### Comments

- Comments in **Chinese** for gameplay intent (current convention in codebase).
- English is acceptable for API docs and technical notes.
- Use `/** */` only when documenting UFUNCTION/UPROPERTY.

## User Profile

- UE 5.7 C++ developer, based in the United States.
- Prefers **concise, accurate** answers grounded in source code facts.
- Chinese is acceptable for general communication; code and technical terms remain in English.
