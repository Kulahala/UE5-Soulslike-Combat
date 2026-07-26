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

# Maintenance Cleanup v1

## 范围与执行顺序

1. 删除无调用、非反射的 `UItemOwnershipComponent::TryConsumeDefinitionQuantity(...)`、其唯一调用的 `USoulslikeGameInstance::ConsumeOwnedItemQuantity(...)` 与 `ABaseCharacter::PlayAttackMontage(...)`，不添加替代入口。
2. 修正遗留 SunTemple 项目元数据与不存在的服务端默认地图，不改变主菜单、编辑器启动地图、ProjectID、BuildTarget 或渲染/打包配置。
3. 将 README 的英中文档补齐已完成的 Bow、远程敌人与敌人 Gold 临时奖励事实；收束 ARCHITECTURE 的旧攻击蒙太奇说明与 ROADMAP 的已失效风险。
4. 在 ROADMAP 仅记录后续 `TODO-05C1: Combo Start Effect Dedup v1`，本阶段不改连招运行时或资产。

## 验证

- CodeGraph 与全仓精确搜索确认三条删除符号不存在调用、override 或 Blueprint 入口；核对目标地图包和最终 ini 值。
- 用户手动编译 `TestEditor`，在 Editor 检查项目元数据和 Game Maps 无缺失地图警告。
- 用户在 PIE 从 MainMenu 进入 TestMap，并回归基础剑、盾、弓交互。
- 用户验证后执行正常 review 与新的 600 秒证据预算对抗性 review；文档收尾和提交须等待明确批准。

## 提交边界

仅包含必要的 Character / ItemOwnership / GameInstance C++、`DefaultEngine.ini`、`DefaultGame.ini`、`README.md`、`ARCHITECTURE.md`、`ROADMAP.md` 与本文件。持续排除全部资产、HUD、Bow/Potion/Dodge/Combo WIP、TestMap External Actors、Marketplace 内容、`AGENTS.md`、`CLAUDE.md` 与 Boss 路线 WIP。

## 结果

- 三条无调用、非反射的旧 API 已删除；CodeGraph 与全仓精确搜索未发现剩余调用、override 或 Blueprint 入口。
- `SunTemple` 元数据和不存在的服务端地图已收束到当前 `Soulslike Combat` / `TestMap` 合同；静态地图包与 ini 检查通过。
- 用户已手动编译 `TestEditor`，并在 PIE 从 MainMenu 进入 TestMap 回归基础剑、盾、弓交互。
- 主线程正常 review 与新的独立 600 秒对抗性 review 均无 blocker。独立 review 未单独启动 dedicated-server 流程，也未重新执行 Bow/远程敌人/Gold 的专项资产验证；这些不属于本阶段运行时改动。
