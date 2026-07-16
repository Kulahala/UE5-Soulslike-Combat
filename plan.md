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

# TODO-04B-A2: Ammo Containers And Bonfire Refill v1

## 目标

将箭矢拆为已装填容器与储备 ItemInstance 栈：弓只消费已装填箭，真实火堆休息以检查点激活为同一次写盘事务补充箭矢。`DA_Item_DarkKnightArrow` 的作者配置固定为装填上限 `20`、储备单栈上限 `99`；储备无总上限，满栈自动新建稳定实例。

## 已锁定边界

- `SaveVersion` 保持 `2`。旧 v2 箭实例保留为储备，`LoadedAmmoContainers` 为空；下一次真实休息才装填，不加载时迁移或写盘。
- 只有火堆“休息”补箭；死亡、Continue、离开菜单、换装、字段交互和地图重载都只恢复已保存数量。
- 组件只构造已验证 `InstanceId` 的储备转移计划；GameInstance 是唯一写入者，并一次提交检查点、储备与装填容器。
- 不新增背包 UI、HUD、地图拾取物、弓模型/动画或 `BP_PlayerArrow`；后者的投射物退款 P2 仍归 `TODO-04B-B`。

## 实施顺序

1. 新增 SaveGame 装填记录、Definition Ammo 配置和缓存验证。
2. 实现储备栈授予、已装填消费、火堆补给与检查点的原子事务及 scoped 失败注入。
3. 将弓发射改为消费已装填箭，并把火堆休息改走角色/组件协调入口。
4. 扩展 Dump 与无副作用的损坏记录 fixture 验证；完成 `DA_Item_DarkKnightArrow` 配置。

## 当前进度

- [x] 已完成实现前 CodeGraph/存档/火堆调用链预检，并确认工作区干净。
- [x] 已完成 C++ 数据模型、事务和火堆接线：装填容器、已验证实例索引快照、储备分栈、弓消费、检查点补给原子回滚和开发期 fixture 已接入。
- [x] Live Editor 预检与 Definition 接线完成：Editor、`3000`、`8088` 和可调用 MCP 已确认；`DA_Item_DarkKnightArrow` 已保存并读回 `bUsesAmmoContainer=True`、`LoadedAmmoCapacity=20`、`ReserveAmmoStackLimit=99`。
- [x] PIE 基础箭矢循环已验证：`25` 支仅进入储备（`0/20 + 25`）；零装填拒射；真实休息后为 `20/20 + 5`；发射一箭后为 `19/20 + 5`，运行时与保存数据一致。
- [x] PIE 完整验收已完成：再次休息补一支；补给写盘失败注入不触发重载且正常重试后恢复；损坏记录 fixture 只在内存副本中验证；储备多栈为 `99 + 21`；已装填消费失败注入不发射、不扣数。死亡、Continue 和重新 PIE 不补箭由用户人工确认；其余关键状态与注入日志已由 MCP 输出日志读回。
- [x] 正常 review：未发现 A2 的事务回滚、写入范围、缓存恢复、火堆转场或旧 v2 默认容器阻断问题。
- [x] 对抗性 review：坏容器/同 ID 脏记录、空储备、死亡/Continue 和失败注入均未绕过真实休息补给边界；记录一个不影响当前箭矢路径的 P2，供首个非箭矢可堆叠消耗品/背包阶段替换旧 raw-DefinitionId 数量消费 API。
- [x] 文档同步：`ARCHITECTURE.md` 记录装填/储备、火堆事务与调试边界；`ROADMAP.md` 将 A2 移入 Done、移除已解决的箭矢 raw-order 风险，并保留通用非箭矢数量 API P2 与投射物退款 P2。
- [x] 最终差异检查：`git diff --check` 无空白错误；工作区范围仅含 A2 C++、`DA_Item_DarkKnightArrow`、`ARCHITECTURE.md` 与 `ROADMAP.md`，未混入无关 UI 或 Encounter WIP。
- [x] 已获用户批准并创建独立提交 `f6e15f2`：`[Feature] 新增弹药容器与火堆补给（Ammo Containers and Bonfire Refill）`。

## 验证重点

- `25` 支储备在休息前为 `0 loaded / 25 reserve`，休息后为 `20 / 5`。
- 已装填消费、再次休息补给、死亡/Continue 恢复、`99 + 21` 多栈、旧 v2 首次休息和失败注入均符合计划。
- 损坏储备 fixture 不能影响合法实例；剑盾、Gold、火堆保护与既有 SaveGame 行为不回归。
