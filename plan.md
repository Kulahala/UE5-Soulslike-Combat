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

### TODO-05A: Player Guard Break v1

**状态：✅已执行，已通过。** 为玩家格挡体力耗尽和 Exhausted 未格挡受击增加 `EAS_GuardBroken`。本次仍由既有 `FCombatHitResolver` 一次结算伤害；破防只影响玩家端的受击表现与动作状态。

#### 固定边界

- 正面有效格挡且 `0 < CurrentStamina <= StaminaCost` 时，本次仍按盾牌减伤结算、消耗剩余体力并请求破防；耗体为 `0` 的配置不触发破防。
- 已处于 `EAS_Exhausted` 时的未格挡受击保留全额伤害，但拑制普通 HitReact/硬直，同样进入破防。破防期间后续非致死伤害不重播、不延长、不覆盖当前破防。
- `AM_GuardBreak_DKM` 为专用、原地、无 Root Motion 的 `DefaultSlot` Montage；无效配置只 warning 并安全回退 `EAS_Exhausted`。不新增 HUD、SaveGame、输入资产、敌人类或通用状态机。
- 破防自然结束或非死亡中断后直接回 `UnOccupied`，清理耗尽门卫并保底 `1` 点体力；不叠加既有 `3 s` Exhausted。

#### 当前执行记录

- 已完成代码/命中链路和 server-memory 预检；当前无匹配的历史错误模式。
- 用户已完成 Guard Break 资产接线、`TestEditor` 编译和原始 PIE 验收。C++ 实现包含 `EAS_GuardBroken`、格挡减伤保留、Exhausted 受击与破防期间普通 HitReact 抑制、Montage 委托恢复、输入/锁定/AnimInstance gate。
- UnrealClaude 编辑器 MCP 未连接，不对 `.uasset` 作二进制修改。C++ 编译通过后由用户导入新的 DarkKnight 兼容破防动画，创建 `AM_GuardBreak_DKM`，并赋值到 `DA_PlayerActionConfig.GuardBreak.Montage`。
- 后续耐力一致性收口已完成：跳跃和正耗体的成功格挡都会刷新 `StaminaRegenDelay`，`ExhaustedTime` 从 `5 s` 调为 `3 s`。用户已完成针对性 PIE 复验。
- 严格 review 已完成。正常与对抗性检查确认命中事务、`<=` 耗尽边界、`OnExhausted` 抑制范围、Montage 委托恢复、死亡/迟到回调和输入清理没有剩余阻塞；发现的枚举序列化兼容问题已通过将 `EAS_GuardBroken` 追加到 `EActionState` 末尾修复，用户已重新编译并回归验证。
- 文档已完成收尾：`TODO-05A` 移入 `ROADMAP.md` Done Milestones，`ARCHITECTURE.md` 固化 Guard Break、体力恢复和枚举追加契约；`TODO-05A1` 仍作为独立的 Combat Presence 脱战冲刺耐力阶段排队。
- `TODO-05A1: Combat-Aware Sprint Stamina v1` 已记录到 `ROADMAP.md`；本阶段不实现 Combat Presence，也不以锁定状态代替战斗判定。
- 当前用户 WIP `DA_EnemyAttack_ErikaArcher.uasset` 与 TestMap External Actor 持续排除。
