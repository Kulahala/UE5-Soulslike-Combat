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

### TODO-04D-A: Enemy Ranged Delivery And Local HFSM v1

**状态：✅ 已执行，已通过验证与两轮严格 review，等待提交批准**

#### 范围与边界

- 保持 `AEnemy` 为唯一敌人基类；不新增 `AMeleeEnemy` / `ARangedEnemy`，不迁移 Paladin、Erika、地图或任何正式资产。
- 在 `FEnemyAttackEntry` 中区分 `Melee` 与 `Projectile` 投递。Projectile 正式条目仍必须具备 Montage、权重、距离、有效投射物类和有效 Delivery Config；无效条目不可被选择。
- `AEnemy` 在既有 Pending Attack、本地 Combat HFSM、攻击协调和冷却流程内处理远程距离环、LOS 重定位、攻击快照与一次性 Release。远程贴身时撤退，不改为近战、不贴身发射。
- 远程 Release 只经 `ACombatProjectile::SpawnConfiguredProjectile()`，不进入玩家弓的 Prepared/Commit 或箭矢持久化路径。
- 调试使用非 Shipping `EnemyRangedDebugProbe [ReleaseDelay]`：临时原生 Enemy + AIController，Timer 仅替代未来 Montage Notify 的 Release 时机；不创建、保存或伪装正式射手资产。

#### 实施顺序

1. 扩展敌人攻击 DataAsset 的投递类型、Projectile 配置、候选选择和编辑器校验。
2. 为 `AEnemy` 增加远程攻击快照、LOS/距离/发射来源检查、Pending 远程移动策略、受状态保护的一次性 Release，并统一攻击中断与 Timer 清理。
3. 将 Controller 绑定收束到 `PossessedBy()`，支持 Probe 动态 Possess 后的导航回调生命周期。
4. 添加 `UAnimNotify_EnemyProjectileRelease` 和仅非 Shipping Probe 命令；Probe 只驱动真实远程判定和 Release guard。
5. 静态审查后由用户编译 `TestEditor`，再按开阔地、格挡、墙体 LOS、贴身撤退、长延迟中断、Paladin 与 04A 回归做 PIE 验收。

#### 验证与 Review 结论

- 用户已完成 `TestEditor` 编译和 PIE。日志证据显示开阔地 Probe 每次只 Release 一枚投射物；玩家持盾时命中走现有格挡；最小距离外的 Release 被拒绝；长延迟 Probe 在 PIE 结束后没有迟到投射物。用户同时确认墙体 LOS、重定位、贴身撤退、Paladin 与 04A 回归符合预期。
- 正常 review：未发现 D-A 阻塞项。Projectile 条目无法因缺类、无效 Delivery Config、缺 Montage 或无效距离静默退化为近战；正式 Release 复查状态、目标、距离、LOS 和一次性 guard；攻击中断、硬直、破防、死亡、Dormant、Controller 丢失与 `EndPlay` 都会拒绝或清理延迟 Release。
- 对抗性 review：保留 `AEnemy` 作为唯一 C++ 基类是当前可辩护的边界，近战和远程仅在攻击条目投递、局部 Pending 移动与 Release 路径分叉，原 Paladin 仍使用默认 `Melee`。无资产 Probe 仅替代未来 Montage Notify 的时间来源，未伪装为正式 Erika 验收。未发现需要记录到 `ROADMAP.md` 的 D-A 风险。

#### 文档与提交边界

- 验证和两轮 review 通过后，才更新 `ARCHITECTURE.md`、`ROADMAP.md` 并完成 D-A 单独提交。
- 不纳入 Erika、`WBP_PauseMenu.uasset`、`WBP_OverwriteConfirmation.uasset` 或 `Content/__ExternalActors__/_GAME/BP/Maps/TestMap/E/RS/`。

#### 收尾状态

- 已同步 `ARCHITECTURE.md` 与 `ROADMAP.md`；README 不改，因为尚无正式玩家可见射手内容。
- 提交边界只包含 D-A C++、`plan.md`、`ARCHITECTURE.md` 与 `ROADMAP.md`。尚未提交，等待用户明确批准。
