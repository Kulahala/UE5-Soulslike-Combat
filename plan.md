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

### TODO-04B-B2: Bow Two-Hand Occupancy And Attachment Contract v1

**状态：✅已执行，用户已完成编译 / PIE 验收，严格 review 已完成且无 B2 blocker；待提交。** 在正式玩家弓 Mesh、箭视觉、动画、拾取物和 HUD 作者化前，建立 `ABow` 的左手附着与双手占用合同。该阶段只收束装备表现/玩法的运行时边界，不能改变玩家的持久化装备选择。

#### 目标与不可变合同

- `ABow` 仍是 `MainHand` 物品；玩家在火堆选中的 OffHand 盾牌仍保留在 `UItemOwnershipComponent` / `USoulslikeGameInstance` 的持久化装备槽中。装备 Bow 不写入卸盾、不改变 SaveGame 版本，也不创建第二份装备记录。
- 当前有效 MainHand 为 `ABow` 时，Bow 消耗双手：它附着在 `LeftHandSocket`，运行时不保留可见或可参与玩法的 `EquippedShield`。右手用于后续 B-B 的搭箭、拉弦和 Release 表现，但 B2 不创建箭视觉或右手 Socket 资产。
- 切回非 Bow 的 MainHand 时，已持久化选择的 OffHand 盾牌自动恢复；切换 Bow 时，原盾牌只从当前 Pawn 的运行时实体化中抑制，不丢失选择。
- `EWeaponState::EWS_TwoHandEquipped` 可作为根 AnimBP 的被动装备姿态输入；实际的盾牌抑制、格挡和弹反合法性仍只从当前有效 `ABow` 判断，绝不能由动画枚举反推玩法。
- 不新增 `EActionState`、通用双手武器框架、背包、输入、SaveGame 字段、HUD、动画资产、世界拾取物、箭/投射物逻辑或网络/GAS。

#### 现状与设计决定

- 当前玩家 MainHand 一律通过共享 `RightHandSocket` 附着；全局移动该常量会错误移动现有剑。B2 在 `AWeapon` 增加仅供玩家装备路径读取的窄 `PlayerEquipSocketName` 合同，默认 `RightHandSocket`；`ABow` 的 CDO 默认 `LeftHandSocket`。敌人的 `WeaponAttachSocketName` 和 `AEnemy` 附着路径不改。
- 当前恢复和火堆换装会独立实体化 MainHand 与 OffHand。B2 将“当前 MainHand 是否消费 OffHand”的判定集中在 `AMyCharacter` 私有帮助函数：先成功实体化 MainHand，再决定是否实体化 OffHand；任何抑制只处理当前 Pawn Actor，不触碰已保存的 OffHand `InstanceId`。
- 当前普通格挡已排除 Bow，但弹反还只检查 `EquippedShield`。B2 统一清理/拒绝 Bow 下的 Block 和 Parry，并在防御解析处保留状态保护，防止已开始的旧格挡、迟到 Notify 或直接调用走出盾牌减伤/弹反旁路。

#### C++ 实施范围

1. `AWeapon`：添加默认右手的玩家装备 Socket 配置和窄 Getter；保持 `Equip()` 的 Owner/Instigator、碰撞、`EquipRotationOffset` 和敌人调用签名不变。
2. `ABow`：在构造阶段把其玩家装备 Socket 默认设为 `LeftHandSocket`。Bow Blueprint 可继承该默认值；本阶段不需要选择 Mesh、调相对 Transform 或创建任何 Component。
3. `AMyCharacter`：
   - 在 `PrepareMaterializedLoadoutActorFromDefinition()` 的 MainHand 分支使用该 Weapon 的玩家 Socket，并保持候选附着失败发生在持久化写入之前。
   - 引入私有的 Bow/OffHand 占用判定与运行时副手重建帮助函数。`MaterializeEquippedLoadout()` 先恢复 MainHand，再依据该判定恢复或抑制 OffHand。
   - 火堆 MainHand 换装在持久化成功后统一协调副手：Bow 成功提交后销毁当前运行时盾牌；非 Bow 成功提交后按已保存 OffHand 选择重新实体化盾牌。副手选择在 Bow 激活期间仍走候选验证和持久化，但成功后不提交可见盾牌 Actor；离开 Bow 后恢复新选择。
   - Bow 进入双手占用时清理已有格挡/弹反和相应 Timer/held 意图；`CanStartBlock()`、`CanStartParry()`、`StartParryAction()` 与防御解析继续以当前 Bow 状态为安全门，不能因 `EquippedShield` 的旧指针、蒙太奇或 Notify 获得格挡/弹反。
   - MainHand 非 Bow 的既有 `EWS_OneHandEquipped` 语义保持；Bow 成功提交时写入 `EWS_TwoHandEquipped`，销毁 MainHand 时保留现有 `EWS_Unequipped` 清理。B2 不改 DarkKnight AnimBP 图；B-B 负责使用该输入作者化正式 Bow Locomotion/Aim。
4. 所有恢复、死亡重生、Continue、Rest、火堆服务换装和 `EndPlay()` 继续从持久化槽位重建运行时 Actor；不得留下失效 `EquippedShield`、错误 Owner/Instigator、已绑定 Parry Timer 或临时隐藏 Actor。

#### 验证

1. 实施前按本机 UE 5.7 头文件做轻量静态检查，并查询 server-memory：装备候选先验证后写盘、`AttachToComponent` Socket 失败、`EWeaponState` 序列化、Montage/Timer 清理和持久化槽与运行时 Actor 分离。
2. 用户手动编译 `TestEditor`；不运行 UBT、打包或自动 PIE。
3. 在已有 Bow + Shield 装备选择下进入 PIE：Bow 使用 `LeftHandSocket`，OffHand 盾牌不显示、不保留玩法引用；右键仍进入瞄准，左键现有放箭、箭数、准星和冷却语义不变。
4. Bow 激活期间按住格挡、点按弹反、接收可格挡近战/投射物、尝试旧状态延续：均不得举盾、弹反或获得盾牌减伤；切回 Sword 后普通格挡、弹反、格挡耐力与 Guard Break 立即恢复。
5. 火堆将 Sword+Shield 换为 Bow+Shield：只持久化 MainHand 选择变化，盾牌选择仍保留但运行时抑制；Bow+Shield 期间改选另一面盾牌后不显示/不播放装备表现，切回 Sword 后只恢复新盾牌。
6. 依次验证 Rest 重载、死亡重载、Continue、重新 PIE、清空 MainHand、清空 OffHand 与候选 Socket/类配置失败：持久化选择和运行时表现一致，失败不会写入无效选择或残留 Actor/Timer。
7. 回归剑盾攻击、格挡、弹反、Guard Break、弓瞄准/放箭/空箭/射击冷却、锁定、火堆服务、Paladin 与 Erika 投射物路径。用户确认 PIE 后执行正常 review 与对抗性 review。

#### 资产与文档边界

- B2 只改 C++、阶段文档和唯一必要资产 `Content/_GAME/BP/DataAssets/Items/Definitions/DA_Item_DarkKnightBow.uasset`。该 Definition 的稳定合同为 `DefinitionId = Item_DarkKnightBow`、`EquipmentSlot = MainHand`、`RuntimeItemActorClass = /Script/Test.Bow`；它必须进入玩家 `DefinitionCatalog`，否则干净检出无法授予、装备或实体化 Bow。除该资产外，不修改 `BP_DarkKnight`、Bow Blueprint、`ArcherAnimsetPro`、`ItemConsumableAnims`、其他 DataAsset、Montage、BlendSpace、Mesh、TestMap 或任何 External Actor。若新字段在既有 Bow Blueprint 出现显式旧值，仅在 Live Editor 预检成功后由用户 Reset to Default，并单独读回确认。
- `ArcherAnimsetPro` 的实际 DarkKnight 重定向兼容性仍由 B-B 作者化前检查 `Bow_Aim_Pull`、Aim Hold 和 Release 三条代表性动作决定；B2 不把文件名或预览姿势当作 Skeleton 兼容性证据。
- B2 通过编译、PIE 与两轮 review 后，更新 `ARCHITECTURE.md` 的玩家装备实体化、双手 Bow 和 `EWeaponState` 合同；将 B2 移入 `ROADMAP.md` Done Milestones。`README.md` 默认不改。
- 预计提交范围：`AWeapon`、`ABow`、`AMyCharacter`、`DA_Item_DarkKnightBow.uasset`、`plan.md`、`ARCHITECTURE.md`、`ROADMAP.md`。持续排除用户 WIP：`Content/ArcherAnimsetPro/`、`Content/ItemConsumableAnims/`、`DA_EnemyAttack_ErikaArcher.uasset`、TestMap External Actor、暂停菜单、覆盖确认及所有无关资产改动。

#### 当前执行记录

- 已完成：`AWeapon` 新增默认 `RightHandSocket` 的玩家专用附着 Socket；`ABow` CDO 覆写为 `LeftHandSocket`。玩家候选 MainHand 在持久化写入前验证该 Socket；敌人仍走其既有独立附着字段。
- 已完成：`AMyCharacter` 先恢复 MainHand，再集中协调运行时 OffHand。活跃 Bow 会销毁/清空运行时盾牌但不写持久化 OffHand；切回非 Bow 或清空 Bow 后静默恢复已选择盾牌。火堆 OffHand 换装和自动首装在 Bow 期间会完成候选验证与持久化后丢弃表现候选。
- 已完成：Bow 写入 `EWS_TwoHandEquipped`；Bow 下的 Parry 启动、Notify 激活与命中解析都有独立安全门，副手销毁会清理格挡 held 输入及 Parry Timer。
- 已检查：CodeGraph 已复核候选附着、火堆写盘、恢复、格挡和弹反调用面；定向 server-memory 查询没有返回既有错误模式；`git diff --check` 通过。用户已完成 `TestEditor` 编译和 PIE 验收，正常与对抗性 review 均未发现 C++ 行为 blocker。
- 提交边界纠正（已解决）：用户确认 `DA_Item_DarkKnightBow.uasset` 纳入 B2。其 `DefinitionId = Item_DarkKnightBow`、`EquipmentSlot = MainHand`、`RuntimeItemActorClass = /Script/Test.Bow` 是有效 Bow Definition、Catalog 注册、`ItemDebugGrant Item_DarkKnightBow`、火堆 MainHand 选择和运行时 `ABow` 实体化的必要合同；它是本阶段唯一纳入的资产。严格 review 在该范围纠正后无剩余 B2 blocker。
