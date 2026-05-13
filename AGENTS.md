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

No `PrivateDependencyModuleNames`.

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

APlayerController → ACharacterController (Enhanced Input, move/look/jump/equip/attack/arm/sprint/walk/block bindings, input debug snapshot owner)
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

### Shared Direction Helper

- `ABaseCharacter::CalcForwardDot2D(const FVector& WorldDirection)` is the shared 2D facing helper. Callers must pass a world-space direction vector, not a target location.
- Use it for cases like movement-vs-forward, attacker direction, or target direction. Keep `IHitInterface::GetHitDirection()` separate for signed-angle hit-react routing.

### Health / Regen

- `UAttributeComponent` owns shared health/stamina state for both player and enemies; do not enable new behavior globally there unless every user of the component should get it.
- Health regeneration is opt-in: `UAttributeComponent` exposes regen support, but it should stay disabled by default.
- `AMyCharacter` explicitly enables health regeneration in `BeginPlay()`; enemies should not auto-enable it.
- Player regen currently targets **1 HP/s** through `HealthRegenRate`, driven in `TickComponent()` via `AddHealth(...)` and `OnHealthChanged`.

### Enemy AI (`AEnemy`)

- Controlled by `AAIController` through `SetEnemyState(EEnemyState)` — the real flow is Patrol/Search/Chase/Combat, not just chase/attack.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid target returns to patrol, targets inside `CombatingRadius` switch to `EES_Combating`, targets inside `ChasingRadius` switch to `EES_Chasing`.
- `OnSearching()` stops movement, disables orient-to-movement, starts `PatrolTimer` and repeating `LookTimer`, then rotates toward `GenerateNewLookRotation()` using `PatrolRotationSpeed`.
- `ClearPatrolTimers()` must be called when leaving patrol/search states or on death to avoid stale timers firing after state changes.
- `OnChasing()` reissues `MoveToTarget()` if path-following falls back to idle; `OnCombating()` rotates toward the target until `DotProduct > AttackAngleThreshold`, then attacks.
- `EES_Attacking`, `EES_Stunned`, and `EES_Dead` are hard-stop states for Tick-driven AI reactions.

### Health Bar Buffer System

- `UBaseHealthBarWidget` owns two `UProgressBar`: `PB_Health` for the immediate value and `PB_Buffer` for delayed catch-up.
- `SetHealthPercent()` updates `PB_Health` immediately; when health drops it resets `CurrentBufferDelay` using `BufferDelayTime` before `PB_Buffer` starts moving.
- `NativeTick()` lerps `PB_Buffer` toward `PB_Health` with `BufferInterpSpeed`; healing snaps the buffer bar upward immediately instead of animating.
- Shared buffer interpolation math lives in `UBaseHealthBarWidget::TickBufferDelayImpl(...)`; `UPlayerHUDWidget` reuses that helper while keeping player-only `DamageFlash` / `Vignette` behavior local.
- `UHealthBarComponent::SetHealthPercent()` delegates to the widget instance, and `UAttributeComponent::ReceiveDamage()` broadcasts `OnHealthChanged` for UI bindings.
- Enemy health bar visibility is timer-driven by `AEnemy::ShowHealthBar()` / `HideHealthBar()`, with optional Blueprint fade-out through `UBaseHealthBarWidget::PlayFadeOutAnim()`.

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
- Console variables:
  - `test.Debug.Enable` → master text/shape toggle
  - `test.Debug.Enemy` → enemy text toggle
  - `test.Debug.Shapes` → world debug sphere toggle
- Player debug currently includes input snapshot text, HP, stamina, action state, montage name, and movement speed.
- Enemy debug currently includes enemy state / ground speed text plus optional distance and chase/combat radius spheres.

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
