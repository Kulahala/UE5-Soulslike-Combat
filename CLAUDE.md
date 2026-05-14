# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Editor

- UE 5.7 project, Windows only, VS 2022 required.
- Generate VS project: right-click `Test.uproject` → **Generate Visual Studio project files**, then open `Test.sln`.
- Compile in IDE (Development Editor) or via UBT: `UnrealBuildTool TestEditor Win64 Development Test.uproject`
- Launch editor: open `Test.uproject` directly.
- Module: `Test` (Runtime), `SmartBPCreator` (Editor plugin). Targets: `TestEditor` (Editor), `Test` (Game).
- `Test.Build.cs` pulls in: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `Slate`, `SlateCore`.

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
├── AMyCharacter (UAttributeComponent, spring arm + camera, weapon equipping)
└── AEnemy + IHitInterface (AI patrol/search/chase/combat state machine, directional hit react)

APlayerController → ACharacterController (Enhanced Input: 9 bound actions)
UActorComponent → UAttributeComponent (health, gold, OnHealthChanged delegate)
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
5. On hit → 格挡判定（跨阵营时 `Cast<IBlockableInterface>` → `TryBlockHit`）；格挡成功：减伤 + 跳过 GetHit 硬直；格挡失败/未防御：原伤害 + GetHit 硬直。`ApplyDamage` → `TakeDamage` → 扣血。
6. HitStop + CameraShake（所有命中都触发）
7. **NotifyEnd** → clears `IgnoreActors` blacklist
8. `OnAttackMontageEnded` delegate fires → `if (bInterrupted) return` guard → sets `EAS_UnOccupied` + resumes stamina regen

### Enemy AI (`AEnemy`)
- Controlled by `AAIController` via `EEnemyState` FSM.
- `CheckCombatTarget()` runs before per-state Tick logic: invalid targets return to `EES_Patrolling`, targets inside `CombatingRadius` switch to `EES_Combating`, and targets inside `ChasingRadius` switch to `EES_Chasing`.
- **Patrolling / Searching**: `OnPatrolling()` moves between `PatrolTargets`; once inside `PatrolRadius`, the enemy switches to `EES_Searching`. `OnSearching()` stops movement, starts `PatrolTimer` plus repeating `LookTimer`, and rotates toward `GenerateNewLookRotation()`.
- **Chasing / Combating**: `OnChasing()` reissues `MoveToTarget()` if path-following falls back to idle. `OnCombating()` rotates toward the target until `DotProduct > AttackAngleThreshold`, then attacks.
- **Combat Spacing**: `OnCombating()` 三分支：攻击就绪+在范围内→Attack()；攻击就绪+超出范围→前压；攻击CD→`UpdateCombatMovement()` 拉扯位移。`UpdateCombatMovement()` 按距离分 Retreat/BackDiag/Strafe/Press 四种策略，通过 `MoveToCombatLocation()` 发起导航请求，`ReceiveMoveCompleted` 委托回调重置 `bRepositionInProgress`。`RotateAngleAxis` 实现恒定半径横移。
- `SetEnemyState()` 使用 entry-action 模式：进入 `EES_Combating` 时 `StopMovement` + 关闭 `bOrientRotationToMovement` + 重置拉扯状态；进入 `EES_Attacking`/`EES_Stunned` 时清除 `bRepositionInProgress`。
- `PatrolTimer` and `LookTimer` are cleared via `ClearPatrolTimers()` on state transitions and death.
- `EES_Attacking`, `EES_Stunned`, and `EES_Dead` are hard-stop states for Tick-driven AI reactions.
- Directional hit react: `GetHitDirection()` returns `DotProduct`-based angle, used to pick `HitReactMontage` section name (Front/Back/Left/Right).

### Health Bar Buffer System
- `UBaseHealthBarWidget` has two `UProgressBar`: `PB_Health` (immediate) and `PB_Buffer` (delayed).
- `SetHealthPercent()` updates PB_Health instantly; PB_Buffer starts a delay timer (`BufferDelayTime`, default 2s) before lerping toward PB_Health at `BufferInterpSpeed`.
- `UHealthBarComponent::SetHealthPercent()` delegates to the widget.
- `UAttributeComponent::OnHealthChanged` broadcasts `HealthPercent` — widgets and components bind to this.

### Player HUD (`UPlayerHUDWidget`)
- 与 `UBaseHealthBarWidget` 共享 buffer delay 逻辑（PB_Health + PB_Buffer + PB_Stamina）。
- `BindToAttributes()` 绑定 `OnHealthChanged` + `OnStaminaChanged` delegate。
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
- `CanStartBlock()` 前置条件：有盾 + UnOccupied + 非拔刀中 + 地面。（独立于武器状态）
- `TryBlockHit()` 判定链：存活 → 方向(`DotProduct` vs `Cos(HalfAngle)`) → 体力成本检查 → 扣体力 + 减伤。
- 中断规则：`InterruptBlock(false)` = 临时（受击/空中），保留 `bBlockInputHeld` 自动补入；`InterruptBlock(true)` = 永久（耗尽/死亡），必须重按。
- Tick 自动恢复：每帧 `TryResumeBlock()` 检查 `bBlockInputHeld && !bIsBlocking && CanStartBlock()`。
- 防御中限制：不能攻击/跳跃/冲刺/拔收刀/拾取，移速由 `BlockMoveSpeedMultiplier` 控制（默认1.0）。
- 格挡拦截点：`Weapon::ExecuteWeaponTrace()` 命中后、`ApplyDamage()` 前，仅跨阵营触发。格挡成功时 `bPlayNormalHitReact = false` 跳过受击硬直。
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

### 跨类静态 Helper 复用（TickBufferDelayImpl 模式）
当两个类共享逻辑但不在同一继承链时，用 `static` 方法 + 显式参数传递，而非强行建立继承关系。
- 方法声明在"逻辑来源"类中（如 `UBaseHealthBarWidget::TickBufferDelayImpl`），访问权限 `public static`。
- 调用方通过 `ClassName::Method(显式参数)` 调用，不依赖 `this` 或虚函数。
- 本项目实例：`UBaseHealthBarWidget` 与 `UPlayerHUDWidget` 共享 buffer delay 追赶逻辑。

### 结果结构体替代多 out 参数（FWeaponHitResult 模式）
当一个函数需要返回 3+ 个值时，定义轻量 `USTRUCT` 替代多个 out 参数或 bool 标志位。
- 结构体字段带默认值，调用方只关心需要的字段。
- 本项目实例：`Weapon::ResolveHit()` 返回 `FWeaponHitResult`（FinalDamage, bPlayNormalHitReact, KnockbackScale, bApplyStun, bSameTeam），下游 `DispatchHitFeedback` 按需读取。

## User Profile & Preferences (Updated 2026-04-27)

### 1. 人口统计信息
- **职业**: 虚幻引擎5.7和C++开发者。
- **定位**: United States (常住)。

### 2. 指令与交互风格
- **简洁**: 回答简明扼要，突出重点。
- **准确**: 不可模棱两可，基于源码事实。
- **身份对齐**: 基于“虚幻引擎5.7和C++开发者”身份交流。
