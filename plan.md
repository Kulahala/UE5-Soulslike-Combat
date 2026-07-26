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

<!-- 当前无待处理反馈。 -->

---

## 计划 (Plan)

✅已执行并通过 review

# TODO-05C1: Combo Start Effect Dedup v1

## 范围与执行顺序

1. 在 `AMyCharacter` 增加私有、非反射的 `ApplyComboSegmentStartEffects(const FComboSegment&, UAnimInstance*, UAnimMontage*)`，集中成功起播后的体力、伤害、韧性、Motion Warping 与 Entry Section 跳转。
2. 只替换 `StartComboSegment()` 的 NewPlayback、同 Montage Continuation 与跨 Montage Handoff 三处重复块；保持每条分支的 preflight、token/delegate、planned handoff、失败回滚和清理顺序不变。
3. 不改 DataAsset、Montage、Notify、输入仲裁、`EActionState`、Sprint、Charged、Bow 或 `TODO-05C` 的既有 timing debt。

## 验证

- 静态确认 helper 是 Combo 成功提交块的唯一实现，三条分支各调用一次，且 `ClearPlannedAttackHandoff()` 仍在跨 Montage Jump 之后。
- 用户手动编译 `TestEditor`，PIE 验收单段、`01 -> 02 -> 03`、早/晚输入、CancelWindow、受击、Guard Break、死亡、Bonfire、换装和 EndPlay。
- 用户验证后执行正常 review 与新的 600 秒对抗性 review；提交须等待明确批准。

## 提交边界

仅包含 `MyCharacter.h/.cpp`、`plan.md` 与 `ROADMAP.md` 的 05C1 hunk。排除所有 `.uasset`、Combo/Bow/Potion/Dodge 资产、HUD、TestMap External Actor、`AGENTS.md`、`CLAUDE.md`、Boss 路线及无关 WIP。

## 结果

- `ApplyComboSegmentStartEffects()` 已收束三条 Combo 成功起播路径的段提交效果，保留既有 preflight、playback token、planned handoff、资源、Motion Warping、Entry Section 与清理顺序。
- 用户已手动编译 `TestEditor`，并在 PIE 验收单段、`01 -> 02 -> 03`、早/晚输入、CancelWindow、受击、Guard Break、死亡、Bonfire、换装和 EndPlay。
- 主线程正常 review 与独立 600 秒对抗性 review 均无 blocker。未单独作者化 same-Montage 测试条目；其 late delegate / immediate-end 时序债务是既有 `TODO-05C` 范围，不由本阶段新增。
