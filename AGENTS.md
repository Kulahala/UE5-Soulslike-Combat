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
`Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`

`PrivateDependencyModuleNames`:
`Slate`, `SlateCore`

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` files are the primary truth source.
- When repository markdown documents disagree, use this precedence: **source code > AGENTS.md > CLAUDE.md > GEMINI.md**.
- `CLAUDE.md` and `GEMINI.md` are secondary summaries; verify behavior-critical claims against the actual C++ before editing gameplay logic.

## Agent Coordination

- `plan.md` is the shared agent-to-agent communication and handoff file.
- Before continuing prior collaboration work, read the header rules in `plan.md`.
- Write agent feedback and implementation plans there as working context, not end-user documentation.

## Architecture

### State Machine System (`Source/Test/Public/Character/CharacterTypes.h`)

All gameplay states are defined as `UENUM` enums in `CharacterTypes.h` — the single source of truth:

| Enum | C++ Values | Used By |
|------|-----------|---------|
| `EWeaponState` | `EWS_Unequipped`, `EWS_OneHandEquipped`, `EWS_TwoHandEquipped` | `ABaseCharacter`, `AMyCharacter`, `USlashAnimInstance` |
| `EActionState` | `EAS_UnOccupied`, `EAS_Attacking`, `EAS_Arming`, `EAS_Stunning`, `EAS_Exhausted`, `EAC_Dead` | `AMyCharacter` |
| `EArmWeaponState` | `AWS_Arming`, `AWS_Disarming` | `AMyCharacter`, `USlashAnimInstance` |
| `EEnemyState` | `EES_UnOccupied`, `EES_Patrolling`, `EES_Searching`, `EES_Chasing`, `EES_Combating`, `EES_Attacking`, `EES_Stunned`, `EES_Dead` | `AEnemy` |
| `EItemState` | `EIS_Spawning`, `EIS_Dropped`, `EIS_Equipped` | `Aitem` (in `item.h`) |

**Critical: state flow is mixed C++ + montage delegate + `AnimNotify` driven.** Entry states are often set directly in C++ (`Attack()`, `GetHit_Implementation()`, `Die()`, `SetEnemyState()`). Recovery commonly uses `FOnMontageEnded` delegates with `bInterrupted` guards, while `AnimNotify` classes handle collision windows and designer-timed recoveries (`UAnimNotifyState_WeaponCollision`, `UAnimNotify_SetActionState`, `UAnimNotify_SetArmWeaponState`, `UAnimNotify_EnemyHitReactEnd`, `UAnimNotify_EnemyAttackEnd`, `UAnimNotify_CharacterHitReactEnd`). Do not hardcode recovery transitions in `Tick()`.

### Montage Helper Boundaries

- `ABaseCharacter::PlayMontageSection(UAnimMontage*, const FName&)` is intentionally a tiny helper that only does `Montage_Play()` + `Montage_JumpToSection()`.
- End-delegate binding stays in the semantic callsites (`PlayAttackMontage()`, `PlayHitReactMontage()`, `PlayArmMontage()`). Do not replace this with a "universal montage entry" unless those recovery semantics genuinely converge.

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

APlayerController → ACharacterController (Enhanced Input, move/look/jump/equip/attack/arm/sprint/walk/block/lock-on bindings, input debug snapshot owner)
UActorComponent → UAttributeComponent (health, gold, OnHealthChanged delegate)
UWidgetComponent → UHealthBarComponent
UUserWidget → UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars, delayed buffer logic)
           └── UPlayerHUDWidget (player HP/stamina HUD + NativePaint debug text overlay + damage vignette)
UAnimInstance → USlashAnimInstance (GroundSpeed, Direction, state enums)
UDataAsset → UTreasureData
UInterface → UBlockableInterface (weapon hit interception before final damage application)
```

### Combat Pipeline

1. `ACharacterController::Input_Attack()` → `AMyCharacter::Attack()`
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
- `AMyCharacter::CanStartBlock()` is intentionally **independent of `ArmWeaponState`**. Current block-start gates are: shield equipped, `EAS_UnOccupied`, `!bIsArming`, and grounded.
- `AWeapon::ExecuteWeaponTrace()` checks `IBlockableInterface` on the hit actor before final damage application.
- Successful blocks reduce or redirect damage through `FBlockResult`, suppress shared `DirectionalHitReact()` / `PlayHitEffects()`, and play shield-specific sound/particle feedback.
- Blocked hits still flow through `GetHit` via `FPendingHitContext`, so scaled knockback and class-specific feedback can still happen even when normal hit react is suppressed.
- Blocking is canceled when shield/state/falling conditions become invalid; blocked movement speed is reduced through `UpdateMovementSpeed()`.
- When tuning block damage reduction, update both the C++ default (`AShield::BlockedDamageMultiplier`) and the actual `BP_Shield` asset if it overrides the value in Blueprint.

### Lock-On System

- `ACharacterController` owns the lock-on input binding through `LockOnAction` → `Input_LockOn()`; the current input assets are `Content/_GAME/BP/input/IA_LockOn.uasset`, `Content/_GAME/BP/input/IMC_CharacterInput.uasset`, and `Content/_GAME/BP/input/BP_CharacterController.uasset`.
- `AMyCharacter` owns lock-on state through `LockedTarget` + `bIsLockingOn`; do not introduce a separate lock-on action state unless the state machine genuinely needs it.
- `FindLockOnTarget()` currently uses `GetAllActorsOfClass(AEnemy::StaticClass())`, filters by `LockOnRadius`, `LockOnViewAngleDegrees`, and target alive state, then prefers the candidate closest to the camera forward axis.
- `UpdateLockOn()` is intentionally split: target validity / range cleanup always runs, while camera-facing rotation is skipped during `EAS_Stunning` and `EAC_Dead`.
- `ClearLockOn()` must be able to clear state after invalid / pending-kill targets; keep the target cleanup path guarded by `IsValid(LockedTarget)` and keep the local state recovery unconditional.
- Lock-on camera framing currently stays on **yaw-only** control rotation. If future work wants target-height framing to matter, that requires changing the camera-aim math; adding a height offset alone is a no-op while `LookAt.Pitch` is cleared back to zero.

### Lock-On Camera

- The current over-shoulder lock-on camera is driven by `SpringArm->SocketOffset`, not by a separate camera actor or a second spring arm.
- `CachedSocketOffset` is initialized once from the live `SpringArm->SocketOffset` in `BeginPlay()`. This is intentional so Blueprint overrides remain the source of truth for the non-lock camera baseline; do not overwrite that baseline every time lock-on starts.
- `Tick()` interpolates `SpringArm->SocketOffset` in both directions: lock-on moves toward `LockOnSocketOffset`, non-lock moves back toward `CachedSocketOffset`. Keep this interpolation before the `Stunning/Dead` early-return so death and hard-stun exits still recover the camera framing.
- Current code defaults are `LockOnSocketOffset = (0, 80, 80)` and `LockOnSocketOffsetInterpSpeed = 6.f`, but these are tuning values rather than architectural truth; feel free to retune them in Blueprint / defaults without changing the control flow.
- The current camera still computes `LookAt` from `PlayerLoc -> TargetLoc`. That is enough for the present "cheap fix" shoulder framing, but it does **not** mathematically guarantee that the enemy stays centered after socket offset. If future tuning still leaves the target drifting too far off the main viewing area, prefer compensating the aim from the offset camera position before adding arbitrary yaw-bias magic numbers.

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

### Movement Speed

- Player movement speed is updated continuously in `AMyCharacter::UpdateMovementSpeed()`; keep this logic on the character, not the controller.
- Current base speeds are walk `150`, run `300`, sprint `450`. Sprint only applies while `ActionState == EAS_UnOccupied` and forward `DotProduct > 0.2`.
- Directional scaling is intentional: forward `100%`, lateral `80%`, backward `65%`.
- The state multiplier is chosen before directional scaling: blocking uses `EquippedShield->BlockMoveSpeedMultiplier`, arming uses `0.875f`, otherwise `1.0f`.
- `TickSprintStamina()` only drains stamina while grounded, unblocked, actually moving, and pushing forward; it also resets the stamina regen cooldown each frame during active sprint drain.

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
- Lock-on keeps enemy health bars visible through `AEnemy::SetTargetedByPlayer(bool)`. When this path changes, verify both sides: C++ timer behavior and the actual Widget Blueprint implementation in `Content/_GAME/BP/HUD/WBP_EnemyHealthBar.uasset`.

### Player HUD

- `AMyCharacter::InitializePlayerHUD()` creates `UPlayerHUDWidget`, adds it to the viewport, and binds it to the live `UAttributeComponent`; HUD setup is not owned by the controller.
- `UPlayerHUDWidget::BindToAttributes()` immediately pushes current health/stamina values after binding, so UI initialization does not wait for the next attribute change event.

### Player Hit Feedback

- Player hurt feedback is split intentionally: **camera shake = hit reaction**, **red vignette = health loss**.
- `AMyCharacter::GetHit_Implementation()` triggers `HitReceivedCameraShake` for the local player whenever the weapon-hit feedback path reaches `GetHit`, including blocked hits and same-team weapon hits.
- `AMyCharacter::TryBlockHit()` writes `LastDamageFlashScale` from `AShield::BlockedDamageMultiplier`; `TakeDamage()` resets that scale on zero-damage/full-block hits to avoid leaking state into the next real hit.
- `UPlayerHUDWidget::SetHealthPercent()` consumes `LastDamageFlashScale` only when health actually drops, so the vignette intensity tracks final post-block damage instead of raw incoming damage.
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
