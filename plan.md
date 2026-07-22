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

### TODO-04B-C0: Skeletal Bow String And Nocked Arrow Socket Presentation v1

**状态：✅已执行，已通过**

- C++：在 `ABow` 增加无碰撞 `BowSkeletalVisual`、仅表现用 `EBowPresentationState`，并将已装填箭 Anchor 接至固定 `BowArrowSocket`；新增 `UBowAnimInstance`，统一由 Bow 自身 AnimBP 从其拥有的 `ABow` 只读同步表现状态，避免每把玩家 Bow 重复 Event Graph 状态桥接。
- 角色：只在既有 Aim、Release、回滚、Montage End 与取消路径单向同步 Bow 表现状态；不改 `Prepared -> Consume -> Commit`、冷却门、弹药、伤害、投射物或敌人 Bow。
- Editor：用户已授权直接修改 `SK_Bow`；创建 Mesh-only `BowArrowSocket`（`Bow_Arrow_Slot`），新建并改父类为 `UBowAnimInstance` 的 `ABP_DarkKnightBow`，配置 `BP_DarkKnightBow.BowSkeletalVisual` 并隐藏旧静态 Bow 渲染。入场动画由用户按预览在普通/快速版本中选择。
- Editor 进度：`ABP_DarkKnightBow` 已继承 `BowAnimInstance`，`SM_BowPresentation` 已有五个 Sequence State 且已接入 AnimGraph；`Relaxed -> EnterAim` 使用用户作者化的命名枚举比较，`EnterAim -> AimHold` 已是自然结束规则。其余 `EBowPresentationState` 分支由用户手动接线：当前 UnrealClaude 会把非零 C++ 枚举值错误序列化为底层字节 `0`，VibeUE 没有对应的文档化枚举 Transition 写入接口；不得用数值字节比较替代命名枚举。
- PIE 结论：空 Loaded Arrow 时，RMB 仍让 Bow 进入当前拉弦表现。这不是 C0 临时补丁目标，转交 `TODO-04B-C` 的正式合同：RMB 只举弓/进入 Aim，LMB 长按才驱动 Draw/蓄力，松开 LMB 再请求发射。
- Dodge 回归已修复并经 PIE 确认：`AM_Dodge_DKM` 的 `DefaultSlot` 现由根 `ABP_DarkKnight` 的真实 `Slot(DefaultGroup, DefaultSlot)` 接收。当前图顺序为 `Layered Blend Per Bone -> DefaultSlot -> Parry -> Output`，即全身 Montage 覆盖既有分层结果后仍允许 Parry 层最后叠加；根 AnimBP 与 Bow AnimBP 的只读验证均为 `UpToDate`、零错误、零警告。严格 review 已复核该 Slot 优先级对全身 Draw / Release 与 UpperBody 行为的影响，未发现 C0 阻塞项。
- Editor 只读复核：`SK_Bow` 当前仅有一份 `BowArrowSocket`，Outer 为 `SK_Bow`、Parent Bone 为 `Bow_Arrow_Slot`，零相对 Transform；`Bow_SK_Skeleton` 不再持有同名 Socket。`BP_DarkKnightBow.BowSkeletalVisual` 已绑定 `SK_Bow` 与 `ABP_DarkKnightBow`，`LoadedArrowAnchor -> BowArrowSocket`，旧静态 `Mesh` 不渲染，Skeletal Visual 为无碰撞、无 Overlap、无物理、无重力、无导航影响。
- 严格 review：正常与对抗性源码审查已通过。C++ 状态仍由 `AMyCharacter` 单向写入，`UBowAnimInstance` 只从拥有它的 `ABow` 读取；`Prepared -> Consume -> Commit`、Montage-only 发射门、伤害、命中与敌人 Bow 未被 C0 反向接管。取消、回滚、Montage End、火堆服务与 `EndPlay` 都经既有状态 guard 收敛。
- 资产迁移复核：Redirector 已被正确 Fix Up。`DA_Item_DarkKnightBow` 与唯一实际 Bow Pickup External Actor `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/7/9V/Q8MSJ0REXCC1D29J63IABP.uasset` 现在直接引用新 `.../Weapons/Bow/BP_DarkKnightBow`，旧路径不再存在。该 DataAsset、该唯一 External Actor、旧路径 Bow Asset 删除与新路径 Bow Asset 是同一次已验证的资产迁移结果；其余当前 TestMap External Actor 改动及 `BP_Pladin.uasset` 删除继续视为用户 WIP，严格排除。
- 验收：用户已完成编译、资产迁移后的最终 PIE、可见性/动作回归与 Output Log 复核；BowString/箭/发射点同步、放箭与回滚、取消/受击/死亡/EndPlay 清理、剑盾与敌人回归均通过。
- 收尾：`ARCHITECTURE.md` 与 `ROADMAP.md` 已同步。提交前仍需得到用户对精确资产迁移范围的明确批准；不得将其余地图 WIP 或 Paladin 删除带入提交。
