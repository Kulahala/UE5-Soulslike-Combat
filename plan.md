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

### TODO-04D-C: Archer Spacing And Escape v1

**状态：✅已通过。** 用户已完成 `TestEditor` 编译和 Erika / Paladin PIE 回归；本阶段仅提交纯 Projectile `RangedEscape`、Erika 距离配置与其攻击 DataAsset，不混入下一阶段的三档速度、`ARangedEnemy` 父类迁移或 LOS 攻击取消。

#### 已交付

- `AEnemy` 的私有 Combat HFSM 新增 `RangedEscape`：只有“至少一条可选 Projectile 且无可选 Melee 条目”的配置可进入，Paladin 与混合攻击配置保持原分支。
- Erika 的已验证距离合同为：`<600 cm` Escape，`600-900` Retreat，`900-1000` BackDiag，`1000-1100` 安全 Strafe/fire，`>1100` Press 到约 `1050`；Escape 在 `>=1000 cm` 退出，并以固定导航腿、独立 request ID 和 BackDiag fallback 避免不断取消寻路。
- 局部仲裁顺序固定为：已承诺攻击蒙太奇、Escape、pending AttackIntent、普通 MovementIntent。冷却 Timer 只释放 gate，下一帧由 `OnCombating()` 统一重评估。
- Projectile 起手继续严格使用条目战术窗口和 LOS；合法起手后，Release 使用物理飞行距离加 LOS/状态/一次性 guard，玩家短暂后退不会再导致距离超限的空 Release。

#### 验收与 Review

- 用户 PIE 确认 Escape 进入/退出滞后、固定目标不抽搐、导航失败 fallback、冷却和 Pending 抢占、墙体 LOS、已承诺 Release、生命周期清理以及 Paladin 回归均符合预期。
- 正常 review：未发现 P0/P1。纯 Projectile 门控、专属导航 request、状态退出与 `EndPlay()` 清理均有明确归属；C++ 与当前 Erika/DataAsset 资产范围一致。
- 对抗性 review：已承诺攻击高于 Escape 可避免在 Draw/AimHold 中突兀改变移动语义；固定 Escape 腿避免追赶时反复取消 `MoveTo`；物理距离放宽不绕过 LOS、目标有效性或一次性 Release guard。未发现 D-C 阻塞。
- 接受一个 P2 表现债务：玩家在 Release 前完全失去 LOS 时不会被隔墙射中，但当前蒙太奇没有 Cancel/Recover，可能仍播放一次无箭 Release。该项已登记到 `ROADMAP.md`，由 `TODO-04D-D` 处理。

#### 文档与提交边界

- 已更新 `ARCHITECTURE.md` 与 `ROADMAP.md`；`README.md` 不改。
- 提交仅包含 D-C C++、`BP_ErikaArcher.uasset`、`DA_EnemyAttack_ErikaArcher.uasset`、`plan.md`、`ARCHITECTURE.md`、`ROADMAP.md`，持续排除暂停菜单、覆盖确认和 Encounter WIP。
