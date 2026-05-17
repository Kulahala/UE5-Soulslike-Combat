# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Editor

- UE 5.7 project, Windows only, VS 2022 required.
- Generate VS project: right-click `Test.uproject` → **Generate Visual Studio project files**, then open `Test.sln`.
- Compile in IDE (Development Editor) or via UBT: `UnrealBuildTool TestEditor Win64 Development Test.uproject`
- Launch editor: open `Test.uproject` directly.
- Module: `Test` (Runtime), `SmartBPCreator` (Editor plugin). Targets: `TestEditor` (Editor), `Test` (Game).
- `Test.Build.cs` pulls in: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `Slate`, `SlateCore`.

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` are the primary truth source.
- When markdown documents disagree, use this precedence: **source code > AGENTS.md > CLAUDE.md > GEMINI.md**.
- Verify behavior-critical claims against actual C++ before editing gameplay logic.

## Architecture

### State Machine System (`CharacterTypes.h`)
All gameplay states are defined as `UENUM` enums in `CharacterTypes.h`. This is the single source of truth for state flow:

| Enum | States | Used By |
|------|--------|---------|
| `EWeaponState` | Unequipped, OneHandEquipped, TwoHandEquipped | `AMyCharacter`, `USlashAnimInstance` |
| `EActionState` | UnOccupied, Attacking, Arming, Stunning, Exhausted, Dead | `AMyCharacter` |
| `EArmWeaponState` | Arming, Disarming | `AMyCharacter`, `USlashAnimInstance` |
| `EEnemyState` | UnOccupied, Patrolling, Searching, Chasing, Combating, Attacking, Stunned, Dead | `AEnemy` |

**State transition pattern**: Mixed C++ + AnimNotify driven. Entry states are set directly in C++ (`Attack()`, `GetHit_Implementation()`, `Die()`). Recovery transitions use `FOnMontageEnded` delegates with `bInterrupted` guards as primary path. `UAnimNotify_CharacterHitReactEnd` is the exception — used for player hit react recovery so designers can tune stun duration in the animation editor. Enemy recovery has double coverage (delegate + AnimNotify with state guards).

**Montage Helper**: `ABaseCharacter::PlayMontageSection(UAnimMontage*, const FName&)` 只做 `Montage_Play()` + `Montage_JumpToSection()`。End-delegate 绑定留在语义调用方（`PlayAttackMontage()`、`PlayHitReactMontage()`、`PlayArmMontage()`）。不要合并成"通用蒙太奇入口"，除非恢复语义真正收敛。

### Class Hierarchy
```
AActor
├── Aitem (base: parabolic spawning, floating animation, overlap events)
│   ├── AWeapon (box-trace sweep collision, hit-stop, camera shake)
│   ├── AShield (off-hand equip, block parameters: angle/damage/stamina/speed)
│   └── ATreasure (gold value, initialized from UTreasureData asset)
├── ABreakAbleActor + IHitInterface (static mesh → GeometryCollection swap on hit)
├── Interfaces: IHitInterface (GetHit — hit reaction), IBlockableInterface (TryBlockHit — angle/stamina block check)
├── AArenaGenerator (USplineComponent + UPCGComponent for PCG-based arena spawning)
└── ABird (APawn subclass, flyable spectator)

ACharacter
├── AMyCharacter (UAttributeComponent, spring arm + camera, weapon equipping, lock-on targeting)
└── AEnemy + IHitInterface (AI patrol/search/chase/combat state machine, directional hit react)

APlayerController → ACharacterController (Enhanced Input: 10 bound actions, incl. LockOn)
UActorComponent → UAttributeComponent (health, gold, OnHealthChanged delegate)
UActorComponent → UPlayerLockOnComponent (lock-on state, target search/scoring, lock-on parameters)
UWidgetComponent → UHealthBarComponent
UUserWidget → UBaseHealthBarWidget (PB_Health + PB_Buffer progress bars, buffer delay logic)
UAnimInstance → USlashAnimInstance (exposes GroundSpeed, Direction, bIsBlocking, bIsStunning, state enums to anim graph)
UDataAsset → UTreasureData (static mesh, gold value, pickup sound, scale)
```

### Combat Pipeline
1. `ACharacterController::Input_Attack()` → calls `AMyCharacter::Attack()`
2. `PlayAttackMontage()` plays attack animation with `UAnimNotifyState_WeaponCollision` baked in
3. **NotifyBegin** → `AWeapon::StartWeaponTrace()` (records old box positions)
4. **NotifyTick** → `AWeapon::ExecuteWeaponTrace()` (sweeps from old→new center to prevent ghost swings)
5. On hit:
   - 同阵营命中：不 `ApplyDamage`，但仍走 `GetHit` 路径（击退、命中反馈、相机晃动）。同阵营判定通过 `FCombatTeamHelper::ShareTeamTag()`（Weapon + Enemy 共用）
   - 跨阵营命中：`IBlockableInterface::TryBlockHit()` 在 `ApplyDamage` 前拦截；格挡成功：减伤 + 跳过硬直
   - `ExecuteWeaponTrace()` 通过 `FPendingHitContext` 写入每命中的上下文（instigator、knockback scale、blocked flag、stun flag），然后调用 `GetHit()`
   - `ABaseCharacter::GetHit_Implementation()` 消费 context 驱动击退/受击反应，子类（`AMyCharacter`、`AEnemy`）在各自硬直逻辑后清空 context
   - `ExecuteWeaponTrace()` 分解为 `BuildIgnoreList()`、`ResolveHit()`、`DispatchHitFeedback()` 三步，不要膨胀为通用战斗管线
6. HitStop + CameraShake（所有命中都触发）
7. **NotifyEnd** → clears `IgnoreActors` blacklist
8. `OnAttackMontageEnded` delegate fires → `if (bInterrupted) return` guard → sets `EAS_UnOccupied` + resumes stamina regen

### Hit Knockback（受击后退）
- `ABaseCharacter` 通过 `FPendingHitContext` + `BaseHitKnockbackDistance` + `HitKnockbackDuration` + `TickHitKnockback()` 共享短距离武器命中击退。
- 击退是**武器命中反馈**，非通用伤害反馈：陷阱/DOT 只调 `TakeDamage()` 不自动触发。
- 默认值：`AMyCharacter` 10cm，`AEnemy` 5cm。
- 运动曲线：quadratic ease-out，通过 `AddActorWorldOffset(..., true, &Hit)` sweep 位移，可被墙挡住。
- 新命中覆盖旧击退；零缩放命中（如满格挡）清除进行中的击退。
- 格挡成功按减伤比例缩放击退距离（`DamageAfterBlock / Damage`）。
- 友方武器命中也触发击退和命中反馈，但不造成伤害。

### Player Hit Feedback（受击反馈分离）
- **相机晃动 = 命中反馈**，**红晕 = 血量损失**。
- `GetHit_Implementation()` 触发 `HitReceivedCameraShake`（含格挡和友方命中）。
- `TryBlockHit()` 写入 `LastDamageFlashScale`；`TakeDamage()` 在零伤害/满格挡时归位，防止状态泄漏。
- `SetHealthPercent()` 仅在血量实际下降时消费 `LastDamageFlashScale`，红晕强度跟踪最终减伤后伤害。
- 红晕蒙版：程序化生成边缘距离渐变（非径向中心衰减），`VignetteFadeWidth = 0.2` = 外围 20% 红晕。

### Enemy AI (`AEnemy`)
- Controlled by `AAIController` via `EEnemyState` FSM.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid **or dead** targets return to `EES_Patrolling`（via `IsValidCombatTarget()` helper — checks `IsValid()` + `Cast<ABaseCharacter>` + `IsAlive()`）。**战斗退出滞后**：已在战斗族状态（`EES_Combating`/`EES_Attacking`/`EES_Stunned`）时，退出半径使用 `CombatingRadius + CombatExitBuffer`（默认 350），防止边界每帧在 Chasing/Combating 间抖动；非战斗族状态仍用 `CombatingRadius`(300) 进入。
- `IsValidCombatTarget()` is also used in `TargetPerceptionUpdated()` (prevents dead player re-acquisition) and `CanAttack()` (defense-in-depth).
- **Patrolling / Searching**: `OnPatrolling()` moves between `PatrolTargets`; once inside `PatrolRadius`, the enemy switches to `EES_Searching`. `OnSearching()` stops movement, starts `PatrolTimer` plus repeating `LookTimer`, and rotates toward `GenerateNewLookRotation()`.
- **Chasing / Combating**: `OnChasing()` is `virtual`，派生类可覆写追逐行为（如法师后撤、自爆兵冲脸）。`OnCombating()` 保留公共流程（距离/朝向计算、转身、速度缓动更新），战斗决策委托给 3 个 `protected virtual` 钩子。
- **战斗决策钩子（Virtual Seam）**：
  - `ShouldTriggerAttack(DistanceToTarget, ForwardDot)` — 是否满足出手条件（默认：非 CD + 朝向达标 + 在攻击距离内）
  - `HandleAttackReadyPositioning(DistanceToTarget, ToTarget)` — 攻击就绪但未出手时的定位（默认：超出范围→`MoveToCombatTarget()` 追踪；在范围内→停住等转身）
  - `HandleCooldownPositioning(DeltaTime, DistanceToTarget, ToTarget)` — 攻击 CD 期间的拉扯（默认：`UpdateCombatMovement()` 四策略）
  - **所有战斗决策入口都必须走这组钩子**：`OnCombating()` Tick 和 `OnAttackCooldownEnd()` 定时器回调都统一调用，子类覆写后两条路径自动生效。
- **Combat Spacing**: `UpdateCombatMovement()` 按距离分 Retreat/BackDiag/Strafe/Press 四种策略，通过 `MoveToCombatLocation()` 发起导航请求，`ReceiveMoveCompleted` 委托回调重置 `bRepositionInProgress`。`RotateAngleAxis` 实现恒定半径横移。
- **Retreat Speed Ease**: Retreat/BackDiag 使用速度缓动：起始 `PatrolSpeed`(150)，quadratic ease-out 降到 `PatrolSpeed * CombatRetreatMinSpeedRatio`(82.5)。`StartCombatRetreatSpeedEase()` 在导航成功时启动，`UpdateCombatRetreatSpeedEase()` 每帧 Tick 更新，`OnRepositionMoveCompleted` 清理。Strafe 不缓动（保持 `PatrolSpeed`），Press 用 `ChaseSpeed`(330)。
- `SetEnemyState()` 使用 entry-action 模式：进入 `EES_Combating` 时保存 `OldState`，仅在 `OldState != EES_Chasing || DistToTarget <= CombatAttackMaxRadius` 时 `StopMovement`（远距离追逐入口让 `OnCombating` 前压接管），关闭 `bOrientRotationToMovement`，重置拉扯状态；进入 `EES_Attacking`/`EES_Stunned` 时清除 `bRepositionInProgress`。
- **参数约束**：`CombatTooCloseRadius(90) < CombatAttackMaxRadius(170) <= CombatPreferredMinRadius(210) <= CombatPreferredMaxRadius(270) < CombatingRadius(300) < ChasingRadius(1000)`。`CombatPressMargin(25)` 必须大于 `CombatRepositionAcceptanceRadius(12)`，否则前压会在攻击范围外停住。
- **CD 结束处理**: `OnAttackCooldownEnd()` 先 `ResetCombatReposition()` 清门闩，再按状态分支：非 Combating 或目标无效→不打断导航；Combating→复用战斗决策钩子：`ShouldTriggerAttack()` 为真→停住等出手，否则→`HandleAttackReadyPositioning()` 走子类定位策略。
- `MoveToCombatLocation()` 用 `FAIMoveRequest` + `SetReachTestIncludesAgentRadius(false)` + `SetReachTestIncludesGoalRadius(false)`。不要手动 `ProjectPointToNavigation()`，`MoveTo` 已内置目标投影。`MoveToCombatTarget()` 同理，但用 `SetGoalActor(ChasingTarget)` 动态追踪。
- `OnChasing()` 对 `ChasingTarget` 的 `MoveToTarget()` 同样禁用 agent/goal radius 紧凑测试，避免追击距离过远停下。
- `PatrolTimer` and `LookTimer` are cleared via `ClearPatrolTimers()` on state transitions and death.
- `EES_Attacking`, `EES_Stunned`, and `EES_Dead` are hard-stop states for Tick-driven AI reactions.
- Directional hit react: `GetHitDirection()` returns `DotProduct`-based angle, used to pick `HitReactMontage` section name (Front/Back/Left/Right).

### Health Bar Buffer System
- `UBaseHealthBarWidget` has two `UProgressBar`: `PB_Health` (immediate) and `PB_Buffer` (delayed).
- `SetHealthPercent()` updates PB_Health instantly; when health drops it resets `CurrentBufferDelay` using `BufferDelayTime` before PB_Buffer starts moving; healing snaps buffer bar upward immediately.
- `TickBufferDelayImpl(...)` 是共享的 buffer 追赶逻辑，`UPlayerHUDWidget` 复用。
- `UHealthBarComponent::SetHealthPercent()` delegates to the widget.
- `UAttributeComponent::OnHealthChanged` broadcasts `HealthPercent` — widgets and components bind to this.
- 敌人血条可见性由 `ShowHealthBar()`/`HideHealthBar()` 定时器驱动，可选蓝图淡出动画 `PlayFadeOutAnim()`。
- `RevealHealthBar()` 是共享的"血条拉回可见态" helper：恢复 component visibility + widget visibility + opacity + `CancelFadeOutAnim()`。`ShowHealthBar()`（受击）和 `SetTargetedByPlayer(true)`（锁定）共用此入口，防止两条路径恢复逻辑漂移。

### Lock-On System (`AMyCharacter` + `UPlayerLockOnComponent`)
- **组件架构**：`UPlayerLockOnComponent`（`ActorComponent`，不 Tick）拥有锁定状态（`bIsLockingOn`、`LockedTarget`）、目标搜索/评分逻辑、所有 `LockOn*` 参数。`AMyCharacter` 保留 facade（`ToggleLockOn`、`ClearLockOn`、`IsLockingOn`）+ 旋转/相机实际写入。组件只返回"期望值"，Character 统一执行 CharacterMovement 和 SpringArm 修改。
- **输入**：`ACharacterController::Input_LockOn()` 绑定中键 `ETriggerEvent::Started`，调用 `AMyCharacter::ToggleLockOn()`。`Input_Look()` 在 `IsLockingOn()` 时早退，忽略视角输入。
- **目标搜索**：`LockOnComponent->FindBestTarget(PlayerLoc, CameraForward)` 遍历所有 `AEnemy`，`ScoreTarget()` 按 `IsAlive()` + 距离 + Camera forward 视角角度评分，返回最低分目标。
- **状态管理**：`bIsLockingOn` 和 `LockedTarget` 由组件持有。`IsLockingOn()` / `GetLockedTarget()` 委托组件 + nullptr 守卫。
- **旋转模式切换**：开启时缓存 `bOrientRotationToMovement` / `bUseControllerRotationYaw` / `bUsePawnControlRotation`，切换到锁定模式（`false` / `true` / `true`）。`ClearLockOn()` 恢复缓存值。
- **越肩相机**：`LockOnSocketOffset = (0, 80, 80)` 右肩偏移。`Tick()` 中用 `FMath::VInterpTo` 双向插值——锁定中朝 `LockOnSocketOffset`，非锁定朝 `CachedSocketOffset`。`CachedSocketOffset` 仅在 `BeginPlay()` 初始化一次，不随重新锁定覆盖。
- **自动解锁**：`UpdateLockOn()` 每帧检查目标 `!IsValid()` / `!IsAlive()` / 距离 > `LockOnBreakRadius` → `ClearLockOn()`。玩家死亡也调 `ClearLockOn()`。
- **敌人血条联动**：`SetTargetedByPlayer(true)` → 显示血条 + 清隐藏计时器 + `CancelFadeOutAnim()`；`SetTargetedByPlayer(false)` → 重启隐藏计时器。
- **Tick 执行顺序**：`SocketOffset + TargetArmLength 插值 → UpdateLockOn → [Stunning/Dead 早退] → 格挡/移速/Debug`。`UpdateLockOn` 和相机插值在状态早退之前，确保硬直/死亡时仍能清理目标和回正相机。
- **Tick 旋转应用**：`FindLookAtRotation(PlayerLoc, TargetLoc)` → `LookAt.Pitch = 0.f`（只锁 yaw）→ `RInterpTo` → `SetControlRotation`。内部有死亡/硬直 guard 不应用转向但仍然清理。
- **锁定冲刺 Free-Run**：`ShouldUseLockOnFreeRun()` 条件 = `bIsLockingOn && bIsSprinting && EAS_UnOccupied && !IsFalling && 有移动输入`。满足时角色临时恢复自由移动语义：`bOrientRotationToMovement=true`（角色朝移动方向跑），`bUseControllerRotationYaw=false`（脱离控制器朝向），但控制器/相机继续盯敌人。`UpdateMovementSpeed()` 绕过锁定方向降速，`TickSprintStamina()` 绕过 `Dot>0.2f` 门槛。松开 Sprint 立即恢复普通锁定旋转。`ApplyLockOnRotationMode()` 每帧 Tick 末尾根据条件切换旋转模式。
- **锁定 Free-Run 相机偏移**：Tick 中 SocketOffset 插值改为三态（非锁定→`CachedSocketOffset` / 普通锁定→`LockOnSocketOffset` / free-run→`LockOnSocketOffset + DynamicOffset`）。`GetLockOnFreeRunCameraInputLocal()` 将移动输入转控制器 yaw 局部坐标，`GetLockOnFreeRunCameraOffsetTarget()` 拆 `SideAlpha`(Y) + `BackAlpha`(-X, 仅后撤) 输出偏移。参数：`LockOnFreeRunCameraSideOffset`(60)、`LockOnFreeRunCameraBackHeightOffset`(40)、`LockOnFreeRunCameraBackArmLengthBonus`(0)、`LockOnFreeRunCameraInterpSpeed`(10)。`TargetArmLength` 同步插值，`CachedTargetArmLength` BeginPlay 缓存。
- **锁定冲刺攻击**：`Attack()` 入口检测 free-run → `FaceDirection2D()` 对齐奔跑方向 → 设 `bOrientRotationToMovement=false, bUseControllerRotationYaw=false` 锁住朝向。蒙太奇结束 delegate 调 `RestorePostAttackRotationMode()` 恢复（无 `EAS_Attacking` guard，interrupted 也执行）。`ApplyLockOnRotationMode()` 保留 `EAS_Attacking` 早退防止 Tick 覆盖。`ClearLockOn()`/`Die()` 已有旋转恢复路径，无需额外清理。
- **蓝图待办**：`CancelFadeOutAnim()` 需在血条 Widget Blueprint 中实现（停止淡出动画 + 恢复可见状态）；创建 `IA_LockOn` 输入资产绑定中键。

### Player HUD (`UPlayerHUDWidget`)
- 与 `UBaseHealthBarWidget` 共享 buffer delay 逻辑（PB_Health + PB_Buffer + PB_Stamina）。
- `BindToAttributes(UAttributeComponent*)` 绑定 `OnHealthChanged` + `OnStaminaChanged` delegate。不持有 `AMyCharacter` 指针。
- 受击红晕：`AMyCharacter::TakeDamage()` 通过 `SetPendingDamageFlashScale()` 显式推送 flash scale（不再由 Widget 反查角色）。`SetHealthPercent()` 消费后归位。
- 由 `AMyCharacter::BeginPlay()` 通过 `CreateWidget<>` 创建并 `AddToViewport()`。

### Stamina & Exhaustion System
- `UAttributeComponent` manages stamina: `UseStamina()`, `AddStamina()`, `CheckStamina()`.
- Stamina can temporarily go negative (e.g. 5 stamina → attack costs 15 → -10) to allow "last action" before exhaustion.
- When stamina hits 0, `OnExhausted` broadcasts → `HandleExhausted()` sets `EAS_Exhausted` + starts 5s timer.
- During Exhausted: player can only walk. `RecoverFromExhaustion()` resets state to `EAS_UnOccupied` with state guard (`if (ActionState != EAS_Exhausted) return`).
- `bStaminaJustDepleted` flag prevents repeated exhaustion broadcasts; reset by `ResetExhaustionFlag()` on recovery.
- Stamina regen is paused during attacks (`PauseStaminaRegen`) and resumed by montage end delegates.

### Health Regen System
- `UAttributeComponent` 支持生命恢复：`EnableHealthRegen()` / `DisableHealthRegen()`，默认关闭。
- `HealthRegenRate`（默认 1/s）在 TickComponent 中生效，条件：`bHealthRegenActive && IsAlive() && CurrentHealth < MaxHealth`。
- 玩家在 `BeginPlay` 中显式启用：`Attributes->EnableHealthRegen()`。敌人默认不启用。

### Shared Direction Helper
- `ABaseCharacter::CalcForwardDot2D(const FVector& WorldDirection)` — 共享 2D 面朝点积。调用方传世界空间方向向量，**不传目标位置**。
- 用于移动方向、攻击者方向、目标方向等。`IHitInterface::GetHitDirection()` 单独用于有符号角度的受击反应路由。

### Movement Speed System
- 基础移速: Walk=150, Run=300, Sprint=450（仅前方 DotProduct>0.2 时生效）。
- 方向性缩放: 前(100%) → 侧(80%) → 后(65%)，阈值 DotProduct ±0.2。
- 冲刺体力消耗: 12/秒，仅地面 + 前方移动 + 非防御时扣，每帧 `ResetStaminaRegenCooldown()`。
- 防御移速: `bIsBlocking` 时 `SpeedMultiplier = Shield->BlockMoveSpeedMultiplier`(默认1.0)，优先于方向缩放。
- 拔刀移速: `ArmWeaponState == AWS_Arming` 时 `SpeedMultiplier = 0.875`。

### Shield & Blocking System
- `IBlockableInterface` + `FBlockResult` — 纯 C++ virtual interface，独立于 `IHitInterface`，通过 `Cast<IBlockableInterface>(HitActor)` 调用。
- `AShield` — 副手装备，参数载体：`BlockHalfAngleDegrees`(角度)、`BlockedDamageMultiplier`(减伤)、`BlockStaminaCostPerDamage`(体力/伤害比)、`BlockMoveSpeedMultiplier`(移速)、`BlockSound`/`BlockParticle`(反馈)。
- `EquipToOffhand()` 只设 Owner 不设 Instigator（与 `Weapon::Equip()` 不同），因为盾牌不造成伤害。
- 按住防御：`bBlockInputHeld` + `bIsBlocking` 双标志，不新增 `EActionState`，防御中 `ActionState` 保持 `EAS_UnOccupied`。
- `CanStartBlock()` 前置条件：有盾 + UnOccupied + 非拔刀中 + 地面。（**独立于 `ArmWeaponState`**，不要加武器状态检查）
- `TryBlockHit()` 判定链：存活 → 方向(`DotProduct` vs `Cos(HalfAngle)`) → 体力成本检查 → 扣体力 + 减伤。
- 中断规则：`InterruptBlock(false)` = 临时（受击/空中），保留 `bBlockInputHeld` 自动补入；`InterruptBlock(true)` = 永久（耗尽/死亡），必须重按。
- Tick 自动恢复：每帧 `TryResumeBlock()` 检查 `bBlockInputHeld && !bIsBlocking && CanStartBlock()`。
- 防御中限制：不能攻击/跳跃/冲刺/拔收刀/拾取，移速由 `BlockMoveSpeedMultiplier` 控制（默认1.0）。
- 格挡拦截点：`Weapon::ExecuteWeaponTrace()` 命中后、`ApplyDamage()` 前，仅跨阵营触发。格挡成功时 `bPlayNormalHitReact = false` 跳过受击硬直。
- 格挡命中仍走 `GetHit` → `FPendingHitContext`，所以缩放击退和类特定反馈仍生效。
- 调参时同步更新 C++ 默认值（`AShield::BlockedDamageMultiplier`）和蓝图覆盖值。
- 防御移速：`UpdateMovementSpeed()` 中 `SpeedMultiplier` 优先判断 `bIsBlocking` → `Shield->BlockMoveSpeedMultiplier`(默认1.0)，覆盖方向缩放。

### Content Organization
- C++ source under `Source/Test/` (Public/Private mirrors UE module structure).
- Game assets under `Content/_GAME/` — this is the only Content directory that should be modified.
- Large reference assets (AncientContent, AnimalVarietyPack, ParagonAurora, Mixamo, etc.) are ignored for AI context — they're read-only marketplace/paragon content.

### Debug System (`FDebugDrawHelper`)
- `FDebugDrawHelper` — 静态工具类，集中管理屏幕调试文字 + 世界调试图形。路径：`Utils/DebugDrawHelper.h/cpp`。
- 屏幕文字：`Add(Text, Color)`，帧号自动刷新（`GFrameCounter`），`GetEntries()` 帧号校验防 stale data。
- 世界图形：`AddSphere(World, Center, Radius, Color, Segments)`，通过 `DrawDebugSphere` 实现，不参与文字帧生命周期。
- CVar 开关（默认全开，Demo 前改 `Enable=0`）：`test.Debug.Enable`（总开关）、`test.Debug.Enemy`（敌人文字）、`test.Debug.Shapes`（世界图形）。`IsShapesEnabled()` 受 `Enable` 约束。
- 跨文件访问：通过 `IsDebugEnabled()`/`IsEnemyEnabled()`/`IsShapesEnabled()` 公开静态方法，不暴露 CVar 变量。
- 绘制宿主：`UPlayerHUDWidget::NativePaint()` 通过 `FSlateDrawElement::MakeText` 在左侧中部绘制。
- `NativePaint` 必须使用 `Super::NativePaint()` 返回的 Layer 作为绘制基准，不要用传入的 `LayerId`。
- 玩家调试内容：输入快照、HP、体力、动作状态、蒙太奇名、移动速度。
- 敌人调试内容：状态/地速、距离、追逐/战斗半径球、`CombatMove: Ready/Retreat/BackDiag/Strafe/Press/AlreadyAtGoal/MoveFail`。

### Input Debug System
- `ACharacterController` 持有输入调试状态（held bools + expire times + move dir），`MyCharacter` 只读显示。
- `GetDebugInputText()` 格式化一行：`Input: [Sprint] [Block] Attack Move(0.0, 1.0)`。
- 单次输入用 `GetTimeSeconds() + 0.15f` 过期，不受帧率影响。
- 所有 handler 统一先采样 debug 再走 gameplay gate，HUD 显示真实玩家输入。
- Sprint/Walk/Block 补 `Canceled` 绑定，防止 held 状态挂住。
- 所有调试代码标注 `[调试]` 注释区分。

### AI Collaboration (`plan.md`)
- 项目根目录 `plan.md` 用于多 AI agent 间通信（Claude Code、Codex 等），已 gitignore。
- 虚线上方为规则（不可修改），下方为反馈意见和计划。
- 计划完成后标 `✅ 已完成`，下次可清空重写。

### UE 5.7 API 注意事项
- `FSlateDrawElement::MakeText` 需要 `SlateCore` 模块（Build.cs 中启用）。
- `ToPaintGeometry` 弃用 `(offset, clippedZone)` 签名，改用 `(size, FSlateLayoutTransform(offset))`。
- `GetCurrentActiveMontage()` 可能返回 nullptr，即使 `IsAnyMontagePlaying()` 为 true。必须单独 null 检查。
- `FAIMoveRequest` 默认 `bReachTestIncludesAgentRadius(true)` + `bReachTestIncludesGoalRadius(true)` — 胶囊体半径（~34cm）会被加进 AcceptanceRadius，短距离移动（<50cm）会直接返回 `AlreadyAtGoal`。短距离导航必须 `SetReachTestIncludesAgentRadius(false)` + `SetReachTestIncludesGoalRadius(false)`。
- `UFUNCTION()` 回调参数不能用 `struct`/`enum` 前向声明（UHT 解析不到）。枚举必须用 namespace 前向声明：`namespace EPathFollowingResult { enum Type : int; }`，参数写 `EPathFollowingResult::Type`。

### 程序化纹理生成（NativePaint 用）
- `UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8)` 创建临时纹理，不需要外部 PNG。
- 写入流程：`GetPlatformData()->Mips[0].BulkData.Lock()` → 填像素 → `Unlock()` → `UpdateResource()` → 挂到 `FSlateBrush`。
- `NeverStream = true` + `TF_Bilinear` 防止流式加载和锯齿。
- 参数变化时需 `ConditionalBeginDestroy()` 旧纹理再重建，避免内存泄漏。

### 跨系统瞬时值通信（LastDamageFlashScale 模式）
- 当需要在一个系统（如格挡判定）设置数据、另一个系统（如 HUD）消费时，用瞬时 float + 归位守卫。
- 设置方写入值，消费方读取后立即归位到默认值（如 1.f），兜底路径处理消费方未执行的边界情况。
- 本项目实例：`TryBlockHit` 设置 → `SetHealthPercent` 消费归位 → `TakeDamage` 零伤害兜底归位。

### 替换式状态更新：先清后判
当函数需要"覆盖旧状态"时，先清空旧状态再做 early-return 守卫。
错误：`if (Scale <= 0) return;` 然后才清旧状态 → 零值输入无法终止旧行为。
正确：先清 `bActive = false; elapsed = 0;`，再 `if (bad_input) return;`。
实例：`StartHitKnockback` 中零缩放命中必须终止旧 knockback。

### Sweep 位移累计：用实际距离而非目标距离
`AddActorWorldOffset(Delta, true, &Hit)` 带 sweep 时可能被墙挡住。
撞墙后按目标距离累计会丢失剩余位移。
正确做法：记录 `OldLocation`，位移后 `Distance += Dist2D(OldLocation, GetActorLocation())`。

### UI 动画曲线
- 指数衰减 `Alpha *= pow(0.01, dt/Duration)` 比线性淡出更自然（前快后慢拖尾）。
- Duration 含义：Alpha 衰减到 1% 的时间。`0.01` 可调，越小衰减越快。

### 相机偏移方向：用控制器局部空间，不用 Actor 局部空间
当需要"玩家输入方向 → 相机偏移"时，必须用 `GetControlRotation().Yaw` + `UnrotateVector` 转到控制器局部空间。Actor 局部空间在角色身体旋转后会吃掉左右后信息（如 free-run 时角色朝运动方向旋转，Actor Forward = 运动方向，左侧输入变成 Back）。用速度方向也有问题：撞墙时速度归零但玩家意图不变。

### 跨类静态 Helper 复用（TickBufferDelayImpl 模式）
当两个类共享逻辑但不在同一继承链时，用 `static` 方法 + 显式参数传递，而非强行建立继承关系。
- 方法声明在"逻辑来源"类中（如 `UBaseHealthBarWidget::TickBufferDelayImpl`），访问权限 `public static`。
- 调用方通过 `ClassName::Method(显式参数)` 调用，不依赖 `this` 或虚函数。
- 本项目实例：`UBaseHealthBarWidget` 与 `UPlayerHUDWidget` 共享 buffer delay 追赶逻辑。

### 结果结构体替代多 out 参数（FWeaponHitResult 模式）
当一个函数需要返回 3+ 个值时，定义轻量 `USTRUCT` 替代多个 out 参数或 bool 标志位。
- 结构体字段带默认值，调用方只关心需要的字段。
- 本项目实例：`Weapon::ResolveHit()` 返回 `FWeaponHitResult`（FinalDamage, bPlayNormalHitReact, KnockbackScale, bApplyStun, bSameTeam），下游 `DispatchHitFeedback` 按需读取。

## Code Conventions

### Includes
- System/engine first, then project — forward declare when possible.
- `.generated.h` must be last include.
- Project includes use full relative path from `Public/`.

### UPROPERTY / UFUNCTION
- Components: `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` + `meta = (AllowPrivateAccess = "true")` when private.
- Editable config: `EditAnywhere` or `EditDefaultsOnly`。Runtime-only: `VisibleInstanceOnly`。
- ToolTip: `meta = (ToolTip = "...")` — 行注释只帮源码读者，ToolTip 帮编辑器用户。
- `AddDynamic(...)` 绑定的函数**必须**标记 `UFUNCTION()`，即使不暴露给蓝图。
- UMG 子控件用 `UPROPERTY(meta = (BindWidget))`；蓝图钩子用 `BlueprintImplementableEvent`。
- 不做全局 `TObjectPtr` 迁移。跟随文件风格：本项目大多用裸指针，`TObjectPtr` 仅少数位置（如 `AArenaGenerator`）。

### Naming
| Type | Convention | Example |
|------|-----------|---------|
| Classes | `A`/`U` prefix, PascalCase | `AMyCharacter`, `UAttributeComponent` |
| Enums | `E` prefix, value prefix `ECS_`/`EAS_`/`EES_` | `EActionState::EAS_Attacking` |
| Interfaces | `I` prefix | `IHitInterface` |
| Members | PascalCase, no `m_` prefix | `OverLapItem` |
| Booleans | `b` prefix | `bIsSprinting` |
| Methods | PascalCase | `GetCharacterState()` |

### Comments
- 中文注释用于 gameplay 意图。
- 英文用于 API 文档和技术笔记。
- `/** */` 仅用于 UFUNCTION/UPROPERTY 文档。

## User Profile & Preferences (Updated 2026-04-27)

### 1. 人口统计信息
- **职业**: 虚幻引擎5.7和C++开发者。
- **定位**: United States (常住)。

### 2. 指令与交互风格
- **简洁**: 回答简明扼要，突出重点。
- **准确**: 不可模棱两可，基于源码事实。
- **身份对齐**: 基于“虚幻引擎5.7和C++开发者”身份交流。
