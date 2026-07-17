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

### TODO-04D-B0: Shared Character Animation Data v1

**状态：已完成。用户已编译 `TestEditor` 并完成 PIE 验收；正常 review 和对抗性 review 均未发现 B0 阻塞项。`TODO-04D-B` 的已创建 Erika 资产保留，正式作者化可在弓 Mesh、Socket 对位和 TestMap 摆放前置满足后恢复。**

#### 目标与成功标准

- 创建 `UBaseCharacterAnimInstance`，只从 `ABaseCharacter` 与 `UCharacterMovementComponent` 读取统一动画数据：`GroundSpeed`、`Direction`、`IsFalling` 与 `ZSpeed`。
- `USlashAnimInstance` 改为继承该基类，并只维护玩家特有的 `WeaponState`、`bIsBlocking` 与 `bIsStunning`。
- Paladin 和 Erika 的 AnimBP 直接使用 `UBaseCharacterAnimInstance`，不再各自在 Event Graph 重复计算速度和方向。
- 玩家、Paladin 与 Erika 的 locomotion 和既有攻击/受击/死亡 Montage 均保持原行为。

#### 已锁定的范围与边界

- 这是 `ABaseCharacter` 级的共享动画数据，不创建 `ARangedEnemy`、`AMeleeEnemy`、共享敌人 AnimBP 资产或跨 Skeleton 的 AnimGraph 父类。
- 只同步已存在的稳定移动数据；`ABaseCharacter::Tick()` 已是 `GroundSpeed` 与 `Direction` 的唯一计算源。AnimInstance 不得再次根据自身 Velocity 产生第二套结果。
- 不在本阶段缓存 `EEnemyState`、死亡、受击硬直、攻击或 Encounter Dormant。当前攻击、受击与死亡由 C++ 状态和 Montage 驱动；过早复制会形成可能陈旧的第二状态源。
- `UHitReactionConfigDataAsset` 继续拥有 Hit React Montage 与 `FrontSection`、`BackSection`、`LeftSection`、`RightSection`。单一通用受击动画通过把四个字段明确指向同一 Section 实现，不增加隐式 DefaultSection 降级。
- 不改 `AEnemy` HFSM、投射物、SaveGame、奖励、输入、武器、骨骼 Socket 或 TestMap。

#### 执行顺序

1. 先做 C++ 静态检查与针对 `UAnimInstance::NativeInitializeAnimation`、`NativeUpdateAnimation`、反射属性继承和 AnimBP Parent Class 迁移的 server-memory 查询；用户手动编译 `TestEditor`，代理不运行 UBT。
2. 新建 `UBaseCharacterAnimInstance`，缓存 `ABaseCharacter` 与 `UCharacterMovementComponent`；在 `NativeUpdateAnimation()` 中读取角色 Getter 和移动组件，只暴露通用只读动画变量。
3. 精简 `USlashAnimInstance`：调用 `Super`，保留玩家专属缓存与状态同步，删除重复的移动变量、重复角色移动缓存和重复速度/方向更新。
4. 在 Editor 中依次将 Paladin 与 `ABP_ErikaArcher` 的 Parent Class 设为 `UBaseCharacterAnimInstance`，编译保存；Erika 的 `Direction` / `GroundSpeed` 使用继承变量，删除当前手工变量和 Event Graph 更新节点。
5. 用户 PIE 回归：玩家移动、跳跃/下落、剑盾/弓动作；Paladin 前压、后退、受击与死亡；Erika BlendSpace 样本切换及 Montage Slot。正式弓 Mesh、Socket、TestMap 摆放仍留给恢复后的 D-B。
6. 通过编译、PIE、正常 review、对抗性 review 后，再更新稳定 `ARCHITECTURE.md`，将 B0 移入 `ROADMAP.md` Done Milestones，并单独请求提交批准。

#### 完成记录

- 已新增 `UBaseCharacterAnimInstance`：缓存 `ABaseCharacter` 与 `UCharacterMovementComponent`，读取唯一的 `ABaseCharacter::GroundSpeed` / `Direction` 以及落地、垂直速度数据；无 Owner 时清空输出，避免 Pawn 替换后的旧动画值残留。
- 已将 `USlashAnimInstance` 迁移为该基类的玩家专属派生，只继续同步 `WeaponState`、`bIsBlocking`、`bIsStunning`。
- 已将 `ABP_Paladin` 与 `ABP_ErikaArcher` 设为根 `UBaseCharacterAnimInstance` AnimBP；DarkKnight 的 MainState 和 IK Linked AnimGraph 保持 `UAnimInstance`，继续通过 Exposable Properties 接收根 AnimBP 数据。
- Erika 已删除本地 `Direction` / `GroundSpeed` 和 Event Graph 重复更新；Paladin 的 `ActionState -> BisDead` 线程安全死亡路径保持不变。
- 用户已验证玩家移动/跳跃、剑盾攻击/格挡/受击/死亡，Paladin 移动/受击/死亡，以及 Erika locomotion Preview；正常 review 与对抗性 review 均无阻塞项。

#### 当前 Editor 接线核验

- 用户已将错误的 `Instanced Struct` 变体替换为 Object `Is Valid` 宏；实时只读 MCP 确认 `Try Get Pawn Owner` 同时连接到该 Object 输入、`Get Velocity` 与 `Get Actor Rotation`，并确认 Event Update 的有效分支依次驱动 `Set GroundSpeed -> Set Direction`。Editor 编译成功。
- `Locomotion/Idle` 中 `BS_ErikaArcher_Locomotion` 的 `Direction` 和 `GroundSpeed` 输入也已显式连接，当前可作为 B0 迁移前的有效对照基线。
- 已移除这段 Event Graph 和两个局部变量，Erika BlendSpace 现只读取继承变量，避免同一移动数据存在 C++ 和 Blueprint 两个写入来源。

#### D-B 已创建资产保留与恢复条件

- 已创建：`BP_ErikaArrow`、`BP_ErikaArcher`、`ABP_ErikaArcher`、`BS_ErikaArcher_Locomotion`、`AM_Attack_ErikaArcher_Ranged`、`AM_HitReact_ErikaArcher`、`AM_Death_ErikaArcher`、`DA_EnemyAttack_ErikaArcher`、`DA_EnemyHitReaction_ErikaArcher`。
- `BP_ErikaArcher` 继续继承 `BP_BaseEnemy`，并在正式弓 Mesh 到位前保持 `WeaponClass = None`。
- D-B 恢复条件：B0 已通过玩家/Paladin/Erika 动画回归；同时具备合法可见弓 Mesh、`RightHandSocket`、`ArrowReleaseSocket` 与一个明确的 TestMap 放置位置。

#### 验证、文档与提交边界

- B0 的验证不要求地图新 Actor 或弓资产；只验证原生动画数据的初始化、更新、父类迁移和现有 Montage 回归。
- 敌人状态机保持一个名为 `Locomotion` 的基础 State，它播放完整 2D locomotion BlendSpace，并非只播放 Idle。攻击、受击与死亡继续由 C++ 发起的 Montage 经 `DefaultSlot` 覆盖；本阶段不增加第二套 `Dead` State。D-B 验收必须确认死亡 Montage 的终止姿势会保持到 Actor 销毁，不能 Auto Blend Out 后回落为 locomotion Idle。
- 已更新 `ARCHITECTURE.md`、`ROADMAP.md` 与本计划记录；README 无需更新，因为 B0 不增加玩家可见功能或操作。
- 提交只包含 B0 C++、必要的 Paladin/Erika AnimBP Parent Class 资产、`plan.md`、`ROADMAP.md` 和稳定文档；持续排除 `WBP_PauseMenu.uasset`、`WBP_OverwriteConfirmation.uasset` 与 `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/E/RS/` 用户 WIP。
