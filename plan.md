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

### TODO-04B-B3: Player Bow Aim Orientation And Grip Presentation v1 - ✅已执行 / 已通过

**目标**：DarkKnight Bow 在任何 `EAS_Aiming` 下使角色面向 `ControlRotation` / 准星，进入临时右肩越肩镜头，并完成通用左掌武器 Socket、盾牌专用手背 Socket 与现有 Aim BlendSpace 的表现接线。

**锁定瞄准合同**：Aim 中保留锁定目标、血条标记、目标有效性清理、死亡自动换锁和滚轮切换，但暂停 Lock-On 对 Control Rotation 与 Actor Facing 的接管；鼠标可自由移动准星、镜头和角色。正常松开 RMB 后保留的目标通过既有 `RInterpTo` 平滑重新接管镜头。Aim 不清锁、不新增状态、Timer 或第二套锁定系统。

**执行清单**：

- [x] 读取 C++ Lock-On / Bow / SpringArm 流程，确认 `Input_Look()` 当前仅因 `IsLockingOn()` 拦截，`UpdateLockOn()` 前半段可保留目标维护，后半段可独立暂停。
- [x] 实施 C++：Aim 旋转缓存、锁定瞄准自由 Look、右肩 SpringArm 绝对目标、Aim/Lock 恢复保护，以及原生 `BowAimYaw` / `BowAimPitch`。已完成 CodeGraph 回读、旧相机目标符号搜索和 `git diff --check`；server-memory 对该精确模式无既有记录。
- [x] 修订 Socket 合同：`AShield` 的原生默认 `OffhandSocketName` 改为 `LeftHandShieldSocket`；`LeftHandSocket` 保持 `ABowBase` 与未来通用左手武器的掌心默认值，不创建 `LeftHandBowSocket`，也不覆写 `BP_DarkKnightBow.DefaultEquipSocketName`。
- [x] 已完成轻量静态检查与 targeted server-memory 查询：`AShield::EquipToOffhand()` 的唯一玩家调用路径保留 Socket 存在性 guard，`git diff --check` 通过；未找到原生 FName 默认值迁移 / Blueprint CDO 重置的既有记忆条目。
- [x] 用户已手动编译 `TestEditor`。
- [x] 用户已在 Editor 完成 DarkKnight `LeftHandSocket` 掌心武器握持、`LeftHandShieldSocket` 盾牌手背握持、`BP_Shield.OffhandSocketName` 迁移、Bow/Arrow/Spawn Point 对齐与真实 Bow Aim AnimGraph 接线。
- [x] 用户已 PIE 验收自由瞄准、锁定自由瞄准与回锁、目标失效/切换、右肩镜头、Bow 视觉、Projectile 与中断清理。
- [x] 严格 review 修复后复测：失败锁定后立即 Aim 不受归中拉回；锁定 Aim 中转离目标后暂停/恢复不清锁；冲刺速度为 `380`；Aim SpringArm 距离为 `295`（基础 `360` + `-65`）。
- [x] 已完成正常 review 与对抗性 review；已更新 `ARCHITECTURE.md`，并将 B3 移入 `ROADMAP.md` Done Milestones。

**严格 Review：已通过**：正常审查确认失败锁定后的残留 Camera Recenter 与暂停时的 Aim-away 误解锁均已用窄 gate 修复；无效目标仍会在暂停时清理。对抗性审查确认 Aim/Lock-On 双缓存只恢复瞄准前的底层旋转状态，锁定 Aim 只暂停相机/朝向接管而不影响目标交接，绝对 Aim 相机目标不与 Lock-On 偏移叠加，且 Release Montage-only gate、Prepared -> Consume -> Commit 与敌人 Bow 路径未被触碰。用户已手动编译并 PIE 验收。

**提交边界**：本阶段纳入 B3 的 C++、DarkKnight Skeleton Socket、`BP_DarkKnightBow`、`BP_Shield`、实际 Bow Aim AnimGraph、Aim Offset 资产和必要 BlendSpace 重命名；`ABP_DarkKnight_IkTrace.uasset`、Erika、Encounter、菜单、外部 Actor、`AGENTS.md`、`DA_EnemyAttack_ErikaArcher.uasset`、未进入 B3 图的动画资产与 `ROADMAP.md` 的 05B WIP 持续排除。

**Socket 合同**：仅 DarkKnight Skeleton 的既有 `LeftHandSocket` 代表通用左掌武器握持，`ABowBase` 继续从它继承默认值；新建的 `LeftHandShieldSocket` 只用于玩家盾牌的手背握持，`AShield` 默认值与 `BP_Shield` CDO 都必须指向它。Erika 使用独立 Skeleton，既有 `LeftHandSocket` 合同不受影响。蓄力、FOV、准星蓄力反馈与命中提示继续留给 `TODO-04B-C`。
