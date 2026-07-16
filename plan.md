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

# TODO-04B-B0: Prepared Projectile Commit And Refund Atomicity v1 — ✅已执行，已通过，待提交

## 目标

将弓箭发射固定为“准备即承诺”：所有可能失败的配置验证和 Prepared Projectile 创建都发生在已装填箭持久化消费之前；消费成功后只执行原生、不可失败的 Commit 启动。由此移除旧的第二次写盘退款分支。

## 已锁定边界

- 不新增 `SaveVersion`、预留箭状态、退款事务、背包、HUD、输入映射、动画、音效、`BP_PlayerArrow`、世界拾取物或敌人射箭。
- `SpawnPreparedProjectile()` 是唯一可失败候选入口：Deferred Spawn、配置、`FinishSpawning()`、`BeginPlay()` 后必须确认原生碰撞、移动、快照和未提交状态均可 Commit。
- `CommitPreparedLaunch()` 不是 BlueprintCallable，也没有失败返回值；未来 `BP_PlayerArrow` 只能增加无碰撞表现，不能改变原生碰撞、移动、寿命、命中或 Commit 语义。
- `BowDebugFailNextProjectilePrepare` 仅为当前 Pawn 的非 Shipping 一次性注入：候选已准备后、消费前销毁候选，不写入 SaveGame，不跨死亡、转场、Continue 或 PIE。

## 实施顺序

1. 收束 `ACombatProjectile` 的 native prepare/readiness 状态、完成后校验和即时/Prepared 共用 Commit 路径。
2. 将 `ReleaseBowArrow()` 改为候选准备成功后才消费 Loaded Arrow，删除激活失败退款路径。
3. 添加 Pawn 生命周期内的准备失败注入及 Controller Exec 转发；死亡与 `EndPlay` 清理标记。
4. 静态检查调用链、无第二写盘退款、Owner/Instigator 时序和候选销毁；再请求用户手动编译与 PIE。

## 当前进度

- [x] 已完成 A2 工作记录清理、Git 边界确认和 B0 路线图排期。
- [x] 已完成 CodeGraph 调用链预检：当前风险位于 `ReleaseBowArrow()` 的“消费 -> `ActivateConfiguredProjectile()` -> 退款”分支；立即发射由 `BeginPlay()` 启动，Prepared 候选由弓路径独占。
- [x] 已完成定向 server-memory 查询：`SpawnActorDeferred`、`FinishSpawning`、`BeginPlay`、`UProjectileMovementComponent` 与 Prepared Commit 生命周期没有可复用的既有错误条目。
- [x] 已完成原生 prepare/commit 契约、弓的单次消费路径和一次性准备失败注入：`SpawnPreparedProjectile()` 在 `FinishSpawning()` 后拒绝未完成 native preparation 的候选；立即发射仍由 `BeginPlay()` 调用同一 `CommitPreparedLaunch()`；成功发射不再有退款写盘。
- [x] 已完成轻量静态检查：旧 `ActivateConfiguredProjectile()` 无源码调用，`ReleaseBowArrow()` 无退款 `TryGrantDefinitionQuantity()`，新 Exec/Character/Projectile 声明定义齐全，死亡与 `EndPlay` 均清除 Pawn 内注入标记。
- [x] 已修复首次编译的 `C2039`：当前 UE 版本的 `UProjectileMovementComponent` 不提供 `GetUpdatedComponent()`；改为在 `BeginPlay()` 重设原生 `CollisionSphere` 为 UpdatedComponent，并以根组件验证准备态。该 API 差异已记录到 server-memory。
- [x] 用户已手动编译并完成 PIE 验收，MCP 日志确认：正常发射 `Loaded 18 -> 17` 且 Runtime/Saved 一致；准备失败注入只销毁候选、不扣箭；下一次未重武装正常发射；已装填消费写盘注入不发射、不扣箭；死亡重载清除未消费的准备注入，重载后首次发射正常；两次 `ProjectileDebugFire` 均走即时 Commit 并正常停止。
- [x] 正常 review：未发现 B0 的候选销毁、Owner/Instigator 时序、单次持久化消费、失败注入范围、即时投射物回归或 Shipping 守卫阻断问题。`TryConsumeLoadedAmmo()` 的成功写盘后只会重建本地缓存，不会转场、销毁 Pawn 或触碰候选；`RestoreFromSave()` 对持久化存档只降级忽略脏记录，不会制造第二个失败出口。
- [x] 对抗性 review：所有当前 Commit 调用者均先完成 `IsPreparedForActivation()`；`BeginPlay()` 重设原生 UpdatedComponent，准备态要求无碰撞/未激活移动/有效快照；消费失败和准备注入都在 Commit 前销毁候选；死亡与 `EndPlay` 清除 Pawn 注入。无 B0 阻断发现。
- [x] 已补即时 Debug 投射物的命中与超时回归：日志确认命中 `BP_Pladin` 并结算 `10.00` 伤害；多次空射均在约 `3 s` 后输出 `expired without an impact`。墙体优先停止、目标命中和空射寿命都由当前 Commit 路径覆盖。
- [x] 已完成稳定文档同步：`ARCHITECTURE.md` 记录可失败准备、不可失败原生 Commit、单次 Loaded Arrow 写盘和 Blueprint 表现边界；`ROADMAP.md` 将 B0 移入 Done，并移除已解决的两次写盘退款风险。
- [x] 已获明确提交批准；提交范围仅限 B0 C++、`plan.md`、`ARCHITECTURE.md` 与 `ROADMAP.md`。

## 验证重点

1. 正常发射只消费一支 Loaded Arrow，并维持现有声音、冷却、墙体优先、命中和超时表现。
2. `BowDebugFailNextProjectilePrepare` 在候选准备后销毁它：无投射物、无声音、无冷却、Runtime/Saved Dump 不变；下一次有效释放正常发射。
3. `BowDebugFailNextAmmoConsumeSave` 仍只让消费写盘失败：候选销毁且 Loaded/Reserve/SaveGame 不变。
4. 死亡、Continue、重新 PIE 和 `ProjectileDebugFire` 不保留候选、碰撞、寿命或注入状态；`ReleaseBowArrow()` 不再包含退款数量写入。
