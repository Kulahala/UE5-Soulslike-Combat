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
| `EActionState` | `EAS_UnOccupied`, `EAS_Attacking`, `EAS_Stunning`, `EAS_Exhausted`, `EAS_Parrying`, `EAS_Dodging`, `EAS_UsingPotion`, `EAS_Dead` | `AMyCharacter` |
| `EEnemyState` | `EES_UnOccupied`, `EES_Patrolling`, `EES_Searching`, `EES_Chasing`, `EES_Combating`, `EES_Attacking`, `EES_Stunned`, `EES_StanceBreak`, `EES_Dead` | `AEnemy` |
| `EItemState` | `EIS_Spawning`, `EIS_Dropped`, `EIS_Equipped` | `Aitem` (in `item.h`) |

**Critical: state flow is mixed C++ + montage delegate + `AnimNotify` driven.** Entry states are often set directly in C++ (`Attack()`, `GetHit_Implementation()`, `Die()`, `Dodge()`, `SetEnemyState()`). Recovery commonly uses `FOnMontageEnded` delegates with `bInterrupted` guards, while `AnimNotify` classes handle collision windows and designer-timed recoveries (`UAnimNotifyState_WeaponCollision`, `UAnimNotifyState_DodgeInvulnerable`, `UAnimNotify_SetActionState`, `UAnimNotify_EnemyHitReactEnd`, `UAnimNotify_EnemyAttackEnd`, `UAnimNotify_CharacterHitReactEnd`). Do not hardcode recovery transitions in `Tick()`.

### Montage Helper Boundaries

- `ABaseCharacter::PlayMontageSection(UAnimMontage*, const FName&)` is intentionally a tiny helper that only does `Montage_Play()` + `Montage_JumpToSection()`.
- End-delegate binding stays in the semantic callsites (`PlayAttackMontage()`, `PlayHitReactMontage()`). Do not replace this with a "universal montage entry" unless those recovery semantics genuinely converge.

### Class Hierarchy

```
AActor
├── Aitem (parabolic spawning, floating animation, overlap events)
│   ├── AWeapon (box-trace sweep collision, hit-stop, camera shake, base poise damage)
│   ├── AShield (offhand equip, block angle/damage/stamina/move-speed config, block FX, parry FX)
│   └── ATreasure (gold value, UTreasureData asset)
├── ABreakAbleActor + IHitInterface (StaticMesh → GeometryCollection swap)
├── AArenaGenerator (USplineComponent + UPCGComponent)
└── ABird (APawn subclass, flyable spectator)

ACharacter
├── AMyCharacter + IBlockableInterface (UAttributeComponent, spring arm + camera, weapon/shield equipping, hold-to-block)
└── AEnemy + IHitInterface (AI patrol/search/chase/combat FSM, directional hit react, poise/stance break system)

APlayerController → ACharacterController (Enhanced Input, move/look/jump/equip/attack/sprint/walk/block/lock-on/dodge/parry/potion/pause bindings, pause state management, input debug snapshot owner)
UActorComponent → UAttributeComponent (health, gold, OnHealthChanged delegate)
               └── UPlayerLockOnComponent (lock-on state, target selection, camera tunables)
UWidgetComponent → UHealthBarComponent
UUserWidget → UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars, delayed buffer logic)
           ├── UPlayerHUDWidget (player HP/stamina HUD + NativePaint debug text overlay + damage vignette)
           └── UPauseMenuWidget (Btn_Resume + Overlay_Background, delegate-driven, keyboard resume via NativeOnKeyDown)
UAnimInstance → USlashAnimInstance (GroundSpeed, Direction, state enums)
UDataAsset → UTreasureData
             ├── UComboDataAsset (light attack combo chain configurations, poise damage multiplier per segment)
             └── UAttackConfigDataAsset (LightAttackCombo + TArray<FSpecialAttackConfig> for sprint/jump-style specials + FChargedAttackConfig for charged attack)
UAnimNotifyState → UAnimNotifyState_WeaponCollision
                 ├── UAnimNotifyState_DodgeInvulnerable
                 └── UAnimNotifyState_ComboWindow (opens character combo buffer window)
UAnimNotify → UAnimNotify_PotionHeal (configurable HealPercent, triggers during potion montage)
UInterface → UBlockableInterface (weapon hit interception before final damage application, block/parry result only; no stance-break params)
```

### Combat Pipeline

1. `ACharacterController` binds attack press/release separately: `Started` → `Input_AttackPressed()`, `Completed` / `Canceled` → `Input_AttackReleased()`.
2. Press flow: combo window buffers first; otherwise `AMyCharacter::OnAttackInputPressed()` gives sprint attack priority, then starts the charged-attack decision timer (`ChargeInputThreshold`, default `0.2s`).
3. Release flow: releasing before the threshold clears the timer and calls normal `Attack()`; holding past the threshold enters charge mode, plays `ChargedAttack.Montage` section `Default`, and release jumps to section `Release`.
4. Normal/combo attacks still use `PlayAttackMontage()` with `UAnimNotifyState_WeaponCollision`
5. **NotifyBegin** → `AWeapon::StartWeaponTrace()` (records old box positions) + `SetAttackHyperArmor(true)` (player only)
6. **NotifyTick** → `AWeapon::ExecuteWeaponTrace()` (sweep old→new center)
7. On hit:
   - shared tags between attacker and target mean **no `ApplyDamage`**, but the target still enters the shared hit-feedback path (`GetHit`, knockback, hit-stop, camera shake)
   - cross-team hits may be intercepted by `IBlockableInterface::TryBlockHit()` before final damage is applied
   - for `ABaseCharacter` targets, `AWeapon::ExecuteWeaponTrace()` writes a per-hit `FPendingHitContext` (instigator, knockback scale, blocked flag, stun flag) before calling `IHitInterface::GetHit()`
   - `ABaseCharacter::GetHit_Implementation()` consumes that context for knockback / normal hit-react routing, and leaf classes (`AMyCharacter`, `AEnemy`) clear it after their own stun logic runs
   - **Poise damage is applied in `DispatchHitFeedback()`** (not `ResolveHit()`), following a deferred trigger pattern to avoid state conflicts:
     1. `ApplyPoiseDamage()` reduces enemy poise and sets `bPendingStanceBreak` flag if poise reaches zero
     2. `GetHit()` executes normally → enemy enters `EES_Stunned`
     3. After `GetHit()`, `ShouldTriggerStanceBreak()` is checked → `ApplyStanceBreak()` overrides to `EES_StanceBreak` + slow-motion hit react
   - **Parry poise damage targets the attacker** (`GetOwner()`), not `HitActor`: parry occurs when an enemy weapon hits the player, so `HitActor` is the player. `Cast<AEnemy>(HitActor)` would fail; `Cast<AEnemy>(GetOwner())` correctly targets the attacking enemy
   - `AWeapon::ExecuteWeaponTrace()` is intentionally decomposed into `BuildIgnoreList()`, `ResolveHit()`, and `DispatchHitFeedback()`. Keep `ResolveHit()` focused on same-team/block/damage math, and keep `DispatchHitFeedback()` focused on context write / poise damage / `GetHit()` / stance break check / camera shake / hit stop / ignore list; do not inflate it into a generic combat pipeline without a real new use case
8. **NotifyEnd** → clears the weapon hit ignore list through `ClearIgnoreActors()` + `SetAttackHyperArmor(false)` (player only)
9. `OnAttackMontageEnded` delegate (with `bInterrupted` guard that restores `ActionState`) → clears charge input state, restores `EAS_UnOccupied` when appropriate, and resumes stamina regen

### Hit Knockback

- `ABaseCharacter` owns the shared short-distance weapon-hit knockback system through `FPendingHitContext`, `BaseHitKnockbackDistance`, `HitKnockbackDuration`, and `TickHitKnockback()`.
- Knockback is intentionally **weapon-hit feedback**, not generic damage feedback: traps / DOT that only call `TakeDamage()` do not automatically trigger it.
- Current defaults are constructor-set per class: `AMyCharacter` uses **10 cm**, `AEnemy` uses **5 cm**.
- Motion is explicit **quadratic ease-out**, applied in `Tick()` through `AddActorWorldOffset(..., true, &Hit)` so the capsule sweeps and can be blocked by walls.
- A new hit always overrides the previous knockback state. A zero-scale hit (for example full block) clears any in-flight knockback instead of letting the old motion continue.
- Successful blocks scale knockback by final post-block damage ratio (`DamageAfterBlock / Damage`), so a 95% reduction produces a very short pushback.
- Friendly weapon hits are intentionally allowed to trigger knockback and hit feedback even though they do not deal damage.

### Attack Hyper Armor System

- **Player-only feature**: During the weapon collision window (`AnimNotifyState_WeaponCollision`), `AMyCharacter` gains hyper armor.
- **Lifecycle**: `NotifyBegin` calls `SetAttackHyperArmor(true)`, `NotifyEnd` calls `SetAttackHyperArmor(false)`, synchronized with weapon trace window.
- **Hyper armor effect**: Incoming hits still deal damage, apply knockback, trigger camera shake, and play hit effects, but do **not** play hit-react montage or force `EAS_Stunning` state. Attack animation continues uninterrupted.
- **Implementation**: `AMyCharacter::GetHit_Implementation()` checks `bAttackHyperArmor` before calling `Super`. Hyper armor branch manually replicates necessary logic (`IHitInterface::GetHit`, `ConsumePendingHitKnockback()`, `PlayHitEffects()`, camera shake) while skipping `Super::GetHit_Implementation()` to avoid triggering `DirectionalHitReact()` → `PlayHitReactMontage()`.
- **Interrupt recovery**: `OnAttackMontageEnded(bInterrupted=true)` ensures `ActionState` is restored to prevent being stuck in `EAS_Attacking`.
- **Priority**: Dodge invulnerability (`bDodgeInvulnerable`) has higher priority than hyper armor (complete immunity vs partial immunity).
- **Extensibility**: Currently player-only. Future expansion: move `bAttackHyperArmor` to `ABaseCharacter` + add `virtual bool ShouldUseHyperArmor()` hook for Boss-type enemies.
- **Files**: `MyCharacter.h:219` (member), `MyCharacter.cpp:272-329` (GetHit logic), `AnimNotifyState_WeaponCollision.cpp:27-30, 65-68` (lifecycle).

### Poise / Stance Break System

- **Unified mechanism**: Parry and combo-based poise depletion both trigger `EES_StanceBreak` — there is no separate `EES_Parried` state. Parry = instant poise clear, combo = gradual poise reduction, both converge on the same `ApplyStanceBreak()`.
- **Poise damage formula**: `Final Poise Damage = Weapon::BasePoiseDamage × PoiseDamageMultiplier`. For combo attacks, the multiplier comes from `ComboSegment::PoiseDamageMultiplier`; for sprint attacks, from `FSpecialAttackConfig::PoiseDamageMultiplier` via `AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack)`; for charged attacks, it is interpolated from `1.f` to `FChargedAttackConfig::MaxPoiseDamageMultiplier`. `CurrentPoiseDamage` is set before the weapon collision window and read by `DispatchHitFeedback()`.
- **Deferred trigger pattern** (`DispatchHitFeedback()` execution order):
  1. `ApplyPoiseDamage(Damage, Instigator)` reduces `CurrentPoise` and sets `bPendingStanceBreak` flag if poise reaches zero — does **not** immediately trigger stance break
  2. `CachePendingHitContext()` + `GetHit()` → enemy enters `EES_Stunned` with normal hit react
  3. `ShouldTriggerStanceBreak()` checked → `ApplyStanceBreak()` overrides to `EES_StanceBreak` + slow-motion hit react
  4. Visual result: brief normal hit → transition to slow-motion stance break (natural feel)
- **Parry path**: In `DispatchHitFeedback()`, parry poise damage targets `GetOwner()` (the attacking enemy), not `HitActor` (the player). `ShouldTriggerStanceBreak()` is also checked on `GetOwner()` for parry. **Stance break parameters are unified** — both parry and normal poise break use `Enemy->GetStanceBreakDuration()` / `Enemy->GetStanceBreakPlayRate()` (enemy's own parameters).
- **Normal combo path**: `ShouldTriggerStanceBreak()` is checked on `HitActor` (the enemy). Uses `Enemy->GetStanceBreakDuration()` / `Enemy->GetStanceBreakPlayRate()` from enemy config.
- **Parameter source**: Shield no longer holds `StanceBreakDuration/PlayRate`. `FBlockResult` and `FWeaponHitResult` no longer pass these fields. All stance breaks use enemy parameters, allowing different enemy types to have different stance break characteristics.
- **`ApplyPoiseDamage()` guards**: Early-returns on `EES_Dead` and `EES_StanceBreak`. Records `LastPoiseDamageInstigator` for directional hit react in `ApplyStanceBreak()`.
- **`ApplyStanceBreak()` behavior**: Clears `bPendingStanceBreak`, calls `ResetPoise()` (immediate poise refill), stops current montage, sets `EES_StanceBreak`, plays `DirectionalHitReact` from `LastPoiseDamageInstigator`, applies slow `PlayRate`, starts `StanceBreakRecoveryTimer`.
- **Poise reset**: `PoiseResetTimer` (default `5.f` seconds, `PoiseResetDelay`) resets `CurrentPoise = MaxPoise`. Timer is cleared and restarted on each `ApplyPoiseDamage()`. Not started when poise hits zero (stance break handles it).
- **Recovery**: `RecoverFromStanceBreak()` restores montage play rate to `1.f` and delegates to `CheckCombatTarget()` for state transition.
- **Death cleanup**: `Die()` clears `bPendingStanceBreak`, `LastPoiseDamageInstigator`, `PoiseResetTimer`, `StanceBreakRecoveryTimer`.
- **Debug**: `DrawDebugInfo()` shows `Poise: X.X/Y.Y` in cyan; `"BREAK"` in red when in `EES_StanceBreak`.
- **Config on `AEnemy`** (all `EditAnywhere` under `"Combat|Poise"`):
  - `MaxPoise` — default `10.f`
  - `PoiseResetDelay` — default `5.f` (seconds)
  - `StanceBreakDuration` — default `2.f` (seconds)
  - `StanceBreakPlayRate` — default `0.3f`
- **Config on `AWeapon`** (protected, `EditAnywhere` under `"Combat|Poise"`):
  - `BasePoiseDamage` — default `1.f`; accessed via `GetBasePoiseDamage()`
- **Config on `AMyCharacter`** (`EditDefaultsOnly` under `"Combat"`):
  - `AttackConfig` — `UAttackConfigDataAsset*`, unified attack configuration asset
- **Implementation files**: `Source/Test/Public/Enemy/Enemy.h` (poise API/getters + private params), `Source/Test/Private/Enemy/Enemy.cpp` (poise/stance-break implementation), `Source/Test/Public/Items/Weapon/Weapon.h` (`BasePoiseDamage` + `GetBasePoiseDamage()`), `Source/Test/Private/Items/Weapon/Weapon.cpp` (`DispatchHitFeedback` poise/stance-break logic), `Source/Test/Public/Combat/ComboDataAsset.h` (`PoiseDamageMultiplier`), `Source/Test/Public/Combat/AttackConfigDataAsset.h` (`ESpecialAttackType` + `FSpecialAttackConfig`), `Source/Test/Public/Character/BaseCharacter.h` (`CurrentPoiseDamage` + getter), `Source/Test/Public/Character/MyCharacter.h` (`AttackConfig`), `Source/Test/Public/Items/Shield/Shield.h` (no stance-break params), `Source/Test/Public/Interfaces/BlockableInterface.h` (`FBlockResult` has no stance-break fields).

### Block System

- `AMyCharacter` implements `IBlockableInterface` and owns hold-to-block state through `bBlockInputHeld` + `bIsBlocking`.
- `ACharacterController` binds `BlockAction` start/end to `StartBlockInput()` / `ReleaseBlockInput()`.
- `AShield` is equipped to the offhand via `EquipToOffhand()` and provides block tuning values:
  `BlockHalfAngleDegrees`, `BlockedDamageMultiplier`, `BlockStaminaCostPerDamage`, `BlockMoveSpeedMultiplier`.
  Parry-specific params: `ParryStaminaCost` (default `15.f`), `ParryCooldown` (default `0.4f`), `ParrySound`, `ParryParticle`.
- `AShield` parameters are private `UPROPERTY` values with `AllowPrivateAccess`; gameplay code reads them through `GetBlock*()`, `GetParry*()`, and `GetOffhandSocketName()` accessors.
- `AMyCharacter::CanStartBlock()` is intentionally independent of weapon equip state. Current block-start gates are: shield equipped, `EAS_UnOccupied`, and grounded.
- `AWeapon::ExecuteWeaponTrace()` checks `IBlockableInterface` on the hit actor before final damage application.
- Successful blocks reduce or redirect damage through `FBlockResult`, suppress shared `DirectionalHitReact()` / `PlayHitEffects()`, and play shield-specific sound/particle feedback.
- Blocked hits still flow through `GetHit` via `FPendingHitContext`, so scaled knockback and class-specific feedback can still happen even when normal hit react is suppressed.
- Blocking is canceled when shield/state/falling conditions become invalid; blocked movement speed is reduced through `UpdateMovementSpeed()`.
- When tuning block damage reduction, update both the C++ default (`AShield::BlockedDamageMultiplier`, read through `GetBlockedDamageMultiplier()`) and the actual `BP_Shield` asset if it overrides the value in Blueprint.

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

### Potion System

- `ACharacterController` binds `UsePotionAction` (R key) → `Input_UsePotion()` → `AMyCharacter::UsePotion()`.
- **Entry guard** (`CanUsePotion()`): allows `EAS_UnOccupied` **or** `EAS_Exhausted`, grounded, has potion (`UAttributeComponent::HasPotion()`), not on cooldown, HP < 100%.
- **Execution** (`UsePotion()`): consumes one potion via `Attributes->UsePotion()` → `StopSprinting()` → two paths:
  - **Montage path** (if `PotionMontage != nullptr`): sets `EAS_UsingPotion` → `PlayPotionMontage()` → `EmitNoise()`. `UAnimNotify_PotionHeal` triggers `HealFromPotion(Percent)` at designer-placed keyframes (default 25% per notify, two notifies for 50% total).
  - **Fallback path** (no montage): immediately calls `HealFromPotion(0.5f)` for 50% heal, then `StartPotionCooldown()`.
- **Heal guard** (`HealFromPotion()`): if `PotionMontage != nullptr`, requires `ActionState == EAS_UsingPotion` to prevent stale AnimNotify healing after interrupt. Fallback path bypasses this guard.
- **Interrupt** (`InterruptPotion()`): `Montage_Stop(0.1f)` → `ActionState = EAS_UnOccupied` → `StartPotionCooldown()`. Called from `GetHit_Implementation()` (before hyper armor check) and `Die()`.
- **Cooldown**: `bPotionOnCooldown` + `FTimerHandle PotionCooldownTimer` with `PotionCooldown` (default `2.f`). Started in `OnPotionMontageEnded`, `InterruptPotion`, and fallback path.
- **Montage end** (`OnPotionMontageEnded`): `bInterrupted` early-returns (delegated to `InterruptPotion`). State guard checks `EAS_UsingPotion`. Checks `IsExhaustionTimerActive()` — if exhaustion is active, reverts to `EAS_Exhausted` rather than `EAS_UnOccupied` (consistent with attack/dodge/parry recovery patterns). Then starts cooldown.
- **Stamina**: Potion does **not** call `PauseStaminaRegen()` / `ResumeStaminaRegen()`. Stamina regenerates naturally during the drinking animation (Souls-like design where stamina recovers while drinking).
- **`HandleExhausted()`**: early-returns if `ActionState == EAS_UsingPotion` — stamina depletion does not interrupt the potion animation.
- **Movement**: `UpdateMovementSpeed()` early-returns with `WalkSpeed` when `ActionState == EAS_UsingPotion`, skipping `TickSprintStamina()` and directional speed calculation.
- **`Input_Move()` gate**: `CharacterController.cpp` must allow `EAS_UsingPotion` alongside `EAS_UnOccupied` and `EAS_Exhausted` — otherwise the player cannot move while drinking.
- **`Jump()`**: blocks during `EAS_UsingPotion` via early-return guard.
- **Noise**: emits `PotionNoiseLoudness` (default `0.5`) with `PotionNoiseRange` (default `500.f` cm) at montage play time, notifying nearby enemies through the hearing perception system.
- **HUD**: `UPlayerHUDWidget::Text_PotionCount` (`UTextBlock`, `BindWidget`) displays `"Current/Max"` format. Bound via `OnPotionCountChanged` delegate.
- **Debug**: `DrawDebugInfo()` shows `"Potion: 3/3 [CD: 1.2s]"`; `GetDebugInputText()` shows `"Potion"` marker; `ActionStateNames[]` includes `"UsingPotion"`.
- **Attribute storage** (`UAttributeComponent`): `CurrentPotionCount` / `MaxPotionCount` (both `EditAnywhere`, default `3`), `PotionHealPercent` (default `0.5`), `FOnPotionCountChanged` delegate. Methods: `HasPotion()`, `UsePotion()` (decrements + broadcasts), `AddPotion(Amount)`, `SetPotionCount(Count)`, `GetPotionCount()`, `GetMaxPotionCount()`.
- **Config** (all `EditDefaultsOnly`/`EditAnywhere` on `AMyCharacter`):
  - `PotionCooldown` — default `2.f` (seconds)
  - `PotionMontage` — optional cooking animation montage asset
  - `PotionNoiseLoudness` — default `0.5f`
  - `PotionNoiseRange` — default `500.f` (cm)
- **Implementation files**: `Source/Test/Public/Character/CharacterTypes.h:19` (enum), `Source/Test/Public/AttributeComponent/AttributeComponent.h:81-92,124-129` (storage), `Source/Test/Public/Character/MyCharacter.h:63-67,226-236,292-296` (declaration), `Source/Test/Private/Character/MyCharacter.cpp:790-891` (implementation), `Source/Test/Public/AnimNotify/AnimNotify_PotionHeal.h` (notify), `Source/Test/Public/HUD/PlayerHUDWidget.h:36-37` (HUD), `Source/Test/Private/Character/Controller/CharacterController.cpp:77-80,271-278` (input).

### Combo System

- **Data-Driven Configuration**: A light attack combo chain is defined via `UComboDataAsset`. Each segment configures a custom Montage Section, a damage multiplier (scales base weapon damage), a stamina cost, and a poise damage multiplier (scales weapon base poise damage).
- **Unified Attack Config**: `AMyCharacter` uses `UAttackConfigDataAsset` (referenced as `AttackConfig` member) to manage attack data. `AttackConfigDataAsset` references `LightAttackCombo` (`UComboDataAsset`), contains a `TArray<FSpecialAttackConfig>` for sprint / future jump-style special attacks, and owns a dedicated `FChargedAttackConfig ChargedAttack`. Each `FSpecialAttackConfig` has a `Type` enum (`ESpecialAttackType`), `Montage`, `DamageMultiplier`, `PoiseDamageMultiplier`, and `StaminaCost`. Lookup is via `AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack)`.
- **No backward compat layer**: Old fields (`LightAttackCombo`, `SprintAttackMontage`, `SprintAttackDamageMultiplier`, `SprintAttackStaminaCost`, `SprintAttackPoiseDamageMultiplier`) have been **removed**. `AttackConfig` is the single source of truth. `BeginPlay()` has `ensureMsgf(AttackConfig, ...)` guard.
- **Input Buffering**: Checked during `ACharacterController::Input_AttackPressed()`. If `AMyCharacter::IsComboWindowOpen()` is true, the input is buffered by setting `bComboInputReceived = true` before any charged-attack timer is started. Otherwise press/release handling decides sprint, normal, or charged attack.
- **Combo Window State**: Controlled by `UAnimNotifyState_ComboWindow` placed in the attack montage. It calls `OpenComboWindow()` and `CloseComboWindow()` to toggle the input window.
- **Combo Progression**: In `AMyCharacter::OnAttackMontageEnded()`, if `bComboInputReceived` is true and a next combo segment exists, the character increases the combo counter, temporarily marks the action state as `EActionState::EAS_UnOccupied` (to pass the `CanAttack()` gate), and immediately triggers `Attack()`.
- **Reset Guards & Interruption**: On attack finish (without buffered inputs), normal interruption (getting hit, death, dodge roll, or exhaustion), `ResetCombo()` is called to reset the combo counter, input flag, and restore the damage multiplier to `1.0f` and poise damage to `EquippedWeapon->GetBasePoiseDamage()` (or `1.f` fallback if no weapon equipped).
- **Damage Multiplier Application**: Applied in `AWeapon::ResolveHit()`. The weapon queries the attacker's `GetAttackDamageMultiplier()` to scale the raw damage *before* passing it to `IBlockableInterface::TryBlockHit()`, ensuring block stamina costs and damage mitigation calculations scale accordingly.
- **Poise Damage Multiplier Application**: For combo attacks, `CurrentPoiseDamage = EquippedWeapon->GetBasePoiseDamage() * Segment.PoiseDamageMultiplier` is set in `Attack()` before montage play. For sprint attacks, `CurrentPoiseDamage = EquippedWeapon->GetBasePoiseDamage() * SprintConfig->PoiseDamageMultiplier` via `AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack)`. For charged attacks, `PerformChargedRelease()` interpolates from `1.f` to `ChargedAttack.MaxPoiseDamageMultiplier`. The final value is read by `DispatchHitFeedback()` when calling `Enemy->ApplyPoiseDamage()`.
- **Debug Visibility**: Rendered via `FDebugDrawHelper` in `DrawDebugInfo` when master debug rendering is active, printing combo index, input/window status, and current damage multiplier.

### Sprint Attack System

- Triggered via `ACharacterController::Input_AttackPressed()` → `AMyCharacter::OnAttackInputPressed()` / `Attack()` → `ShouldUseSprintAttack()` → `PerformSprintAttack()`.
- **Priority**: sprint attack is checked **before** `CanAttack()` and the normal combo flow. Guard order: `bIsBlocking` → `ShouldUseSprintAttack()` → `CanAttack()` → combo logic.
- **Conditions** (`ShouldUseSprintAttack()`): `bIsSprinting`, movement input (`GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER`), weapon equipped (`WeaponState != EWS_Unequipped`), `EAS_UnOccupied`, grounded. No lock-on restriction — works in both lock-on and non-lock states.
- **Reuses `EAS_Attacking`**: no new `EActionState` enum value. No `bIsSprintAttack` flag — `OnAttackMontageEnded()` handles cleanup correctly without one: sprint attacks never open a combo window, so `bShouldContinueCombo` is always false, and `ResetCombo()` + `RestoreRotationMode()` + exhaustion check all work as-is.
- **Independent montage**: separate from the combo montage to avoid pollution. Requires `AnimNotifyState_WeaponCollision` for weapon trace windows. End delegate reuses `OnAttackMontageEnded()`.
- **Configuration**: Managed via `AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack)`. Returns `FSpecialAttackConfig` containing `Montage`, `DamageMultiplier`, `PoiseDamageMultiplier`, `StaminaCost`. No fallback to deprecated fields — `AttackConfig` must be set on `BP_MyCharacter`.
- **Replaces lock-on free-run attack**: the old `ShouldUseLockOnFreeRun()` + `FaceDirection2D()` + `SetMovementRotationMode()` branch in `Attack()` is removed. `ShouldUseLockOnFreeRun()` itself is preserved — it's still needed for lock-on sprint **movement** behavior (speed, rotation mode, camera in `UpdateMovementSpeed()`, `ApplyLockOnRotationMode()`, `GetLockOnCameraTargets()`).
- **Allows stamina overspend**: uses `Attributes->UseStamina()` directly with no `CheckStamina()` gate, consistent with the "last action overspend" design shared by dodge, parry, and normal attacks.
- **Clears combo state**: calls `ResetCombo()` at the start of `PerformSprintAttack()` before setting damage multiplier, so stale combo state from a prior attack cannot contaminate the sprint hit.
- **Direction with fallback**: faces `GetLastMovementInputVector().GetSafeNormal2D()` direction, falling back to `GetActorForwardVector()` if input is nearly zero (defensive pattern borrowed from `ComputeDodgeDirection()`).
- **Rotation lock**: sets `bOrientRotationToMovement = false` and `bUseControllerRotationYaw = false` during the attack; `OnAttackMontageEnded()` calls `RestoreRotationMode()` to recover.
- **Stops sprinting**: calls `StopSprinting()` after rotation lock and before montage play, so the character returns to normal run speed when the attack montage ends.
- **Not in combo system**: sprint attack does not use `AnimNotifyState_ComboWindow` and does not advance `ComboCounter`. It is a single-hit attack that ends with `EAS_UnOccupied`.
- **Default values** (in `DA_AttackConfig` asset): SprintAttack `DamageMultiplier = 1.8`, `StaminaCost = 25`, `PoiseDamageMultiplier = 2.0`.
- **Implementation files**: `Source/Test/Public/Combat/AttackConfigDataAsset.h` (ESpecialAttackType + FSpecialAttackConfig + FChargedAttackConfig + UAttackConfigDataAsset), `Source/Test/Private/Combat/AttackConfigDataAsset.cpp` (FindSpecialAttack + PostInitProperties), `Source/Test/Public/Character/MyCharacter.h` (AttackConfig declaration), `Source/Test/Private/Character/MyCharacter.cpp` (Attack, PerformSprintAttack, OnAttackMontageEnded, PlayAttackMontage, BeginPlay).

### Charged Attack System

- Triggered via attack input hold/release: `Input_AttackPressed()` starts the path and `Input_AttackReleased()` resolves short press vs charged release.
- **Priority**: combo window buffering happens first in the controller; sprint attack has priority over charged attack in `OnAttackInputPressed()`. Charged attack uses the same entry guards as a grounded weapon attack: not blocking, `EAS_UnOccupied`, weapon equipped, and not falling.
- **Short press behavior**: releasing before `ChargeInputThreshold` (default `0.2s`, kept on `AMyCharacter` for input feel only) clears the decision timer and falls back to normal `Attack()`.
- **Configuration**: `AttackConfig->ChargedAttack` is a dedicated `FChargedAttackConfig`, separate from `SpecialAttacks`. It owns `Montage`, `MaxDamageMultiplier`, `MaxPoiseDamageMultiplier`, `StaminaCost`, `MinChargeHoldTime`, and `MaxChargeHoldTime`. If the charged montage is missing, charge entry cancels and falls back to normal attack.
- **Charge scaling**: `PerformChargedRelease()` calculates held time, clamps alpha between `MinChargeHoldTime` and `MaxChargeHoldTime`, then interpolates damage and poise multipliers from `1.f` to the configured max values. If `MaxChargeHoldTime <= MinChargeHoldTime`, alpha stays `0.f` rather than dividing by an invalid range.
- **Montage contract**: C++ hard-codes section names `Default` and `Release`. A typical montage link setup is `Default -> Loop`, `Loop -> Loop`, `Release -> None`; UE Montage section markers are **start markers**, not end markers, so `Release` can bound the loop and still only be entered by `Montage_JumpToSection("Release")`.
- **Root motion / notify contract**: The hold or loop section should use an in-place/no-root-motion hold pose or tiny static clip; the `Release` section may keep root motion for the forward step. Put `UAnimNotifyState_WeaponCollision` on `Release`, not on the charge hold loop. Do not put `UAnimNotifyState_ComboWindow` in the charged montage.
- **State/recovery**: charged attack reuses `EAS_Attacking` and `OnAttackMontageEnded()`. End/death paths call `CancelChargeInputState()`. Stamina is consumed on release; exhaustion after release is carried through `bPendingExhaustedAfterAttack` so recovery can enter `EAS_Exhausted` instead of incorrectly returning to idle.
- **Implementation files**: `Source/Test/Public/Combat/AttackConfigDataAsset.h`, `Source/Test/Private/Combat/AttackConfigDataAsset.cpp`, `Source/Test/Public/Character/Controller/CharacterController.h`, `Source/Test/Private/Character/Controller/CharacterController.cpp`, `Source/Test/Public/Character/MyCharacter.h`, `Source/Test/Private/Character/MyCharacter.cpp`.

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
- **Sprint Attack replaces free-run attack**: the old free-run attack branch in `Attack()` (which played a normal combo segment at movement direction) has been removed. Sprinting + attack now triggers `PerformSprintAttack()` regardless of lock-on state, with montage and parameters from `AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack)`. `ShouldUseLockOnFreeRun()` is still used for lock-on sprint **movement** (speed, rotation mode, camera offset).
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
- Current base speeds are walk `200`, run `300`, sprint `360`. Non-lock movement is treated as free movement and passes a forward dot into `CalcBaseSpeed()` so sprint intent can apply without fake side/back penalties.
- Ordinary lock-on movement uses directional speed interpolation from forward `1.0` through `LockOnStrafeSpeedMultiplier` to `LockOnBackSpeedMultiplier`; current defaults are strafe `0.95` and back `0.9`.
- Lock-on Sprint free-run bypasses lock-on directional slowdown: any movement-input direction can reach sprint speed while the locked camera remains on the enemy.
- The state multiplier is chosen before directional scaling: blocking uses `EquippedShield->GetBlockMoveSpeedMultiplier()`, otherwise `1.0f`. `EAS_UsingPotion` early-returns with `WalkSpeed` before `TickSprintStamina()` and directional scaling. Being armed / holding a weapon does not reduce normal movement speed by itself.
- `TickSprintStamina()` drains stamina while grounded, unblocked, and moving. In ordinary lock-on combat step it still uses the forward-dot gate, while lock-on free-run bypasses that gate so side/back sprint consumes stamina.

### Enemy AI (`AEnemy`)

- Controlled by `AAIController` through `SetEnemyState(EEnemyState)` — the real flow is Patrol/Search/Chase/Combat, not just chase/attack.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid target returns to patrol, non-combat states enter `EES_Combating` inside `CombatingRadius`, and combat-family states (`EES_Combating`, `EES_Attacking`, `EES_Stunned`, `EES_StanceBreak`) use `CombatingRadius + CombatExitBuffer` as their chase fallback threshold before dropping to `EES_Chasing`.
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
- `EES_Attacking`, `EES_Stunned`, `EES_StanceBreak`, and `EES_Dead` are hard-stop states for Tick-driven AI reactions.
- `WeaponInit()` treats `WeaponClass == nullptr` as an intentional unarmed enemy, but if `WeaponClass` is set and `SpawnActor<AWeapon>()` fails it must log and return before `Equip()`. Do not dereference spawned runtime actors without a null guard even when the class config looks valid.

### Hearing Perception System

Enemies use `UAISenseConfig_Hearing` alongside the existing `UAISenseConfig_Sight` to detect player noises. Hearing is **event-driven** (not continuous like sight), and all stimuli route through the same `TargetPerceptionUpdated` callback.

**Enemy configuration** (`AEnemy::AEnemy()`, `Enemy.cpp:59-65`):
- `UAISenseConfig_Hearing` with `HearingRange = 800.f` (`Enemy.h:131`)
- `bDetectEnemies = true`, `bDetectNeutrals = false`, `bDetectFriendlies = false`
- `SetDominantSense(SightConfig)` is preserved — hearing is auxiliary, not the primary sense
- Hearing stimuli trigger the same `TargetPerceptionUpdated` path as sight, which writes `ChasingTarget` and lets `CheckCombatTarget()` drive the state transition

**Noise emission** (`AMyCharacter`, `MyCharacter.cpp:1100-1168`):

| Source | Loudness | Range (cm) | Trigger |
|--------|----------|------------|---------|
| Walk (Alt) | 0.0 | — | Silent (stealth mode) |
| Run (default) | 0.4 | 500 | Continuous every `MovementNoiseInterval` (0.5s) |
| Sprint (Shift) | 0.6 | 600 | Continuous every `MovementNoiseInterval` + immediate burst on sprint start |
| Attack | 1.0 | 800 | Single event at montage play time (both combo and sprint attack) |
| Dodge | 0.4 | 400 | Single event at montage play time |
| Potion | 0.5 | 500 | Single event at montage play time |

**Architecture**:
- **Emission**: `EmitNoise(Loudness, MaxRange)` calls `UAISense_Hearing::ReportNoiseEvent()` + `FDebugDrawHelper::AddNoiseRange()`
- **Movement timer**: Owned by `ACharacterController` via `AMyCharacter::StartMovementNoiseTimer()` / `StopMovementNoiseTimer()`. Timer starts in `Input_Move()` (before action-state guard, so stunned characters keep the timer pre-warmed), stops in `Input_MoveEnd()`.
- **Instant bursts**: `Sprint()` emits immediately on start (with velocity guard); `Attack()` and `Dodge()` emit single events at montage play time
- **Silent walk**: `EmitMovementNoise()` early-returns when `bIsWalking` is true
- **Air silence**: `EmitMovementNoise()` early-returns on `IsFalling()`
- **Velocity guard**: `EmitMovementNoise()` early-returns when `Speed2D < 10.f`
- **Cleanup**: Timer stopped in `GetHit_Implementation()` (stun path), `Dodge()`, `Die()`. On exhaustion, `bIsSprinting` is cleared and immediate noise reflects the downgraded speed.

**All noise parameters are `EditAnywhere` `UPROPERTY` on `AMyCharacter`** under `"Combat|Hearing"`, tunable in Blueprint. Debug visualization: orange sphere via `test.Debug.Shapes`, 0.5s lifetime.

**V1 behavior**: Hearing writes `ChasingTarget` directly (enemy chases player position, not noise position). The "后续扩展" section in plan.md covers future work: Stimulus-type distinction to route hearing to `EES_Searching` with `LastKnownLocation`, AnimNotify-driven precise attack sounds, and `ABaseCharacter` extraction of `EmitNoise`.

### Attack Coordination System

Prevents multiple enemies from attacking simultaneously for better gameplay feel. Uses `UGameplayStatics::GetAllActorsOfClass` traversal with distance + same-target + state filtering.

- **Detection**: `AEnemy::IsAllyAttackingNearby(float& OutSuggestedWaitTime)` traverses all `AEnemy` actors and returns true only when another enemy is within `AttackCoordinationRange`, is in `EES_Attacking`, and has the same `ChasingTarget`.
- **Wait strategy**: Coordination waits use this enemy's `AttackCoordinationBuffer` (0.5s default), capped by `MaxAttackCoordinationWait` (3s default). Do not read the attacking ally's `AttackCooldownTimer`; that timer represents the full attack interval, not attack animation/state remaining time, and makes combat feel too passive.
- **Entry points**:
  - `OnCombating()` checks coordination both before the ready-to-attack `Attack()` path and in the attack-ready positioning path. All real attack starts must pass this coordination gate.
  - `OnAttackCooldownEnd()` re-checks before the original CD expiry logic runs, preventing a same-frame window where two enemies' cooldowns could expire simultaneously and both attack.
- **Interruption recovery**: If the attacking ally is stunned/stance-broken and exits `EES_Attacking`, the waiting enemy's next `IsAllyAttackingNearby()` call finds no attacking ally and proceeds to attack immediately. Blocked/damage flags apply normally — the waiting enemy is still in `EES_Combating` and can be hit.
- **Forced attack cap**: `MaxAttackCoordinationWait` (3s) prevents infinite waiting if an ally gets stuck in `EES_Attacking` state (e.g., unusually long montage).
- **`ShouldTriggerAttack()` remains `const`**: The coordination logic lives in the callers (`OnCombating()` and `OnAttackCooldownEnd()`), not inside the predicate, preserving the existing extension seam for derived enemy classes.
- **Debug**: Yellow `"WaitAlly"` text via `FDebugDrawHelper` when coordination is active, visible when `test.Debug.Enemy` is enabled.
- **Config** (all `EditAnywhere` on `AEnemy` under `"Combat|Attack Coordination"`):
  - `AttackCoordinationRange` — default `800.f` (cm)
  - `AttackCoordinationBuffer` — default `0.5f` (seconds)
  - `MaxAttackCoordinationWait` — default `3.f` (seconds)
- **Implementation files**: `Source/Test/Public/Enemy/Enemy.h` (parameters + declaration), `Source/Test/Private/Enemy/Enemy.cpp` (`IsAllyAttackingNearby`, `OnCombating`, `OnAttackCooldownEnd`)
- **Future work**: AI Perception Team-based sensing for large scenes, priority system for elite enemies, attack slot system allowing 2+ simultaneous attackers, attack mode toggle (coordinated / free).

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
- `UPlayerHUDWidget::BindToAttributes()` immediately pushes current health/stamina/potion values after binding, so UI initialization does not wait for the next attribute change event.
- `Text_PotionCount` (`UTextBlock`) displays potion count in `"Current/Max"` format, bound via `Attributes->OnPotionCountChanged`.

### Player Hit Feedback

- Player hurt feedback is split intentionally: **camera shake = hit reaction**, **red vignette = health loss**.
- `AMyCharacter::GetHit_Implementation()` triggers `HitReceivedCameraShake` for the local player whenever the weapon-hit feedback path reaches `GetHit`, including blocked hits and same-team weapon hits.
- `AMyCharacter::TryBlockHit()` writes `LastDamageFlashScale` from `EquippedShield->GetBlockedDamageMultiplier()`; `TakeDamage()` pushes that scale into `UPlayerHUDWidget::SetPendingDamageFlashScale(...)` before damage application, then immediately resets the local value so hits cannot leak state across frames.
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
- Player debug currently includes input snapshot text, HP, stamina, potion count/cooldown, action state, montage name, and movement speed.
- Player debug currently includes the short-lived `LockOn`, `Potion`, and other action markers from controller input snapshots, in addition to the held-state text.
- Enemy debug currently includes enemy state / ground speed text, distance text, optional chase/combat/attack radius spheres, poise display (`Poise: X.X/Y.Y` in cyan, `"BREAK"` in red during stance break), and `CombatMove: Ready/Retreat/BackDiag/Strafe/Press/AlreadyAtGoal/MoveFail` while in `EES_Combating`.

### Pause Menu System

- `ACharacterController` owns pause state: `bIsPaused`, `bCanPause`, `TogglePause()`, `ClearPauseIfActive()`.
- `PauseAction` input action (`Content/_GAME/BP/input/IA_Pause.uasset`) bound to P key in `IMC_CharacterInput`.
- `Input_Pause()` → `TogglePause()`: pauses game via `SetGamePaused(true)`, switches to `FInputModeUIOnly`, shows `UPauseMenuWidget`.
- `UPauseMenuWidget` (`Source/Test/Public/HUD/PauseMenuWidget.h:19`) communicates via delegate: `FOnResumeRequested OnResumeDelegate` bound by Controller in `BeginPlay()`.
- Controller creates Widget via `TSubclassOf<UPauseMenuWidget> PauseMenuClass` (EditDefaultsOnly) so Blueprint subclass (`WBP_PauseMenu`) takes effect.
- Keyboard resume: Widget's `NativeOnKeyDown` listens for P/Escape keys and broadcasts `OnResumeDelegate`.
- Smart lock-on handling on pause: if `IsValid(LockedTarget)` fails → `ClearLockOn()`; if target valid but rotation incomplete (yaw delta ≥ 1°) → `ClearLockOn()`; otherwise keep lock.
- `AMyCharacter::Die()` clears pause first: `ClearPauseIfActive()` + `SetCanPause(false)` before death montage.
- `AMyCharacter::Tick()` early-returns when `Controller->IsPaused()` to skip gameplay logic during pause.
- `ClearPauseIfActive()` delegates to `TogglePause()` for unified unpause path — do not duplicate unpause logic.
- Widget uses `SetIsFocusable(true)` in `NativeConstruct()`; `FInputModeUIOnly::SetWidgetToFocus()` ensures keyboard input reaches the widget.
- **Implementation files**: `Source/Test/Public/Character/Controller/CharacterController.h:100-121`, `Source/Test/Private/Character/Controller/CharacterController.cpp:253-343`, `Source/Test/Public/HUD/PauseMenuWidget.h:19-46`, `Source/Test/Private/HUD/PauseMenuWidget.cpp:7-47`, `Source/Test/Private/Character/MyCharacter.cpp:67-73,293-300`.

### UE 5.7 Pitfalls

- `GetCurrentActiveMontage()` can be `nullptr`; current debug code null-checks the pointer before reading montage/section names.
- The current Slate geometry calls use `ToPaintGeometry(Size, FSlateLayoutTransform(...))`; do not reintroduce older deprecated signatures.
- `UPlayerHUDWidget`'s runtime vignette is a transient texture. When rebuilding it, clear the brush resource first, destroy the old texture with `ConditionalBeginDestroy()`, then recreate it with `UTexture2D::CreateTransient(...)`.
- `AWeapon::ResolveHit()` returning `FWeaponHitResult` is an established local pattern. Prefer a small result struct over piling on more out params when a combat helper needs to return several coupled values.
- `AWeapon::IgnoreActors` is private; notify/caller code must clear it through `ClearIgnoreActors()` instead of direct array mutation.

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

### Encapsulation

- Editable config that external code only reads should be `private` `UPROPERTY(EditAnywhere/EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))` plus a `FORCEINLINE` getter.
- Runtime state that should be visible but not externally mutated should be `private` `UPROPERTY(VisibleInstanceOnly, meta = (AllowPrivateAccess = "true"))` plus a getter.
- Keep members `protected` only when subclasses genuinely need direct access; current examples include `PendingHitContext` and `ConsumePendingHitKnockback()`.
- Container members such as `TArray` / `TMap` should stay private behind controlled methods when callers only need a narrow operation. Current example: `AWeapon::IgnoreActors` is cleared through `ClearIgnoreActors()`.
- Recent examples to follow: `AEnemy` poise parameters, `AShield` block/parry/equip parameters, `AWeapon::IgnoreActors`, and `ABaseCharacter` internal knockback state.

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
