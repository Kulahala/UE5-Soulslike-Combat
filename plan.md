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

### TODO-04B-B: Bow Presentation And World Pickup v1 - ✅已执行 / 已通过

**目标**：完成可见玩家 Bow、已装填箭视觉、`Loaded / Capacity` HUD，以及只写入 Reserve 的 20 支箭固定世界拾取。现有 Bow 发射、双手占用、Rest 装填和持久化装备合同保持不变。

**执行与验证状态**：

- C++ 已由用户手动编译；已确认在 Loaded Arrow 存在时正常 RMB 瞄准、LMB 按下/松开发射。运行日志也记录了成功 Release。
- 用户已完成世界 Bow / 箭束、失败回滚、真实 Rest、持久化与放箭 PIE 验收。Live Output Log 额外确认：`TestMap_DarkKnightBowPickup` 与 `TestMap_DarkKnightArrowBundlePickup` 均已写入 `ClaimedRewardIds`；`ItemDebugFailNextClaimSave` 先注入箭束 Save 失败，之后成功写入 `Loaded=0/20`、`Reserve=20`，并记录多次成功 Bow Release。当前 Editor 位于 `MainMenu`，因此不将该时刻的 `TestMap` 零 Actor 查询误判为地图未放置拾取物。
- 用户已为 Bow 与 Arrow Definition 配置 `PickupSound`；待下一次成功领取各复验一次声音与无缺失音效 warning。
- P2 C++ 已实施并完成 CodeGraph 回读、旧符号搜索与 scoped `git diff --check`：`ShotCooldown` 已从 `ABow`、角色计时器和 Blueprint CDO 移除。玩家放箭改为 `Prepare -> 成功启动必填 Release Montage -> Consume -> Commit`；Release Montage 的物理播放状态（含取消后的混出）是唯一再射门卫。消费存档失败时只停止本次 Release 表现、恢复已装填箭视觉并继续保持瞄准。用户已手动编译 `TestEditor`、编译保存 `BP_DarkKnightBow` 并完成 PIE 验收。
- 用户已确认本轮编译与 PIE 通过。VibeUE 的只读日志筛选未发现 Bow 领取、Ammo Claim、Prepared Projectile 或 Release Presentation 失败；最新会话记录正常 Bow Release 与投射物墙体停止。日志不能单独重建快速点击的输入序列，快速连点通过以用户手动验收为准。
- Live Editor 只读复核确认 `BP_DarkKnightBow : ABow` 依赖 Bow Mesh、箭视觉、`BP_ErikaArrow`、Draw / Release Montage；`BP_DarkKnightArrowBundle : AAmmoPickup` 与 `WBP_PlayerHUD.Text_ArrowCount` 均已编译。HUD 文本是可绑定变量，默认 Collapsed，运行时由 C++ 推送显示。
- 已复核用户提供的 `ABP_DarkKnight` 截图：`ABP_DarkKnight_MainState` 的 `Exposable Properties` 通过属性绑定传入 `GroundSpeed`、`ZSpeed`、`IsFalling`、`Direction`、`EweaponState` 和 `IsBowAiming`。此前 MCP 图查询只显示 Pin 连线、未显示 Details 面板绑定，因此“MainState 未接收数据”的 review finding 已撤回。
- `TODO-04B-B3` 已登记为 B-B 后的下一阶段：先评估现有非 Additive `Bow_Idle_Aim_BlendSpace` 的上半身层复用、非锁定瞄准的 Control Yaw 朝向与 DarkKnight 专用左手掌 Bow Socket、Bow/箭/Spawn Point 对齐；必要时才创建 Mesh Space Additive Aim Offset。`TODO-04B-D` 已登记为独立的 Bow Profile DataAsset 阶段；不把玩家输入/弹药事务与敌人 AI Attack DataAsset 混成一个入口，也不把 Bow Montage 塞进通用 ActionConfig。

**严格 Review：已通过**：

1. **[P2] Montage-only Release gate。** `ABow::ShotCooldown`、`bBowReleaseOnCooldown` 和其 Timer 已删除；`ReleaseBowArrow()` 只会在已准备投射物后成功启动有效、未在物理播放中的 `ReleaseMontage` 时，才消费 Loaded Ammo 并 Commit。取消瞄准会清理逻辑 pending 状态并请求 `0.12 s` 混出，但下一次发射仍由 `Montage_IsPlaying(ReleaseMontage)` 拒绝，直到旧 Montage 实际结束。`ReleaseMontage`、AnimInstance 或播放失败均在消费/Commit/音效前拒绝；消费存档失败会销毁 Prepared Projectile、只混出当前 Release Montage、恢复箭视觉且保持 `EAS_Aiming`。

- **正常审查**：未发现 P1/P2 或行为回归。CodeGraph 回读确认 Release 只经 `Prepare -> TryStartBowReleasePresentation -> TryConsumeLoadedAmmo -> Commit`；旧 ShotCooldown/Timer 符号不存在，所有瞄准取消入口仍统一走 `CancelBowAim()`。`git diff --check` 通过。
- **对抗性审查**：物理 `Montage_IsPlaying(ReleaseMontage)` 在逻辑 pending 被取消后仍是唯一重入门卫；Save 失败注入会在 Commit 前回滚存档容器，P2 只停止本次 Montage 并不退出瞄准；Prepared Projectile 的原生无碰撞预备态在失败时销毁，无法造成命中或生命周期残留。
- **用户验收与只读证据**：用户已手动编译与 PIE 通过。Output Log 记录 `BP_DarkKnightBow` 编译、保存，普通放箭，`BowDebugFailNextProjectilePrepare` 的 Commit 前丢弃，以及 `BowDebugFailNextAmmoConsumeSave` 后的 `Injected loaded-ammo save failure` 与后续正常重试。只读 Blueprint 查询确认其父类为 `/Script/Test.Bow`，依赖中保留 `AM_DarkKnightBow_Release`。
- **非阻塞备注**：当前 Output Log 没有保留临时清空 `ReleaseMontage` 时的 warning；该条以用户手动 PIE 通过为验收证据，不将缺少日志片段误写成引擎自动证明。

**已完成基线与本轮补充验收**：

1. 在 Editor 的 `TestMap` 初始火堆附近放置 `BP_DarkKnightBow` 与 `BP_DarkKnightArrowBundle`。在继承的 `Persistent Pickup` 分类分别填写 `TestMap_DarkKnightBowPickup` / `Item_DarkKnightBow` / `1` 和 `TestMap_DarkKnightArrowBundlePickup` / `Item_DarkKnightArrow` / `20`，保存地图与 External Actor。
2. 使用尚未领取这两个 PersistentId 的存档开始 PIE；先输入 `ItemDebugDump`，确认 Runtime 与 Saved 输出均未包含这两个 Claimed Reward ID。若当前存档已经领取过，使用明确的新游戏存档或临时未使用的测试 ID，不要在已领取 ID 上判断拾取是否失效。
3. 先领取 Bow：世界 Actor 消失且只领取一次。若 MainHand 为空，它应自动装备到左手并运行时抑制盾牌；若 MainHand 已有剑，Bow 应只加入拥有列表，随后在火堆装备它。装备 Bow 后 HUD 显示 `0 / 20`，除非该存档原本已有 Loaded Arrow。
4. 在第一次成功领取箭束前输入 `ItemDebugFailNextClaimSave`，然后交互箭束。预期 Actor 保持可见/可交互、没有新增 Reserve 或 Claimed Reward ID；再次交互才成功。成功后 `ItemDebugDump` 应显示 `Item_DarkKnightArrow` 的 `Loaded=0/20`、`Reserve=20`，箭束消失且不能重复领取。
5. 在火堆执行真实 Rest，不以死亡、Continue 或重新 PIE 代替。预期 `ItemDebugDump` 和 HUD 都转为 `Loaded=20/20`、`Reserve=0`；按住 RMB 时出现准星和已装填箭，LMB 按下/松开可正常发射。
6. 成功领取后停止 PIE 并用同一存档再次进入：两个世界拾取物都不应重新出现，`ClaimedRewardIds` 与 Bow/Ammo 状态保持。确认普通剑盾、Erika 与 Paladin 不回归。
7. 手动编译本次 Montage-only Release gate 修复后，确认 `BP_DarkKnightBow` 已移除 `ShotCooldown`、`ReleaseMontage` 仍为 `AM_DarkKnightBow_Release`，然后编译并保存该 Blueprint。至少 3 支 Loaded Arrow 下持续 RMB 瞄准，快速完成多次 LMB 按下/松开：每段 Release Montage 最多 Commit 一箭，下一支箭只能在 Release 结束且重新上箭视觉出现后发射。发射后立刻松开 RMB、重新瞄准并在旧 Montage 混出期间松开 LMB：不得有第二箭、扣箭、ShotSound 或新的 Release 表现。`BowDebugFailNextProjectilePrepare` 必须不启动 Release；`BowDebugFailNextAmmoConsumeSave` 必须只让刚启动的 Release 混出、保留箭数并恢复箭视觉；临时清空 `ReleaseMontage` 时必须 warning 且不发射、不扣箭、不播声音，最终保存前恢复该资产。
8. 使用未领取的测试 ID 成功领取一次 Bow 与 Arrow Bundle，确认两次均有新配置的拾取声音；再通过 Output Log 复核没有新的 `completed without a PickupSound`。

**静态结论**：Reserve + `ClaimedRewardIds` 的世界箭束写入在同一次 `SaveNow()` 前保存旧数组并共同回滚；Prepared -> Consume -> Commit 仍保持发射事务单向性，且 P2 在 Consume 前加入不可回退的 Release Montage 启动门卫。HUD 和 Loaded Arrow Visual 都由角色事件/生命周期单向推送，没有新增轮询或 Notify 驱动发射。地图拾取物闭环、PickupSound 已由用户 PIE 通过；P2 等待本轮 Montage-only 回归验收。准星驱动瞄准、非锁定 Control Yaw 朝向与弓握把对齐明确留在 B-B3，不应混入 B-B 收尾。

**持续排除**：`Anim_PS_Hit_Front_Middle_RM.uasset`、`SKM_ErikaArcher.uasset`、`ABP_DarkKnight_IkTrace.uasset`、`DA_EnemyAttack_ErikaArcher.uasset`、现有 TestMap/Encounter External Actor WIP 与 `ROADMAP.md` 的 05B 分组调整。
