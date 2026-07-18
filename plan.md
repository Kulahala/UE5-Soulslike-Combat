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

### TODO-05A1: Combat-Aware Sprint Stamina v1

**状态：✅已执行，已通过。** Shift 冲刺保留既有速度、Free-Run、噪音、输入与动作语义；只有运行时 Combat Presence 有效时才沿用既有的每秒体力扣除和恢复延迟重置。脱战冲刺不消耗体力，也不持续阻断体力恢复。

#### 核心契约

- Combat Presence 是 `AMyCharacter` 持有的运行时瞬态时间戳，不新增 `EActionState`、组件、Timer、SaveGame 字段、HUD、输入资产或 Blueprint 接线。默认退出滞后 `4 s`，以 `GetWorld()->GetTimeSeconds()` 比较最近有效来源时间，不创建可能跨 Pawn 或地图残留的回调。
- 敌人来源复用现有 `ATestGameMode::IsPlayerEngagedByEnemy(const AMyCharacter*)`，不再复制全场敌人扫描或另造 AI 状态。该只读查询继续以 `AEnemy::IsEngagingActor(Player)` 为准：敌人必须以当前玩家为 `ChasingTarget`，且位于 Chasing / Combating / Attacking / Stunned / StanceBreak 之一；死亡、Encounter Dormant、目标失效与巡逻/搜索不算战斗。
- 将现有 GameMode 查询提升为窄的 public C++ 只读接口，供 Checkpoint gate 与 Player Combat Presence 共用同一个敌人“正在交战”定义；不改 `AEnemy` 的状态机、感知、距离、Encounter 或 Checkpoint 行为。
- 命中来源只在 `FCombatHitResolver::ResolveAndApply()` 已确认 `bResolved && !bSuppressed && !bSameTeam` 后刷新，并且双方必须是 `AMyCharacter <-> AEnemy`。有效格挡、弹反和造成零伤害但已确认的敌我战斗碰撞仍算交战；挥空、同队、Dormant 抑制、无效 Actor、可破坏物或其他非敌人目标不算。
- 命中刷新入口由 `AMyCharacter` 自身再次校验存活、非 `EAS_Dead`、非火堆服务保护，避免致死命中、火堆保护或迟到 Resolver 路径把已死亡 / 正在切图的 Pawn 重新标成战斗。
- 每帧在玩家未死亡时、现有动作早退之前检查敌人交战来源；任一敌人仍满足现有查询就刷新时间戳。敌人死亡、丢失目标、进入 Encounter Dormant 后不再刷新，但此前的命中或交战仍保留完整 `4 s` 滞后；新来源在窗口内出现时只刷新时间戳，不产生状态切换抖动。
- `TickSprintStamina()` 保持现有移动、落地、`EAS_UnOccupied`、举盾、前进 / Free-Run 方向 gate 与 `12/s` 消耗速率。仅将 `UseStamina()` 与 `ResetStaminaRegenCooldown()` 一起置于 `IsCombatPresenceActive()` gate 内；不得改 `CalcBaseSpeed()`、`Sprint()`、`StopSprinting()`、`ShouldUseSprintAttack()`、移动噪音或锁定 Free-Run，所以脱战 Shift 仍跑同样快、仍有相同听觉噪音。
- 玩家死亡、`EndPlay`、重生得到的新 Pawn、Continue、地图重载和 PIE 结束都清空 Presence。没有持久化或延迟 Timer，因此不能跨死亡、重载或 PIE 留存。暂停期间沿用当前世界时间与 Tick 暂停语义，不额外运行 Presence。

#### 实施范围

1. 在 `AMyCharacter` 增加 `CombatPresenceExitDelay = 4.f` 的 `EditDefaultsOnly` 玩家调参、最近来源时间、`MarkCombatPresence()`、`ClearCombatPresence()`、`RefreshCombatPresenceFromEnemyEngagement()`、`IsCombatPresenceActive()` 及仅 Player Debug 下的剩余时间文本。状态只允许 C++ 读取，不暴露给 UMG 或 SaveGame。
2. 在 `Tick()` 的暂停检查之后、动作状态早退之前刷新敌人交战来源；在 `Die()` 和 `EndPlay()` 中清理。命中入口仅由 Resolver 调用，不在 `TakeDamage()`、攻击输入、锁定、感知回调或普通 Tick 中猜测“刚刚战斗过”。
3. 让 `FCombatHitResolver` 在既有一次性伤害、格挡、韧性、受击和破防处理完成后，以窄 helper 标记本次 Player / Enemy 敌对命中；不新增或改写 `FCombatHitRequest`、`FCombatHitResult`、投射物、Weapon、`IHitInterface` 或伤害结算顺序。
4. 将 `ATestGameMode::IsPlayerEngagedByEnemy()` 的现有声明提升为 public C++ 查询；实现和 Checkpoint 调用保持不变。`AMyCharacter` 只在当前世界确实使用 `ATestGameMode` 时调用它；缺少该 GameMode 时退化为“仅命中来源可进入 Presence”，不输出每帧 warning。

#### 非目标

- 不把锁定、镜头、敌人视野、听觉感知、敌人血条、攻击按键、挥空、离敌距离或地图区域当作 Combat Presence 的替代来源。
- 不改变脱战冲刺速度、Sprint Attack、锁定 Free-Run、Run / Walk 噪音、攻击/翻滚/格挡/跳跃/弓瞄准/Guard Break 的体力规则，也不让敌人或 GameMode 写玩家体力。
- 不新增 Combat HUD、图标、控制台命令、DataAsset、Editor 资产、动画、地图 Actor、网络/GAS 或“战斗中”通用状态机。

#### 实施与验证顺序

1. 实施前核对 `AMyCharacter::TickSprintStamina()`、`AEnemy::IsEngagingActor()`、`ATestGameMode::IsPlayerEngagedByEnemy()`、共享 Resolver 以及死亡 / EndPlay 调用链；执行轻量静态检查和 server-memory 查询，关键词覆盖 `GetWorld()->GetTimeSeconds`、`FCombatHitResolver`、`AEnemy::IsEngagingActor`、`EEndPlayReason` 与体力恢复 delay。
2. 完成上述 C++ 窄改动后，由用户手动编译 `TestEditor`。本阶段没有 `.uasset` 或 Live Editor MCP 作者化工作，不运行 UBT、打包或自动 PIE。
3. PIE 验收：无敌人主动交战时满体力持续 Shift 冲刺，速度、Free-Run 与噪音保持原样，体力不下降；若此前体力未满，旧的最后一次耗体延迟自然结束后，即使仍按 Shift 也能恢复。
4. 让 Paladin 或 Erika 实际以玩家为目标进入 Chasing / Combating，且玩家已在持续冲刺：当帧或下一帧开始按既有 `12/s` 扣体并重置恢复延迟，不需要锁定；敌人不再主动交战后，扣体保留不多于 `4 s`，随后仍按 Shift 但不再扣体。
5. 不锁定时命中敌人、被敌人近战 / 箭命中、成功格挡、成功弹反分别验证都刷新 Presence；挥空、打碎环境物、同队命中、Dormant Encounter 敌人不能刷新。目标死亡、丢失目标、Encounter Dormant、被击杀后新 Pawn、Continue、地图重载和重新 PIE 均不得遗留旧 Presence。
6. 回归 Checkpoint gate 仍只使用当前敌人主动交战查询，不继承玩家 `4 s` 余温；回归普通冲刺、Sprint Attack、锁定 Free-Run、体力 Exhausted、Guard Break、剑盾格挡、Erika / Paladin 战斗和 Projectile 命中。
7. 用户确认 PIE 后执行正常 review 与对抗性 review，重点检查：敌人 / 命中来源是否完整但不误报、4 秒滞后、进入战斗时已按住 Shift、死亡和 Pawn 生命周期清理、Resolver 单次结算边界、Checkpoint 语义隔离与无锁定依赖。

#### 文档与提交边界

- 通过验证与两轮 review 后，更新 `ARCHITECTURE.md` 的玩家体力 / Combat Presence 合同，将 `TODO-05A1` 移入 `ROADMAP.md` Done Milestones，并按本文件头部规则收尾。`README.md` 默认不改。
- 提交仅包含 `AMyCharacter`、`FCombatHitResolver`、必要的 `ATestGameMode` 声明、`plan.md`、`ARCHITECTURE.md` 与 `ROADMAP.md`。持续排除 `DA_EnemyAttack_ErikaArcher.uasset`、`Content/__ExternalActors__/_GAME/BP/Maps/TestMap/C/GG/A3OYSKXOUMFGPZ2YEKXTZL.uasset`、`CombatMasterAnimBundle` 动画、暂停菜单、覆盖确认和其他用户 WIP。

#### 当前执行记录

- 已完成 C++ 实施，改动限于 `AMyCharacter`、共享 `FCombatHitResolver` 与 `ATestGameMode` 的既有敌人交战查询可见性；无 `.uasset`、地图、输入、SaveGame 或 UI 改动。
- `AMyCharacter` 以 `LastCombatPresenceTime` 保存当前 Pawn 的运行时来源时间，并以 `CombatPresenceExitDelay = 4 s` 查询。敌人主动交战在 Tick 的动作早退前刷新；共享 Resolver 只在已结算、非同队、非抑制的 `AMyCharacter <-> AEnemy` 命中后刷新。待命敌人无论作为受击者或攻击者都不能刷新。
- `TickSprintStamina()` 保留原有速度、地面、举盾、前向 / Free-Run 与 `12/s` gate，仅在 Presence 有效时执行既有 `UseStamina()` 和 `ResetStaminaRegenCooldown()`；`CalcBaseSpeed()`、冲刺输入和噪音未改。
- 玩家死亡、火堆服务保护与 `EndPlay()` 显式清空 Presence；静态检查与 `git diff --check` 已通过。server-memory 查询未发现匹配的历史错误模式。
- 阶段严格 review 已完成：常规检查确认敌人交战、共享 Resolver 命中、耐力单一扣除路径、火堆 gate 与 Pawn 生命周期边界正确；对抗性检查确认正式 Weapon/Projectile 投递均以角色写入 `Request.Attacker`，敌人死亡、Dormant、有效格挡/弹反、晚到投射物和四秒滞后不会破坏该契约。没有剩余 blocker，用户已批准文档收尾与提交。
