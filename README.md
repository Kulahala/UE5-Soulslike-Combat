# UE5-Soulslike-Combat

<p align="center">
  <img src="https://img.shields.io/badge/Unreal_Engine-5.7-black?style=for-the-badge&logo=unrealengine" alt="Unreal Engine 5.7" />
  <img src="https://img.shields.io/badge/Language-C++20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20" />
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?style=for-the-badge&logo=windows" alt="Windows" />
  <img src="https://img.shields.io/badge/IDE-VS_2022-5C2D91?style=for-the-badge&logo=visualstudio" alt="Visual Studio 2022" />
</p>

<p align="center">
  <a href="#english">English</a> • <a href="#中文">中文说明</a> • <a href="ARCHITECTURE.md">Architecture (架构文档)</a>
</p>

---

<a name="english"></a>
## English

An Unreal Engine 5.7 C++ Action-RPG prototype focused on responsive character control, state-driven melee combat, lock-on movement, shield defense, enemy combat AI, and data-driven attack configuration.

This README is a project overview. For implementation details, state machines, combat pipeline, class boundaries, and agent-facing architecture notes, see [ARCHITECTURE.md](ARCHITECTURE.md).

![Gameplay parry and stance break demo](https://github.com/user-attachments/assets/d548f406-fc57-4165-aace-720e091b6b97)

## 📺 QuickDemo

🎬 **Bilibili Gameplay Video (1080P 60FPS):** [https://www.bilibili.com/video/BV1d3Mm6UEqv/](https://www.bilibili.com/video/BV1d3Mm6UEqv/)

### ✨ Highlights

- ⚔️ **State-Driven Player Combat**: Light combos, sprint attack, charged attack, dodge, parry, block, potion use, stamina exhaustion, hit stun, and death all flow through explicit action-state guards.
- 🔥 **Bonfire Services, Loadout & Checkpoints**: Independent main menu (New Game, Continue, Settings, Quit); New Game creates writable progress without a spawn anchor. Death before first bonfire returns to `PlayerStart` while Gold and claimed fixed items persist. Bonfire offers real-time (non-pausing) Rest, Equipment, and Leave services. Owned weapons and shields equip immediately and persist across death, rest, and Continue.
- 📊 **Data-Driven Attack Configuration**: `UAttackConfigDataAsset` manages player combo chains, sprint attacks, and charged attacks; `UEnemyAttackConfigDataAsset` manages enemy movesets with per-attack Motion Warping support.
- 🏹 **Ranged Combat & Transient Rewards**: Player Bow supports prepared arrows and projectile release from `BowArrowSocket`; configured ranged enemies use projectile attacks; defeated enemies drop transient, interactable Gold light orbs that persist on pickup.
- 🎯 **Precise Weapon Collision**: Box swept traces between previous and current frames to prevent hit misses during high-speed swings.
- 🛡️ **Directional Shield Block & Timed Parry**: Block checks angle and stamina before damage application; successful parries instantly drain enemy poise and trigger stance break.
- 💥 **Poise & Stance Break**: Hidden poise system for enemies; standard attacks chip poise while timed parries instantly trigger full-body stance break stun.
- 🔒 **Lock-On & Camera Framing**: Target screening, centered elevated framing, strafing, and lock-on sprint free-run.
- 🧠 **Hierarchical Enemy Combat AI**: Outer FSM (Patrol, Search, Chase, Combat, Stun, Dead) + inner local HFSM managing facing, pressing, waiting, and spacing.
- 🏃 **Motion-Warped Leap Attacks**: Enemy leap attacks write fixed per-attack WarpTargets to track the player dynamically without unnatural homing.
- 🤝 **Attack Coordination**: Multiple nearby enemies chasing the same target coordinate attacks via a dedicated waiting substate to avoid simultaneous spamming.
- 🛡️ **Hyper Armor**: AnimNotify-driven hyper armor grants uninterruptible poise during heavy swings while remaining vulnerable to parry and stance break.
- 🩸 **Readable Combat Feedback**: Delayed buffer health bars, directional hit knockback, camera shake, and low-health vignette.
- 🛠️ **In-Game Debug Menu**: Pause menu includes Debug Settings page to toggle state machines, hit trace boxes, and HUD debug overlays in real time.
- ⚡ **Full UE5 Ecosystem**: Integrates Enhanced Input, UMG HUD, Niagara VFX, Chaos Destruction, PCG Arena Generation, and NavMesh AI.

### 🎮 Gameplay Systems Overview

| Area | Icon | Summary |
|------|:----:|---------|
| **Player Character** | 🤺 | Movement, lock-on, stamina, combo attacks, dodge, block, parry, potion, hit reacts, death |
| **Game Flow & Save** | 💾 | Main menu, single-slot SaveGame, bonfire interaction & loadout, death overlay, map reload |
| **Combat Data** | 📊 | Combo/special/charged attack configs, enemy movesets, damage/block/poise multipliers, Motion Warping |
| **Weapons & Collision**| ⚔️ | Swept box trace, team filtering, block interception, hit feedback dispatch |
| **Ranged & Rewards** | 🏹 | Prepared bow arrows, `BowArrowSocket` projectile launch, transient interactable Gold orbs |
| **Enemy AI** | 🧠 | Patrol, search, chase, combat local HFSM, cooldown spacing, attack coordination |
| **Defense & Poise** | 🛡️ | Directional blocking, timed parry, stamina consumption, stance break, hyper armor |
| **Combat Feedback** | 💥 | Health bars, delayed buffer bars, hit knockback, camera shake, damage vignette |
| **Debug & UI** | 🛠️ | Pause menu, debug settings page, HUD debug text, real-time collision trace visualization |
| **Environment** | 🏛️ | Breakables with Chaos physics and PCG-assisted arena generation |

### 🎬 Demo Clips

![Combat finish and kill feedback](https://github.com/user-attachments/assets/0f34763d-664d-47a9-bdf8-d3a5906f2370)

### 🧩 Extension Points

- **Enemy Variants**: Enemy behavior is decoupled into runtime FSM logic and `UEnemyAttackConfigDataAsset`. New enemy types reuse patrol/chase/combat flow while swapping attack montages, distances, weights, cooldowns, parry rules, and Motion Warping parameters.
- **Weapon Variants**: `AWeapon` standardizes swept box traces, ignore lists, faction filtering, block interception, and hit feedback. New weapon types only require socket and DataAsset tuning without rewriting hit resolution.
- **Playable Character Profiles**: `UPlayerCharacterProfileDataAsset` is the single configuration entry for player Blueprints, referencing attack, action, and reaction config assets.
- **DataAsset Reusability**: Combo chains, special attacks, charged attacks, enemy attack sets, player action montages, and hit reactions are isolated into DataAssets.
- **Future Scaling**: Easily extensible toward weapon-specific movesets, enemy archetype registries, richer reaction tables, or GAS migration.

### 📖 Architecture Deep Dives

- [Player State Machine Flow](ARCHITECTURE.md#player-state-machine-flow)
- [Enemy State Machine Flow](ARCHITECTURE.md#enemy-state-machine-flow)
- [Combat Pipeline](ARCHITECTURE.md#combat-pipeline)
- [Player Action Recovery Helpers](ARCHITECTURE.md#player-action-recovery-helpers)
- [Enemy AI & Combat Local HFSM](ARCHITECTURE.md#enemy-ai)
- [Enemy Attack Motion Warping](ARCHITECTURE.md#enemy-attack-motion-warping)
- [Combat Cooldown & Coordination Flow](ARCHITECTURE.md#combat-cooldown-coordination-flow)
- [Shield Blocking System](ARCHITECTURE.md#shield-blocking-system)
- [Parry System](ARCHITECTURE.md#parry-system)
- [Poise & Stance Break System](ARCHITECTURE.md#poise-stance-break-system)
- [Combo System](ARCHITECTURE.md#combo-system)
- [Charged Attack System](ARCHITECTURE.md#charged-attack-system)
- [Dodge Roll System](ARCHITECTURE.md#dodge-roll-system)
- [World Interaction & Breakables](ARCHITECTURE.md#world-interaction-breakable-system)
- [Lock-On System](ARCHITECTURE.md#lock-on-system)
- [Hyper Armor System](ARCHITECTURE.md#hyper-armor-system)
- [Debug Output System](ARCHITECTURE.md#debug-output-system)

### 📚 Repository Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md): Shared implementation architecture, state flows, combat pipeline, and system boundaries.
- [Test_Demo_Recording_Script.md](docs/Test_Demo_Recording_Script.md): Gameplay-focused demo recording script, pause points, and asset checklist.
- [Test_Demo_Voiceover.md](docs/Test_Demo_Voiceover.md): Voiceover script mapped to demo pause points.
- [AGENTS.md](AGENTS.md): Repository-wide rules and guidelines for AI agents.
- [CLAUDE.md](CLAUDE.md): Claude-specific collaboration notes.

### 💻 Requirements

- Unreal Engine 5.7+
- Windows 10 / 11
- Visual Studio 2022 (with C++ Game Development workload)

### 🚀 Getting Started

1. Clone the repository into your Unreal projects folder.
2. Right-click `Test.uproject` and choose **Generate Visual Studio project files**.
3. Open `Test.sln`.
4. Build the `TestEditor` target in **Development Editor** configuration.
5. Open `Test.uproject` to launch the editor.

---

<a name="中文"></a>
## 中文

这是一个基于 **Unreal Engine 5.7** 和 **C++20** 构建的高响应性魂系近战动作游戏原型，核心涵盖：精准动作状态机、魂系八向锁定、盾牌防御与时机弹反、隐藏韧性破防、分层敌人战斗 AI，以及全数据驱动（DataAsset）的招式与配置管线。

本文档只作为项目首页概览。关于底层实现细节、状态机拓扑、战斗判定管线、类职责边界及架构说明，请查阅 [ARCHITECTURE.md](ARCHITECTURE.md)。

![玩法弹反与破防演示](https://github.com/user-attachments/assets/d548f406-fc57-4165-aace-720e091b6b97)

## 📺 演示视频

🎬 **Bilibili 在线高清实机录屏（1080P 60FPS）：** [https://www.bilibili.com/video/BV1d3Mm6UEqv/](https://www.bilibili.com/video/BV1d3Mm6UEqv/)

### ✨ 核心亮点

- ⚔️ **状态驱动的主角战斗**：轻攻击 3 连击、冲刺攻击、蓄力攻击、8 向翻滚、格挡、弹反、喝药、体力透支、受击硬直与死亡，均通过严谨的动作状态守卫（Action State Guards）调度。
- 🔥 **火堆服务与重生锚点**：独立主菜单提供 New Game、Continue、Settings 和 Quit；首次坐火激活重生锚点并重载地图；后续使用火堆打开不暂停世界的“休息 / 装备 / 离开”实时服务菜单。已拥有装备支持即时换装并跨死亡/Continue 保持。
- 📊 **数据驱动攻击配置**：`UAttackConfigDataAsset` 管理主角连招、冲刺跳劈与蓄力段；`UEnemyAttackConfigDataAsset` 管理敌人招式条目，支持独立配置跳劈 Motion Warping。
- 🏹 **远程战斗与掉落物拾取**：主角持弓支持待发箭生成与 `BowArrowSocket` 投射物发射；敌人死亡生成可拾取的 Gold 临时金币光团，拾取成功后即时落盘持久化。
- 🎯 **精确武器盒体扫掠**：武器采用前一帧至当前帧的盒体扫掠（Swept Box Trace），杜绝高速出招动画中的漏判。
- 🛡️ **盾牌格挡与时机弹反**：伤害结算前先校验防御角度与体力；精准弹反瞬间清空攻击方韧性并触发失衡大硬直。
- 💥 **隐藏韧性与破防系统**：敌人内置隐藏韧性条，常规命中逐步削韧，弹反可直接破防；受击方进入专属全身失衡蒙太奇，为玩家创造处决窗口。
- 🔒 **魂系锁定移动与相机**：目标筛选评分、居中后上方俯仰构图、锁定绕行与冲刺 Free-Run、滚轮屏幕侧目标切换。
- 🧠 **分层敌人战斗 AI**：外层 FSM（巡逻、搜索、追击、战斗、硬直、死亡）+ 内部局部 HFSM（面朝、逼近、拉扯、出招、后退），行为自然多变。
- 🏃 **跳劈 Motion Warping**：指定敌人招式写入一次固定 WarpTarget，跳跃/前突攻击向玩家动态修正，避免僵硬的直线位移或非自然的追踪。
- 🤝 **敌人攻击协调机制**：追击同一目标的多个附近敌人不会同时出手，未获得攻击权者进入协调等待子状态，避免玩家遭遇不可规避的群殴压制。
- 🛡️ **绝对霸体系统**：动画通知驱动的霸体机制，允许角色在重攻击特定阶段硬扛常规攻击，但仍受破防与弹反机制制约。
- 🩸 **清晰战斗反馈**：血条缓冲延迟递减、受击方向性短退、相机震屏、玩家低血量红晕。
- 🛠️ **游戏内实时调试菜单**：暂停菜单集成 Debug Settings 子页，可一键开关项目调试面板、状态机文本、碰撞盒 Trace 输出与感知范围。
- ⚡ **UE 深度技术整合**：全方位使用 Enhanced Input、UMG HUD、Niagara 粒子、Chaos 物理破碎、PCG 竞技场生成与 AI 导航。

### 🎮 玩法系统概览

| 模块 | 图标 | 简述 |
|------|:----:|------|
| **主角控制** | 🤺 | 8向移动、锁定、体力管理、攻击连招、翻滚无敌帧、格挡、弹反、喝药、受击与死亡 |
| **流程与存档** | 💾 | 主菜单、单槽 SaveGame、火堆交互与即时换装、死亡重载、进度恢复 |
| **战斗数据资产** | 📊 | 连招链/特殊/蓄力攻击配置、敌人招式表、伤害/格挡/韧性倍率、Motion Warping 资产 |
| **武器与判定** | ⚔️ | 盒体扫掠、同阵营过滤、盾牌拦截、命中反馈派发 |
| **远程与奖励** | 🏹 | 弓箭待发箭、`BowArrowSocket` 投射物起点、敌人 Gold 光团与世界交互拾取 |
| **敌人 AI** | 🧠 | 巡逻、警戒搜索、追击、战斗局部 HFSM、拉扯站位、防群殴攻击协调 |
| **防御与霸体** | 🛡️ | 方向性格挡、时机弹反、体力消耗、破防处决硬直、出招霸体 |
| **受击反馈** | 💥 | 延迟缓冲血条、受击击退、相机晃动、伤害红晕 |
| **调试与 UI** | 🛠️ | 暂停菜单、调试设置页、HUD 实时状态调试文本、碰撞盒可视化开关 |
| **环境互动** | 🏛️ | Chaos 几何破碎可破坏物、PCG 辅助竞技场生成 |

### 🎬 演示片段

![战斗收尾与击杀反馈](https://github.com/user-attachments/assets/0f34763d-664d-47a9-bdf8-d3a5906f2370)

### 🧩 扩展性设计

- **敌人种类扩展**：敌人行为拆为运行时 FSM 逻辑和 `UEnemyAttackConfigDataAsset` 招式配置。新增敌人复用巡逻/追击/战斗流程，仅需替换攻击蒙太奇、距离、权重、冷却与 Motion Warping 参数。
- **武器种类扩展**：`AWeapon` 统一处理盒体扫掠、忽略列表、阵营过滤、格挡拦截、命中反馈、基础伤害与韧性伤害。新增武器仅需配置挂点、碰撞尺寸与数值资产。
- **可玩角色配置扩展**：`UPlayerCharacterProfileDataAsset` 作为主角单一配置入口，聚合攻击、动作和受击反应资产，支持多角色资产解耦与复用。
- **DataAsset 模块化**：连招链、特殊招式、蓄力重击、敌人招式库、主角动作和受击反应全模块化隔离。C++ 负责规则与状态流转，DataAsset 负责参数与动画引用。
- **后续演进路径**：可无缝扩展武器专属招式集、敌人 Archetype 预设库，或在技能复杂度提升后平滑迁移至 GAS（Gameplay Ability System）。

### 📖 架构深度解析入口

- [主角状态流转](ARCHITECTURE.md#player-state-machine-flow)
- [敌人状态流转](ARCHITECTURE.md#enemy-state-machine-flow)
- [战斗管线](ARCHITECTURE.md#combat-pipeline)
- [主角动作恢复 Helper](ARCHITECTURE.md#player-action-recovery-helpers)
- [敌人 AI 与 Combat 局部 HFSM](ARCHITECTURE.md#enemy-ai)
- [敌人跳劈 Motion Warping](ARCHITECTURE.md#enemy-attack-motion-warping)
- [战斗冷却与攻击协调流程](ARCHITECTURE.md#combat-cooldown-coordination-flow)
- [盾牌格挡系统](ARCHITECTURE.md#shield-blocking-system)
- [弹反系统](ARCHITECTURE.md#parry-system)
- [韧性与破防系统](ARCHITECTURE.md#poise-stance-break-system)
- [连招系统](ARCHITECTURE.md#combo-system)
- [蓄力攻击系统](ARCHITECTURE.md#charged-attack-system)
- [翻滚系统](ARCHITECTURE.md#dodge-roll-system)
- [环境交互与破坏物系统](ARCHITECTURE.md#world-interaction-breakable-system)
- [锁定系统](ARCHITECTURE.md#lock-on-system)
- [霸体系统](ARCHITECTURE.md#hyper-armor-system)
- [调试输出系统](ARCHITECTURE.md#debug-output-system)

### 📚 当前文档职责

- [ARCHITECTURE.md](ARCHITECTURE.md)：共享实现架构、状态流转、战斗管线和系统边界。
- [Test_Demo_Recording_Script.md](docs/Test_Demo_Recording_Script.md)：按游戏行为组织的演示顺序、暂停点和资产展示清单。
- [Test_Demo_Voiceover.md](docs/Test_Demo_Voiceover.md)：与演示暂停点对应的后期配音文本。
- [AGENTS.md](AGENTS.md)：本仓库通用 agent 工作规则。
- [CLAUDE.md](CLAUDE.md)：Claude 专用协作说明。

### 💻 构建要求

- Unreal Engine 5.7+
- Windows 10 / 11
- Visual Studio 2022（需安装 C++ 游戏开发工作负载）

### 🚀 快速开始

1. 将仓库克隆到 Unreal 项目目录。
2. 右键 `Test.uproject`，选择 **Generate Visual Studio project files**。
3. 打开 `Test.sln`。
4. 使用 **Development Editor** 配置编译 `TestEditor` 目标。
5. 打开 `Test.uproject` 启动编辑器。
