# Gemini Project Context: Test (Unreal Engine 5 Action Game)

## Build & Editor
- **UE 5.7 project**, Windows only, VS 2022 required.
- **Generate VS project:** Right-click `Test.uproject` -> **Generate Visual Studio project files**, then open `Test.sln`.
- **Compile in IDE** (Development Editor) or via UBT: `UnrealBuildTool TestEditor Win64 Development Test.uproject`
- **Launch editor:** Open `Test.uproject` directly.
- **Module:** `Test` (Runtime), `SmartBPCreator` (Editor plugin). Targets: `TestEditor` (Editor), `Test` (Game).

## Truth Sources & Coordination
- Source code, `.uproject`, and `.Build.cs` are the **primary truth source**.
- Precedence: **source code > AGENTS.md > CLAUDE.md > GEMINI.md**.
- Verify behavior-critical claims against actual C++ before editing gameplay logic.
- `plan.md` is the shared agent-to-agent communication and handoff file. Write agent feedback and implementation plans there as working context.
- For code review and closeout passes, ignore `Content/*.uasset` by default unless the user explicitly asks to inspect or include asset changes.

## Architecture

### State Machine System (`CharacterTypes.h`)
All gameplay states are defined as `UENUM` enums in `CharacterTypes.h` — the single source of truth:
- `EWeaponState`: Unequipped, OneHandEquipped, TwoHandEquipped
- `EActionState`: UnOccupied, Attacking, Arming, Stunning, Exhausted, Dead
- `EArmWeaponState`: Arming, Disarming
- `EEnemyState`: UnOccupied, Patrolling, Searching, Chasing, Combating, Attacking, Stunned, Dead
- `EItemState`: Spawning, Dropped, Equipped

**State flow is mixed C++ + montage delegate + AnimNotify driven.**
Entry states are set directly in C++ (`Attack()`, `GetHit_Implementation()`, `Die()`, `SetEnemyState()`).
Recovery commonly uses `FOnMontageEnded` delegates with `bInterrupted` guards.
`AnimNotify` classes handle collision windows and designer-timed recoveries (`UAnimNotify_CharacterHitReactEnd` etc). **Do not hardcode recovery transitions in `Tick()`.**

### Class Hierarchy
```text
AActor
 ├── Aitem (parabolic spawning, floating animation, overlap events)
 │    ├── AWeapon (box-trace sweep collision, hit-stop, camera shake)
 │    ├── AShield (offhand equip, block angle/damage/stamina config, block FX)
 │    └── ATreasure (gold value, UTreasureData asset)
 ├── ABreakAbleActor + IHitInterface (StaticMesh -> GeometryCollection swap)
 ├── AArenaGenerator (USplineComponent + UPCGComponent)
 └── ABird (APawn subclass, flyable spectator)

ACharacter
 ├── AMyCharacter + IBlockableInterface (UAttributeComponent, spring arm + camera, weapon/shield equipping, hold-to-block)
 └── AEnemy + IHitInterface (AI patrol/search/chase/combat FSM, directional hit react)

APlayerController -> ACharacterController (Enhanced Input, input debug snapshot owner)
UActorComponent -> UAttributeComponent (health, stamina, gold, OnHealthChanged)
               └── UPlayerLockOnComponent (lock-on state, target selection)
UWidgetComponent -> UHealthBarComponent
UUserWidget -> UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars)
           └── UPlayerHUDWidget (player HP/stamina HUD + NativePaint debug text + damage vignette)
UAnimInstance -> USlashAnimInstance (GroundSpeed, Direction, state enums)
```

### Combat Pipeline
1. `Input_Attack()` -> `AMyCharacter::Attack()` -> `PlayAttackMontage()`
2. **NotifyBegin** -> `StartWeaponTrace()`. **NotifyTick** -> `ExecuteWeaponTrace()` (sweep old->new center).
3. On hit:
   - Shared tags between attacker/target mean **no `ApplyDamage`**, but target enters shared hit-feedback path.
   - Cross-team hits intercepted by `IBlockableInterface::TryBlockHit()`.
   - `ExecuteWeaponTrace()` writes `FPendingHitContext` before calling `IHitInterface::GetHit()`.
   - `GetHit_Implementation()` consumes context for knockback / normal hit-react.
4. **NotifyEnd** -> clears `IgnoreActors`.
5. `OnAttackMontageEnded` (with `bInterrupted` guard) restores `EAS_UnOccupied` and resumes stamina regen.

### Hit Knockback
- Short-distance weapon-hit knockback owned by `ABaseCharacter` (`TickHitKnockback`).
- Quadratic ease-out. A new hit overrides previous knockback. Zero-scale hit (full block) clears it.
- Successful blocks scale knockback by post-block damage ratio.

### Block System
- `AMyCharacter` implements `IBlockableInterface` (`bBlockInputHeld` + `bIsBlocking`).
- `AShield` provides block tuning (`BlockHalfAngleDegrees`, `BlockedDamageMultiplier`, etc.).
- Block reduces/redirects damage via `FBlockResult`, suppresses shared hit react, plays block FX.
- Blocked hits still flow through `GetHit` via `FPendingHitContext`.

### Lock-On System
- `ACharacterController` owns lock-on input. `UPlayerLockOnComponent` owns target search/scoring state.
- `AMyCharacter` keeps direct writes to `CharacterMovement`, controller rotation, and `SpringArm`.
- Lock-on camera driven by `SpringArm->SocketOffset`, interpolates via `Tick()`.
- Lock-on Sprint free-run: Target remains locked, camera yaws to enemy, but character uses `bOrientRotationToMovement=true` and `bUseControllerRotationYaw=false`.
- Sprint free-run attacks use `FaceDirection2D()` to attack towards movement input.

### Health, Regen & Stamina
- `UAttributeComponent` owns shared health/stamina.
- Regen is opt-in (enabled for player, disabled for enemies). Player regen: **1 HP/s**.
- `UseStamina()` allows brief "last action" overspend.
- `OnExhausted` broadcasts once. Recovery clears exhausted flag before adding stamina.

### Enemy AI
- FSM: Patrol -> Search -> Chase <-> Combat.
- `CheckCombatTarget()` drops to patrol if target is dead/invalid.
- `SetEnemyState(EES_Combating)` is entry boundary. Inside combating, attacks use `MoveToCombatTarget()` (gap close), cooldown uses `MoveToCombatLocation()` (reposition: Retreat, BackDiag, Strafe, Press).
- `OnAttackCooldownEnd()` calls `ResetCombatReposition()`.
- Navigation via `FAIMoveRequest` with `SetReachTestIncludesAgentRadius(false)`.

### HUD & UI Feedback
- `UBaseHealthBarWidget` uses `PB_Health` (immediate) and `PB_Buffer` (delayed catch-up).
- Player hurt feedback: **camera shake = hit reaction** (runs even on block), **red vignette = health loss**.
- Vignette mask is edge-distance gradient (`VignetteFadeWidth = 0.2`).
- Debug Overlay text gathered via `FDebugDrawHelper` and rendered by `UPlayerHUDWidget::NativePaint()`.

### UE 5.7 Pitfalls
- `GetCurrentActiveMontage()` can be `nullptr`.
- Transient textures (vignette) must be cleared, `ConditionalBeginDestroy()`, and recreated via `CreateTransient(...)`.
- `AWeapon::ResolveHit()` returning `FWeaponHitResult` is the local pattern.

## Code Conventions
- Headers in `Public/`, implementations in `Private/`.
- Forward declare in headers, include in `.cpp`. `.generated.h` must be last include.
- Prefix: `A` (Actor), `U` (Object/Component), `E` (Enum), `I` (Interface), `b` (boolean). No `m_` prefix for members.
- Expose to BP: `BlueprintCallable` or `BlueprintNativeEvent`.
- `AddDynamic()` requires `UFUNCTION()`.
- Use `check()` / `ensure()` for invariants. No `try/catch`.
- Comments: Chinese for gameplay intent, English for API docs.

## User Profile
- UE 5.7 C++ developer.
- Prefers concise, accurate answers grounded in source code facts.
- Chinese is acceptable for general communication; code and technical terms remain in English.
