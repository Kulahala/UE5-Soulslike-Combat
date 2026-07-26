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

# TODO-02C0: Encounter Wave Spawn Core v1

## 结果

- 实现互斥的预放置参与者或一次性 `InitialSpawnBatch`，其 Anchor 使用稳定 ID 与 `Point`/`Circle`/`Box` 候选区域；Controller 在任何 Actor 生成前原子解析 NavMesh、地面、Capsule、边界内缩与批内预约安全落点。
- 动态批次通过既有 owner、death delegate、Dormant 与 Activate 合同接线；失败会回滚动态 Actor 且保持 `Idle`、边界开放。`AEnemy::EndPlay` 也清理其巡逻 `TargetPoint`，不把直接 Destroy 伪造成死亡清场。
- 用户已完成当前 `TestEditor`/PIE 验收：正常批次、边界外 Anchor 拒绝和动态 TargetPoint 清理均符合合同；主线程正常 review、fresh 对抗性 review 与 fresh delta review 均无未修复 blocker。
- 提交仍须等待用户明确批准。下一阶段开始前按本文件顶部规则清空本阶段记录。
