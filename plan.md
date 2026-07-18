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

### TODO-04C: Scroll-Wheel Lock Target Switching v1

**状态：✅已执行，已通过。** 以独立 `Axis1D` 滚轮输入切换锁定目标：下滚选择当前目标屏幕右侧最近候选，上滚选择左侧最近候选。无同侧候选时保持原目标；不引入环绕、解锁、额外 UI、SaveGame、地图 Actor 或新的动作状态。

#### 固定边界

- `UPlayerLockOnComponent` 负责按存活、`LockOnRadius`、viewport 投影和当前目标屏幕 X 侧筛选候选；不加入 LOS Trace，不复用初始锁定的 `LockOnViewAngleDegrees`。
- `AMyCharacter::SwitchLockOnTarget()` 只在现有有效锁定内直接调用组件的 `SetLockedTarget()`，不可调用首次锁定用的 `SetLockOnTarget()`，避免重复缓存旋转状态。
- `ACharacterController` 只负责 `IA_LockTargetSwitch` 的输入阈值 `0.5`、re-arm `0.1`、成功切换冷却 `0.15 s`，以及暂停/火堆 UI 拦截。正轴（上滚）切左，负轴（下滚）切右。
- 活动锁定时允许攻击、格挡、翻滚、喝药和瞄准切换；死亡、硬直、暂停与火堆 UI 拒绝。`Input_Look()`、中键 `IA_LockOn`、锁定相机和 FreeRun 合同保持不变。
- 当前锁定目标死亡时，`AMyCharacter` 仅复用既有 `FindBestTarget()` 在当前相机前方和 `LockOnRadius` 内选择下一有效敌人，并直接转交组件目标标记；没有候选时沿用现有解锁。超出 `LockOnBreakRadius`、Actor 无效或玩家死亡不触发自动换目标。

#### 实施顺序

1. 更新 C++ 锁定组件、玩家入口与 Controller 输入绑定/锁存；完成轻量静态检查后由用户手动编译 `TestEditor`。
2. 编译后完成 Live Editor MCP `initialize -> tools/list ->` 只读预检和恢复点，再创建 `IA_LockTargetSwitch`、在 `IMC_CharacterInput` 映射 `Mouse Wheel Axis`，并为 `BP_CharacterController` 赋值；工具不可用时提供精确手工接线步骤。
3. 使用现有 TestMap 的 Paladin 和 Erika 手动 PIE 验收方向、无候选保持、输入节流、动作中切换、死亡自动重定向/UI gate 和现有锁定/敌人回归。
4. 通过后进行正常及对抗性 review，并同步 `ARCHITECTURE.md`、`ROADMAP.md`、`plan.md` 后等待独立提交批准。

#### 当前执行记录

- 已确认现有 `Input_Look()` 在锁定时直接返回，当前不存在滚轮 Input Action；新增独立 Enhanced Input Action 是最小输入边界。
- 已确认 `AMyCharacter::SetLockOnTarget()` 会调用 `CacheLockOnRotationState()`；锁定内目标切换必须走新的窄入口，不能复用该首次锁定路径。
- 当前用户 WIP `DA_EnemyAttack_ErikaArcher.uasset` 与 `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/C/GG/A3OYSKXOUMFGPZ2YEKXTZL.uasset` 持续排除。
- 严格 review：正常与对抗性检查均确认屏幕侧选择、标记交接、旋转缓存、输入锁存、无候选保持、跑远解锁和输入资产接线没有阻塞。P2 已修复：自动重定向 gate 现同时排除 `EAS_Dead` 和 `EAS_Stunning`，硬直中当前目标死亡会回退到既有解锁路径，而非转交新目标。当前 Demo 没有可实际构造“玩家硬直中杀死锁定目标”的 fixture，用户已接受该窄 gate 的静态复核替代额外测试工具。
- 文档已同步稳定锁定输入、屏幕侧选择、死亡自动重定向与硬直/跑远解锁边界；`TODO-04C` 已移入 `ROADMAP.md` Done Milestones。`README.md` 不改，等待独立提交批准。
