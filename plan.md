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

### TODO-04B-B1: Aim Reticle v1

**状态：✅已执行，已通过。** 在现有 `UPlayerHUDWidget::NativePaint()` 添加无资产、视口中心的空心十字准星；它只显示真实的弓瞄准状态，不参与瞄准、弹道、锁定、伤害、输入或存档。

#### 核心契约

- `AMyCharacter::IsBowAiming()` 是唯一游戏状态来源：只有有效 `ABow` 已装备且 `ActionState == EAS_Aiming` 才显示。HUD 不反向读取或修改角色状态。
- `UPlayerHUDWidget` 新增非 Blueprint 的 `SetAimReticleVisible(bool)`，仅在值改变时调用 Paint invalidation；不绑定属性组件、不新建 Tick、Delegate、SaveGame、Widget Blueprint 或第二 HUD。
- `AMyCharacter` 只在 `StartBowAimAction()` 成功进入瞄准、`CancelBowAim()` 清理瞄准、以及 `InitializePlayerHUD()` 创建 HUD 后同步一次有效可见性。`ReleaseBowArrow()` 不改状态，因此成功、空箭和冷却失败后准星按现有瞄准状态继续显示。
- 瞄准移动以角色 `WalkSpeed` 为基础，`ABow::AimMoveSpeedMultiplier` 改为相对该步行速度的作者倍率，默认 `1.0`。这使当前默认弓在不锁定时为 `200 cm/s`，与按住步行键一致；锁定时仍保留已有前/侧/后方向倍率。
- 空心十字使用中心总空隙 `6`、外半径 `10` Slate 单位；先画 `3` 单位深色半透明描边，再画 `1.25` 单位近白内线。它位于既有 UMG、受击红晕和 Debug 文本之后的最高层。
- 受击、Guard Break、死亡、火堆服务、主手换装、允许的高优先级动作取消与 `EndPlay()` 保持现有 `CancelBowAim()` / HUD 拆除边界；本阶段不复制或扩展它们。

#### 实施与验证

1. 修改 `AMyCharacter`、`UPlayerHUDWidget` 和 `ABow` 的 C++；不改任何 `.uasset`、输入、DataAsset、地图、弓 Mesh、投射物、箭数 UI 或动画。
2. 先执行轻量静态检查和定向 server-memory 查询；随后由用户手动编译 `TestEditor`，不运行 UBT、打包或自动 PIE。
3. PIE：装备现有 Debug Bow 后按住右键，确认中心准星显示；未装备、进入失败和右键松开时隐藏。
4. PIE：正常放箭、空 Loaded Arrow 和射击冷却期间准星保持；受击、Guard Break、死亡、火堆、换装、高优先级取消、HUD 重建和停止 PIE 后立即隐藏。
5. PIE：开启玩家 Debug、血条缓冲、药瓶冷却和受击红晕，并切换常规 / 宽屏窗口，确认原 HUD 不回归且准星始终居中。
6. 聚焦复验：不按 Alt 的瞄准前进速度与按 Alt 步行前进速度一致；锁定时瞄准继续使用现有前/侧/后速度倍率，瞄准不恢复冲刺或产生冲刺耗体。
7. 用户确认 PIE 后执行正常 review 与对抗性 review，重点检查单一状态来源、Paint 层级、无 Tick 轮询、HUD 重建同步、步行速度来源和射击语义隔离。

#### 文档与提交边界

- 通过验证和两轮 review 后，更新 `ARCHITECTURE.md` 的 HUD 表现边界，将 B1 移入 `ROADMAP.md` Done Milestones。`README.md` 默认不改。
- 提交仅包含 `AMyCharacter`、`UPlayerHUDWidget`、`ABow`、`plan.md`、`ROADMAP.md` 与 `ARCHITECTURE.md`。持续排除 `WBP_PlayerHUD.uasset`、弓/箭资产、DataAsset、地图、暂停菜单、覆盖确认和当前用户 WIP。

#### 当前执行记录

- 已核对：当前瞄准只由 `StartBowAimAction()` 进入、`CancelBowAim()` 退出；正式取消、受击、破防、死亡、火堆服务与主手换装均已收束到该边界或 HUD 拆除。
- 已完成：`AMyCharacter` 在瞄准进入、统一取消和 HUD 创建后，以 `IsBowAiming()` 推送准星可见性；`ReleaseBowArrow()` 未改，因而不改变现有持续瞄准语义。
- 已完成：`UPlayerHUDWidget::NativePaint()` 使用 `AllottedGeometry` 中心绘制四条空心十字线段；Debug 分支不再提前返回，准星绘制层位于受击红晕与调试文本之后。
- 已检查：`git diff --check` 通过；定向 server-memory 查询无既有错误模式；已按本机 UE 5.7 `FSlateDrawElement::MakeLines(TArray<FVector2f>)` 实际签名传入所有权明确的线段数组。
- 用户已确认准星和瞄准步行速度的 `TestEditor` 编译 / PIE 验收通过：瞄准状态下 Player Debug 为 `Speed: 200`，与默认 `WalkSpeed` 一致。
- 严格 review 已完成：常规检查确认瞄准状态来源、HUD 重建同步、Paint 层级、放箭后持续显示和锁定方向倍率保持隔离；对抗性检查确认受击 / Guard Break / 死亡 / 换装 / EndPlay 的统一取消没有旁路，瞄准不进入冲刺体力路径，且当前有效 Bow 默认值已由 PIE 证明为 `1.0`。没有剩余 B1 blocker。
