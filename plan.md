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

### TODO-04B-B2a: Shared Bow Identity And Attachment Defaults v1

**状态：✅已完成。** 用户已完成 `TestEditor` 编译与 PIE 验收，严格 review 和文档收尾均已通过；当前阶段统一 Bow 的物理身份和默认附着合同，不合并玩家弹药/瞄准/Prepared-Commit 与敌人 HFSM/LOS/Projectile 攻击职责。

#### 目标与不可变合同

- 新增抽象 `ABowBase : AWeapon`，只提供 Bow 身份和默认 `LeftHandSocket`。`ABow` 保持类名并继承它；`BP_ErikaBow` 将重设为该基类的 Blueprint 子类。
- `AWeapon` 的通用 `DefaultEquipSocketName` 默认右手。现有 `PlayerEquipSocketName` 通过 `[CoreRedirects]` 迁移，不能保留两份并行生效的 Socket 字段。
- 玩家继续从有效 `ABow` 判断双手占用、弹药、瞄准和 Release；敌人继续由 `FEnemyAttackEntry`、攻击快照与 `ARangedEnemy` 驱动。B2a 不改伤害、LOS、箭数、Montage、Mesh、拾取或掉落。
- `AWeapon::DefaultEquipSocketName` 是玩家和敌人唯一的附着来源；`AEnemy` 不保留 `WeaponAttachSocketName`、角色级覆写或二次解析。武器默认 Socket 不存在或附着失败时仍销毁候选武器并输出 warning。

#### 当前实施范围

1. C++：新增 `ABowBase`，迁移 `AWeapon` Socket API，更新玩家 MainHand 实体化和敌人 `WeaponInit()` 直接读取武器默认值；`ABow` 移除自己的左手设置，`AEnemy` 删除角色级 Socket 覆写。
2. 配置：在既有 `DefaultEngine.ini [CoreRedirects]` 添加 `/Script/Test.Weapon.PlayerEquipSocketName -> DefaultEquipSocketName`。
3. Editor：在 C++ 编译通过且 Live MCP 预检成功后，先读回 `BP_ErikaBow` 的父类/Mesh/相对 Transform，再重设父类并编译保存；对曾序列化旧字段的 `BP_BaseEnemy`、`BP_Paladin`、`BP_ErikaArcher` 依次编译并保存，清除已删除的 `WeaponAttachSocketName` 标签。不得新增替代覆写字段。

#### 验证与收尾

- 用户手动编译 `TestEditor`；不运行 UBT、打包或自动 PIE。编译后验证玩家 Bow 的左手、双手占用、盾牌恢复、瞄准与放箭；验证 Erika 从 `BP_ErikaBow -> ABowBase` 的左手默认值射箭/LOS/Escape/投射物不回归；验证 Paladin 从普通 `AWeapon` 的右手默认值保持近战与碰撞窗口。
- 用户 PIE 通过后执行正常 review 与对抗性 review，重点检查 Core Redirect、Blueprint 旧字段清理、武器默认值的唯一性，以及玩家/敌人职责隔离。
- 通过后更新 `ARCHITECTURE.md`、`ROADMAP.md`；`README.md` 默认不改。B2a 提交只包含新 Bow 基类、相关 C++/Config、迁移后的 `BP_ErikaBow` / `BP_ErikaArcher`，以及仅为清除已删除反射字段而保存的 `BP_BaseEnemy` / `BP_Paladin` 和阶段文档；持续排除动画包、`DA_EnemyAttack_ErikaArcher`、`ABP_DarkKnight_IkTrace` 与 TestMap External Actor。

#### 当前验证与 Strict Review

- 用户已手动编译 `TestEditor` 并完成 PIE；Editor Details 已确认 `BP_BaseEnemy`、`BP_Paladin`、`BP_ErikaArcher` 不再暴露 `WeaponAttachSocketName`。
- VibeUE AssetRegistry/CDO 复核：三份敌人 Blueprint 的旧反射属性均不可读取；`BP_ErikaBow` 的父类为 `/Script/Test.BowBase` 且 `DefaultEquipSocketName = LeftHandSocket`；`BP_Weapon` 与 `BP_Weapon_Paladin` 均为 `RightHandSocket`。
- 正常 review：无 P0/P1/P2 finding。Core Redirect 仅迁移旧玩家字段；敌人旧字段不建立错误的跨类 Redirect，已由 Blueprint 重编译/保存清除。Socket 不存在时，现有 `AWeapon::Equip()` 失败路径仍销毁候选 Actor，避免无效 `EquippedWeapon`。
- 对抗性 review：`ABowBase` 只承担 Bow 身份和左手默认值，未吸收玩家弹药/瞄准或敌人 HFSM；玩家 MainHand 与敌人 `WeaponInit()` 是 `GetDefaultEquipSocketName()` 的唯一调用者；不存在角色级 Socket 旁路。`AShield` 不继承 `AWeapon` 且不能填入当前 `AEnemy::WeaponClass`，其玩家副手 `OffhandSocketName` 合同不受本阶段影响。
- 剩余非 blocker：玩家 Bow 尚未拥有可见 Mesh，左手位置只能由 `ABowBase` CDO 和附着路径证明；正式视觉对齐仍留给 `TODO-04B-B`。
- 文档收尾：`ARCHITECTURE.md` 已记录共享 Bow 层级与武器唯一 Socket 合同；`ROADMAP.md` 已将 B2a 移入 Done Milestones，B-B 继续作为下一阶段。
