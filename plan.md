# plan.md — AI Agent 协作文件

## 规则

1. **读取**: 先读本文件，了解其他 agent 的意见 and 计划
2. **意见**: 对其他 AI 的改法意见可添加或直接覆盖写在下方"反馈意见"区域
3. **计划**: 接在意见下方的"计划"区域，支持修订和增量修改
4. **完成**: 计划执行后在计划区头部标记"✅已执行"，经过review后可标记"已通过"
4a. **Review 标记**: "已通过"只能由执行 review 的 AI 标记，不能由实现方自行标记
5. **清理**: 下次看到"✅已完成"或"已通过"，直接清空意见区和计划区下方内容
6. **虚线上方规则不可修改**

---

## 反馈意见 (Feedback)

---

## 计划 (Plan)

### TODO-04D-B: Erika Archer Authoring And TestMap v1

**状态：✅已执行，已通过严格 review。D-A 与 B0 已通过；Erika 已使用真实、合法的可见弓 Mesh，并显式使用 `LeftHandSocket`，而默认 `RightHandSocket` 继续保护 Paladin。`AArrowProjectile` 是本阶段的窄原生表现子类，只提供无碰撞 `ArrowVisual` 组件，`BP_ErikaArrow` 只选择 Mesh 和相对变换。不得用剑、空手或无关模型伪装最终弓表现。**

#### 目标与成功标准

- 让 `BP_ErikaArcher` 以 `SKM_ErikaArcher`、`ABP_ErikaArcher` 和真实弓外观成为可放置的正式远程敌人。
- 远程攻击使用现有 `DA_EnemyAttack_ErikaArcher`、`BP_ErikaArrow`、`UAnimNotify_EnemyProjectileRelease` 和 D-A 的 LOS/撤退/一次 Release 合同；不增加敌人 C++ 子类、Timer 或输入路径。
- 在 `TestMap` 放置一只刻意配置的 Erika，验证真实 Montage 的 Release 帧、墙体优先、最小距离撤退、格挡、受击/死亡中断与地图重载。

#### 已确认的运行时边界

- `AEnemy` 会在 `BeginPlay()` 用 `WeaponClass` 生成 `AWeapon`，并通过实例可配置的 `WeaponAttachSocketName` 附着到 Skeletal Mesh。默认值 `RightHandSocket` 保持 Paladin 和既有近战敌人行为；`AWeapon::Equip()` 失败时，`AEnemy` 必须销毁刚生成的候选武器且不写入 `EquippedWeapon`。Erika 已确认左手握弓，正式 `BP_ErikaArcher` 必须显式设为 `LeftHandSocket`。正式 Erika 弓应创建为 `BP_ErikaBow : AWeapon`；不要复用 `ABow`，后者持有玩家箭矢与瞄准配置。禁止把名称叫 `RightHandSocket` 的 Socket 偷挂到左手骨骼上。
- 投射物发射点只从 `GetMesh()->GetSocketLocation(ProjectileSpawnSocketName)` 读取。因此 `ArrowReleaseSocket` 必须位于 `SK_ErikaArcher` 的 Skeleton 上、附着到真正拉弦并在 Release 时松开的手，不能仅存在于弓 Static Mesh 或 `BP_ErikaBow` 内。
- `UAnimNotify_EnemyProjectileRelease` 只请求 `AEnemy` 发射；它已经校验攻击状态、目标、距离、LOS 和一次性 Release。通知不得在动画中重复放置。
- 当前 AI 没有“等待外部信号后跳出无限瞄准循环”的 C++ 路径。因此中段统一命名为 `AimHold`，Section Association 必须是一次性线性 `Draw -> AimHold -> Release`；不得创建真正循环，否则不会到达 Release Notify。
- `ACombatProjectile` 已持有根球体碰撞、直线移动、零重力、生命周期和命中。新增 `AArrowProjectile : ACombatProjectile` 仅在构造期创建附着到根的 `ArrowVisual`，并锁定其为无碰撞、无 Overlap、无 Physics、无重力的表现组件；它不增加发射、伤害、Timer、Tick 或 Blueprint 事件路径。`BP_ErikaArrow` 改为继承该类，只配置 `ArrowVisual` 的 Mesh 与相对变换，不得替换碰撞、移动、寿命或命中实现。
- 用户已批准两项 D-B 窄体验修订：活动 Projectile 攻击进入 `EES_Attacking` 后，`AEnemy` 继续以现有 `CombatRotationSpeed` 追踪有效目标的水平朝向；Melee 攻击、受击、破防、死亡和 Encounter Dormant 继续维持原有 Tick 早退。`FEnemyAttackEntry` 增加 `ProjectileTargetHeightOffset`（cm，相对目标 `GetActorEyesViewPoint()`，默认 `0`），并在创建 `FActiveProjectileAttack` 快照时冻结。LOS 和实际发射必须共用同一目标点 resolver；Erika 的正式条目设为 `-30 cm`，先以胸部为瞄点，后续只通过 DataAsset 微调，不从 Socket 或 Mesh 偏移伪造弹道。

#### 当前资产与预期资产

- 已有：`BP_ErikaArcher`、`ABP_ErikaArcher`、`BS_ErikaArcher_Locomotion`、`AM_Attack_ErikaArcher_Ranged`、`AM_HitReact_ErikaArcher`、`AM_Death_ErikaArcher`、`DA_EnemyAttack_ErikaArcher`、`DA_EnemyHitReaction_ErikaArcher`、`BP_ErikaArrow`。本阶段新增原生 `AArrowProjectile`，供 Erika 及未来箭矢表现 Blueprint 复用。
- 已有动画候选：`standing_draw_arrow`、`standing_aim_overdraw`、`standing_aim_recoil`、`standing_react_small_from_front`、`standing_death_forward_01`、`standing_death_backward_01`。
- 已有可引用箭表现：`/Game/AncientContent/Effects/Fog/Particles/Meshes/SM_Arrow`；只引用，不编辑 Marketplace/参考内容。
- 已有：`BP_ErikaBow : AWeapon` 已引用 `/Game/Fab/Survival_Kit_-_Bow/survival_kit_bow/StaticMeshes/survival_kit_bow`，其 Mesh 已配置为无碰撞、无 Overlap、无 Physics、无重力。已确认将该合法依赖作为 D-B 提交的一部分：Bow StaticMesh、一个 Material Instance 与三张 Texture；不得只提交引用该 Mesh 的 Blueprint 而留下失效引用，也不在本阶段无意义迁移副本到 `_GAME`。

#### 当前执行记录

- 已新增 `AArrowProjectile` 原生类及其无碰撞、无 Overlap、无 Physics、无重力的 `ArrowVisual` 组件；未修改 `ACombatProjectile`、发射调用点或现有投射物数值。
- 已确认 Erika 在 `standing_draw_arrow` 中左手握弓；已实现 `AEnemy::WeaponAttachSocketName` 最小修订与 `AWeapon::Equip()` 失败清理。`BP_ErikaArcher` 已编译并明确配置 `LeftHandSocket`，不影响仍使用默认 `RightHandSocket` 的 Paladin。
- 已完成轻量静态检查与针对 `UStaticMeshComponent` 原生子对象、`SetupAttachment`、`ProjectileMovement` 和无碰撞表现层的 server-memory 查询；无匹配既有错误记录。
- 用户已将 `BP_ErikaArrow` 重设为 `AArrowProjectile` 子类并配置原生 `ArrowVisual=SM_Arrow`；手动编译后 MCP 已确认其 Collision、Overlap、Physics、Gravity 均为关闭状态。
- MCP 已确认 `AM_Attack_ErikaArcher_Ranged` 已修复为一次性 `Draw -> AimHold -> Release`，且 `AimHold` 未循环；`AM_Death_ErikaArcher.Enable Auto Blend Out` 已关闭，死亡 Sections 为 `Forward` 与 `Backward`。仍需在 Editor 人工确认 Release 帧只有一个 `AnimNotify_EnemyProjectileRelease`、收招末尾只有一个 `AnimNotify_EnemyAttackEnd`。
- MCP 已确认 `AM_HitReact_ErikaArcher` 的 `FromFront` Section 与 `DA_EnemyHitReaction_ErikaArcher` 的四方向显式回退均已完成，无需重复配置。`BP_ErikaBow.Mesh` 已是无碰撞、无 Overlap、无 Physics、无重力；`BP_ErikaArcher` 已设置 `WeaponClass=BP_ErikaBow` 与 `WeaponAttachSocketName=LeftHandSocket`，并继承正确的 `AAIController` / `PlacedInWorldOrSpawned` 接管配置。
- 用户已删除 `ABP_Paladin` 中未接入死亡流程的遗留 `dead` State 与两条 Transition；Paladin 和 Erika 的死亡仍统一由 C++ `PlayDeathMontage()` 启动，死亡 Montage 保持末帧。`BP_ErikaArcher` 已使用本阶段距离环 `CombatTooCloseRadius=600`、`CombatAttackMaxRadius=1100`、`CombatPreferredMinRadius=700`、`CombatPreferredMaxRadius=900`、`CombatingRadius=1250`、`CombatExitBuffer=100`、`ChasingRadius=1800`，并已在 TestMap 放置一只正式 Actor。`CombatTooCloseRadius=600` 只控制无 Pending 攻击/冷却中的拉扯退距；当前真实的远程出手下限仍由条目 `MinDistance=500` 决定，并非 D-C 的转身 Escape 阈值。`DA_EnemyAttack_ErikaArcher` 当前为 `MinCooldown=2`、`MaxCooldown=4`、`500-1100 cm`、`Damage=10`、`Poise=1`、`Initial/MaxSpeed=2500 cm/s`、`CollisionRadius=15 cm`、`MaxLifetime=3 s`，物理最大飞行距离约 `7500 cm`，不扩大 `1100 cm` AI 出手范围。已修复原生 `AEnemy` 构造器在 Blueprint 默认值覆盖前复制感知范围的问题：保留 Sense Config 子对象引用，并在 `PostEditChangeProperty()` 与 `BeginPlay()` 将每个敌人自身作者化的 `ChasingRadius`、`VisionAngleDegrees`、`HearingRange` 同步回 Perception Config；用户 PIE 已验证发现范围，后续 MCP 复核 CDO 也显示 `SightRadius=1800`、`LoseSightRadius=2000`、`HearingRange=800`。该同步不写入 Erika 专用数值，Paladin 继续使用自己的默认范围。核心投射物、弓握持、Release 位置与飞行箭视觉已由用户通过。
- 严格 review 从 PIE 日志发现：仅在投射物侧调用 `IgnoreActorWhenMoving(LaunchAttacker)` 不能阻止发射者胶囊主动撞上自己的箭，Erika 曾因此出现 `is stuck and failed to move`。已在 `ACombatProjectile::BeginPlay()` 补充 Owner/Instigator 根 `UPrimitiveComponent` 对该投射物的反向移动忽略；用户重新编译和 PIE 已通过，未再复现卡步。
- 用户发现 Projectile Montage 期间身体朝向冻结、但 Release 仍实时追踪玩家眼点的问题。已实现：`EES_Attacking` 仅当活动投射物快照有效时继续 `TickCombatFacing()`；`FEnemyAttackEntry::ProjectileTargetHeightOffset` 被复制进快照，`ResolveProjectileTargetLocation()` 同时服务 LOS 与 Launch Direction。用户已编译、将 Erika 条目配置为 `-30 cm` 并完成横向移动、胸部瞄准、近战 Paladin 不持续追踪的 PIE 回归。
- 已接受后续体验目标：D-B 完成后另开 `TODO-04D-C: Archer Spacing And Escape v1`。它只对已选择 Projectile 攻击条目的远程敌人在局部 Combat HFSM 中启用 Escape 子状态，保留 `AEnemy` 为唯一基类且不改变 Paladin 的近战路径；初始设计为低于 `300 cm` 进入、离开至 `650 cm` 才退出，逃离期间改为朝移动方向转身、禁止攻击/Release，导航失败则降级回现有斜后撤。
- **本轮严格 review 结论：通过，无新增 P0/P1/P2。** 正常 review 确认持续朝向仅在有效 Projectile 快照存在时执行；Target Height Offset 在攻击开始时冻结，且 LOS 与真实发射共用同一目标 resolver；`TryReleaseConfiguredProjectileAttack()` 继续以状态、目标、距离、LOS 和一次性 guard 阻断迟到 Notify。对抗性 review 确认近战条目不会建立快照，Paladin 保留原攻击期早退；死亡、受击、破防、Encounter Dormant 与 `EndPlay` 都会清空快照和 Release Timer；Owner/Instigator 的反向移动忽略只作用于发射者根碰撞体与自身箭，未放开墙体、目标 Pawn 或其他投射物的正常阻挡。用户已完成本轮编译和 PIE；本地 `Saved/Logs/Test.log` 记录两次 Erika Release 均进入现有盾牌格挡结算，未出现此前 `is stuck and failed to move`。Editor 已正常退出，因此本轮未再通过 MCP 二次读取 `-30 cm` 字段；该字段已由用户的可见胸部瞄准 PIE 结果覆盖，属于只读证据缺口，不是运行时风险。

#### 用户 Editor 作者化顺序

1. 在非 PIE 状态保存当前项目作为恢复点；不手改 `.uasset` 文件。
2. 已确认 `standing_draw_arrow` 为左手握弓。待 `WeaponAttachSocketName` C++ 修订编译通过后，打开 `SK_ErikaArcher` Skeleton，在真实 `lefthand` 骨骼上建立名称严格为 `LeftHandSocket` 的 Socket；在实际拉弦且 Release 时松开的手上建立 `ArrowReleaseSocket`。两者必须按当前动画实际姿势判断，不按角色惯用手猜测。
3. 导入弓 Mesh 后，创建 `BP_ErikaBow`，父类选 `AWeapon`。在其 `Mesh` 组件设置弓模型；保持 Mesh 无碰撞、无物理模拟、无武器 Trace Notify。当前 Erika 的 `LeftHandSocket` 是最终握持关系，`EquipRotationOffset` 与 Mesh Relative Transform 均先保持零值；若仅该导入 Mesh 的 Pivot/Scale 有问题，只调 Mesh Relative Transform；若整个 `AWeapon` Actor（含根、特效或未来附属组件）需要相对 Socket 修正，才只调 `EquipRotationOffset`。不得叠加三层变换来掩盖朝向问题，也不新增碰撞盒或攻击脚本。
4. 将 `ArrowReleaseSocket` 调至拉弓姿势下箭尾/弓弦的实际离手位置，并保存 Skeleton；不要把 `LeftHandSocket` 或 `ArrowReleaseSocket` 只加在弓 Mesh 上。
5. 在 `AArrowProjectile` 编译成功后，将 `BP_ErikaArrow` 的 Parent Class 改为 `AArrowProjectile`。在 Components 面板选中原生 `ArrowVisual`，设置为 `SM_Arrow`，只校正相对位置、旋转与缩放，使箭头正方向随 Projectile 的 +X/速度方向飞行；其 Collision、Overlap、Physics 和导航影响已由 C++ 固定。不要在子蓝图添加 Projectile Movement、碰撞体、伤害或 BeginPlay 脚本。
6. 在 `AM_Attack_ErikaArcher_Ranged` 使用同一 `DefaultSlot`，按线性顺序放入 `standing_draw_arrow -> standing_aim_overdraw -> standing_aim_recoil`。创建 Sections：`Draw`、`AimHold`、`Release`；Association 为 `Draw -> AimHold -> Release`，不循环 `AimHold`。在 `Release` 的箭实际离手帧放置且只放置一个 `AnimNotify_EnemyProjectileRelease`；在恢复末尾放置一个 `AnimNotify_EnemyAttackEnd`。
7. 配置 `AM_HitReact_ErikaArcher` 与 `DA_EnemyHitReaction_ErikaArcher`。最低可验收方案是建立一个 `FromFront` Section，四个方向字段均明确填 `FromFront`；这不是隐式降级。若已有合适方向动画，可再建立 `FromBack`、`FromLeft`、`FromRight` 并逐项对应。每条实际受击路径末尾放置 `AnimNotify_EnemyHitReactEnd`。
8. 配置 `AM_Death_ErikaArcher` 和 `DA_EnemyHitReaction_ErikaArcher.Death`。指定死亡 Montage 与存在的 Section；关闭 Death Montage 的 Auto Blend Out，使最终死亡姿势保持至 Actor 销毁。死亡 Montage 不放置 AttackEnd 或 ProjectileRelease Notify。
9. 已配置 `DA_EnemyAttack_ErikaArcher` 为一条正式 Projectile 条目：`AttackName=ErikaArrow`、`Montage=AM_Attack_ErikaArcher_Ranged`、`StartSection=Draw`、`DeliveryType=Projectile`、`DamageMultiplier=1`、`BlockStaminaDamageMultiplier=1`、`bCannotBeParried=false`、`MinCooldown=2`、`MaxCooldown=4`、`MinDistance=500`、`MaxDistance=1100`、`Weight=1`、`ProjectileClass=BP_ErikaArrow`、`ProjectileSpawnSocketName=ArrowReleaseSocket`、`bUseMotionWarping=false`。Delivery Config 为 `Damage=10`、`PoiseDamage=1`、`BlockStaminaDamageMultiplier=1`、`bCanBeParried=false`、`InitialSpeed=2500`、`MaxSpeed=2500`、`CollisionRadius=15`、`MaxLifetime=3`。
10. 在 `BP_ErikaArcher` 设置 Mesh=`SKM_ErikaArcher`、Animation Mode=`Use Animation Blueprint`、Anim Class=`ABP_ErikaArcher`、`WeaponClass=BP_ErikaBow`、`WeaponAttachSocketName=LeftHandSocket`、`EnemyAttackConfig=DA_EnemyAttack_ErikaArcher`、`HitReactionConfig=DA_EnemyHitReaction_ErikaArcher`。沿用 `BP_BaseEnemy`/Paladin 已验证的 AI Controller 和 Auto Possess AI 配置，不另建射手 Controller。
11. 已为射手设置一套远程距离环：`CombatTooCloseRadius=600`、`CombatAttackMaxRadius=1100`、`CombatPreferredMinRadius=700`、`CombatPreferredMaxRadius=900`、`CombatingRadius=1250`、`CombatExitBuffer=100`、`ChasingRadius=1800`。`CombatTooCloseRadius` 是冷却/无 Pending 意图时的拉扯阈值；Projectile 条目的 `MinDistance=500` 才是本阶段禁止 Release 的下限。该范围环是远程条目的专用配置，不要为了通用近战 Tooltip 把 Preferred 环抬到 1100 以上。
12. 在 `TestMap` 选择可达 NavMesh、开阔且不干扰剑/盾/火堆、PlayerStart 与用户 Encounter WIP 的位置，放置一只 `BP_ErikaArcher`。先单独放置，不纳入永久 Encounter Controller；D-B 只验证一只正式射手，不重复 TODO-02B 的 Encounter 作者化。

#### 验收清单

1. 编译所有新/修改 Blueprint、Skeleton、Montage、DataAsset 和 TestMap，确认没有 Parent Class、Socket、Montage 或 Attack Config warning。
2. Erika 闲置、四向 BlendSpace 移动和追击外观正常；弓稳定附着在动画实际的握弓手，箭从真实 `ArrowReleaseSocket` 出现且沿飞行方向朝前。
3. 开阔区域中，Erika 仅在 500-1100 cm 的窗口内面对玩家发射一枚；每次攻击只生成一支箭，命中沿用 04A 的伤害、受击和韧性。
4. 装备盾牌正面面对箭：现有格挡扣体力、无普通受击硬直。墙体遮挡时 Erika 不穿墙发射，会侧移/重定位；贴近 `500 cm` 内时后撤或斜后撤，而不切近战或贴身射击。`500-600 cm` 仍是当前合法射击窗；`600 cm` 是冷却拉扯阈值，不应误读为硬性禁射距离。
5. 在 Release 前打断、击杀 Erika 或停止 PIE：不会发射迟到箭。死亡姿势不会在 Actor 销毁前自动回落到 locomotion Idle。
6. 重新进入 PIE/地图重载后，无残留箭、Timer、碰撞或存档改动；Paladin、`ProjectileDebugFire`、近战、盾牌和现有 Encounter WIP 不回归。
7. **P1 修复回归**：让 Erika 在发箭后立刻继续前压、侧移和后撤，重复多次。她的胶囊不得再被自己的 `BP_ErikaArrow.CollisionSphere` 挡住，不得出现 `is stuck and failed to move`；玩家、`ProjectileDebugFire` 和玩家弓的 Owner/Instigator 忽略仍保持正常。
8. **瞄准修订回归**：在 Erika 的 Draw/AimHold/Release 期间持续横向绕行。Erika 的身体必须以现有旋转速度持续面对玩家，箭的 LOS 调试线与实际飞行均应指向胸部而非颈部；Melee Paladin 攻击期间不得获得这种持续跟踪转向。

#### 文档、Review 与提交边界

- 用户完成手动资产配置并保存后，先由 MCP 做一次只读复核：Skeleton Socket、`BP_ErikaArrow` Parent 与原生 `ArrowVisual` Details、BP Parent/Class Defaults、Montage Notify/Section、DataAsset 条目和 TestMap Actor；当前 MCP 已连通，但不以它替代用户的 Editor 编译与 PIE 验收。
- 验收通过后执行正常 review 与对抗性 review；重点检查 Socket 归属、一次性 Release、无限 AimLoop、死亡姿势、原生箭表现组件无碰撞、DataAsset/Blueprint 引用和 TestMap 范围。
- 通过后更新 `ARCHITECTURE.md`、将 D-B 移入 `ROADMAP.md` Done Milestones；README 默认不改，除非正式玩家可见射手需要公开说明。提交包含 D-B 的窄箭表现 C++、作者化资产、`Content/Fab/Survival_Kit_-_Bow/` 下的五项已核对 Bow 依赖，以及地图必要 External Actor `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/C/GG/A3OYSKXOUMFGPZ2YEKXTZL.uasset`；持续排除 `WBP_PauseMenu.uasset`、`WBP_OverwriteConfirmation.uasset` 和无关 Encounter WIP `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/E/RS/`。
