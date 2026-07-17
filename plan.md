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

### TODO-04D-D: Ranged Enemy Specialization And Attack Cancellation v1

**状态：✅已执行，已通过。** 本阶段创建 `ARangedEnemy : AEnemy`，迁移 D-C 的纯远程 Escape 运行时状态，建立慢/中/高速职责，并在 Draw/AimHold 期间持续失去 LOS `0.15 s` 后以 `Montage_Stop(0.12 s)` 取消未 Release 的攻击。`AEnemy` 保留共同敌人核心；不创建 `ABaseEnemy`、`AMeleeEnemy`、`AMageEnemy`、DataAsset、Controller、地图或动画资产。

#### 固定边界

- `AEnemy` 继续拥有感知、目标、受击/死亡、通用导航、攻击条目、Projectile Release 与 Debug Probe。`ARangedEnemy` 只拥有纯远程 Escape、远程 CDO 默认值、攻击期 LOS 取消和对应调试。
- `AEnemy` 的三档速度固定为 Patrol `200`、CombatManeuver `290`、Chase `330`；`ARangedEnemy` 构造阶段覆盖为 `220 / 300 / 330`。Retreat、BackDiag、Strafe 与 LOS 重定位使用中速；Press、Chase、Escape 使用高速。
- `AEnemy` 的近战距离基线与现有 Paladin 对齐：Attack Max `220`、Preferred Min `240`；`ARangedEnemy` 继续只覆盖纯远程距离环。实际蓝图已更正为 `BP_Paladin`，旧路径 `BP_Pladin` 仅保留 redirector 以维持现有地图/Encounter 引用，不执行 Fix Up Redirectors。
- Erika 的内层距离合同成为 `ARangedEnemy` CDO 默认：Escape `600 -> 1000`、Retreat `<900`、安全环 `1000-1100`、攻击上限 `1100`、Press Margin `50`、Retreat 最低倍率 `1.0`。外层追击/感知参数保持现有 Blueprint 作者值。
- `AEnemy` 原生拥有所有敌人共用的 HealthBar/LockOnMarker 几何默认：HealthBar 使用屏幕空间与相对位置，但尺寸由 `WBP_EnemyHealthBar` 根 `SizeBox` 的期望大小驱动；锁定标记为 `4 x 4 @ Z=30`。具体 Widget Class 仍由敌人 Blueprint 作者化。
- 取消只针对活动、未 Release、非 Debug Probe 的 Projectile 攻击。首次 LOS 失败开始计时，恢复 LOS 清除计时；持续 `0.15 s` 后先封锁 Release，再只停止当前攻击 Montage。取消沿用条目正常 cooldown；已 Release、近战、受击、死亡和 Dormant 不受影响。

#### 实施顺序

1. 在 `AEnemy` 提炼受保护的远程战术扩展点、窄移动辅助与攻击快照取消包装；删除其 `RangedEscape` 私有状态和第四档 Escape 速度。
2. 新增抽象 `ARangedEnemy`，迁移固定导航腿、fallback、验证、Debug、状态清理与 LOS 取消，保持 Projectile 最终 Release LOS guard 在基类。
3. 编译通过后以 Live Editor 顺序重设 `BP_ErikaArcher` 父类和 `BP_Paladin` / Erika CDO 覆盖；不手改 `.uasset`。
4. 用户手动 PIE 验收取消、三档速度、D-C 距离环、Paladin 与 Debug Probe 回归；随后做两轮 review、文档收尾和独立提交。

#### 当前执行记录

- 已完成源码与调用链勘查；当前 D-C 的 Escape、Attack Tick、Montage End、导航完成和配置验证均集中在 `AEnemy`，适合通过钩子迁移而非覆写完整 `OnCombating()`。
- 已查询 server-memory：`Montage_Stop`/迟到 Notify、Abstract Blueprint reparent/CDO、`FAIRequestID` 生命周期没有既有记录；编译前会执行针对性静态检查。
- UnrealClaude 与 VibeUE 当前均不可连接；C++ 可先完成，所有资产改动等待用户编译并重新建立 live Editor 预检。
- C++ 已完成：`ARangedEnemy` 承接纯远程 Escape、远程 CDO 默认值和未 Release 的 LOS 取消；`AEnemy` 只保留通用移动、攻击、投射物与 Debug Probe。`CombatManeuverSpeed` 已替代旧的第四档 Escape 速度，基类旧 Escape 状态/请求 ID 已删除。
- 已将 Paladin 的通用近战 Attack Max / Preferred Min 基线收敛到 `AEnemy`，并把 Erika 重设父类后遗失的原生 Widget Component 展示默认收敛到 `AEnemy` 构造函数。HealthBar 现在通过 `DrawAtDesiredSize` 使用 `WBP_EnemyHealthBar` 根 `SizeBox` 的 `170 x 14` 期望尺寸，用户已重新编译并完成 PIE 验收。
- 已完成轻量静态检查：没有残留基类 `RangedEscape` 枚举或字段；速度调用点符合 Patrol/Search、CombatManeuver、Chase/Press/Escape 职责；`git diff --check` 通过。
- 严格 review 已完成。正常 review 与对抗性 review 均未发现 D-D C++ 行为阻塞：未 Release LOS 取消先封锁一次性 Release guard，再通过 Montage End 进入既有幂等 cooldown；Escape request、fallback、死亡、受击、Dormant、EndPlay 与 Paladin 隔离均有明确归属。
- 文档已同步 D-D 的 `ARangedEnemy` 边界、三档速度、Escape 与 LOS 取消契约，并将阶段移入 `ROADMAP.md` Done Milestones。用户已批准将实际资源更正为 `BP_Paladin`，同时提交旧路径 `BP_Pladin` redirector；不执行 Fix Up Redirectors，也不修改地图/Encounter External Actor WIP。
