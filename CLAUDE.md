# CLAUDE.md

**Claude Code specific collaboration rules and workflow guidelines.**

For shared architectural knowledge (state machines, class hierarchy, combat pipeline, system boundaries), see **[ARCHITECTURE.md](ARCHITECTURE.md)**.

For general agent guidelines, see **[AGENTS.md](AGENTS.md)**.

---

## Build & Editor

- UE 5.7 project, Windows only, VS 2022 required.
- Generate VS project: right-click `Test.uproject` → **Generate Visual Studio project files**, then open `Test.sln`.
- Compile in IDE (Development Editor) or via UBT: `UnrealBuildTool TestEditor Win64 Development Test.uproject`
- Launch editor: open `Test.uproject` directly.
- Module: `Test` (Runtime), `SmartBPCreator` (Editor plugin). Targets: `TestEditor` (Editor), `Test` (Game).
- `Test.Build.cs` pulls in: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `Slate`, `SlateCore`.

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` are the primary truth source.
- When markdown documents disagree, use this precedence: **source code > ARCHITECTURE.md > AGENTS.md > CLAUDE.md > GEMINI.md**.
- **ARCHITECTURE.md** is the shared source of truth for all architectural knowledge. Read it before making structural changes.
- Verify behavior-critical claims against actual C++ before editing gameplay logic.

## Claude-Specific Workflow

### Content Organization
- C++ source under `Source/Test/` (Public/Private mirrors UE module structure).
- Game assets under `Content/_GAME/` — this is the only Content directory that should be modified.
- Large reference assets (AncientContent, AnimalVarietyPack, ParagonAurora, Mixamo, etc.) are ignored for AI context — they're read-only marketplace/paragon content.
- For code review and closeout passes, ignore tracked `.uasset` changes under `Content/` by default unless the user explicitly asks to inspect or include asset changes.

### Debug System

**架构概览** → 见 [ARCHITECTURE.md § Debug Output System](ARCHITECTURE.md#debug-output-system)

**Claude 协作规则：**

- **API 使用约定**：
  - UI checkbox 初始化必须读 **Raw Getter**（`GetPlayerEnabledRaw()`），显示子开关自身状态
  - 实际调试输出必须用 **Effective Getter**（`IsPlayerEnabled()`），尊重总开关压制
  - Setter 使用 `ECVF_SetByCode` 标记代码设置

- **守卫位置规则**：
  - ✅ 优先在函数入口：`if (!IsPlayerEnabled()) return;`
  - ✅ 次选 block 入口：`if (IsPlayerEnabled()) { ... }`
  - ❌ 避免每行判断：不要在每个 `Add()` 前重复塞判断

- **兼容策略**：
  - 旧 API（`IsShapesEnabled()`）用 `FORCEINLINE` 转发到新 API（`IsRangesEnabled()`）
  - 保留旧控件（`CB_DebugShapes`）和新控件（`CB_DebugRanges`）共存，支持渐进迁移
  - 新代码推荐使用 `Ranges` 命名

- **不做事项**：
  - 不要把 `FDebugDrawHelper::Add()` 改成带 category 参数的复杂接口
  - 不要让 `FDebugDrawHelper` 依赖 gameplay 类（`AEnemy`、`AMyCharacter`、战斗状态枚举）
  - 不要在 `FDebugDrawHelper` 中持有 gameplay 状态

### AI Collaboration (`plan.md`)
- 项目根目录 `plan.md` 用于多 AI agent 间通信（Claude Code、Codex 等），已 gitignore。
- 虚线上方为规则（不可修改），下方为反馈意见和计划。
- 计划完成后标 `✅ 已完成`，下次可清空重写。

### Documentation Update Policy
- Stable architecture facts → `ARCHITECTURE.md`
- Claude-specific workflow/rules → `CLAUDE.md`
- General agent rules → `AGENTS.md`
- User-facing project overview / feature highlights / setup → `README.md`
- Temporary plans, cross-agent discussion, and review feedback → `plan.md`
- Do not update every markdown file by default. Update only the document layer affected by the task, then let the review agent check for missing documentation.

### Implementation Notes
- **Dodge Roll System**: `SelectDodgeSection()` 必须在 `FaceDirection2D()` 之前调用，否则 `UnrotateVector()` 参考系错误
- **Combo System**: 续接判断必须在状态恢复之前，临时设 `ActionState = EAS_UnOccupied` 让 `CanAttack()` 通过
- **Charged Attack**: 蒙太奇契约：C++ 硬编码跳转 `Default` 和 `Release` Section



### Includes
- System/engine first, then project — forward declare when possible.
- `.generated.h` must be last include.
- Project includes use full relative path from `Public/`.

### UPROPERTY / UFUNCTION
- Components: `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` + `meta = (AllowPrivateAccess = "true")` when private.
- Editable config: `EditAnywhere` or `EditDefaultsOnly`。Runtime-only: `VisibleInstanceOnly`。
- ToolTip: `meta = (ToolTip = "...")` — 行注释只帮源码读者，ToolTip 帮编辑器用户。
- `AddDynamic(...)` 绑定的函数**必须**标记 `UFUNCTION()`，即使不暴露给蓝图。
- UMG 子控件用 `UPROPERTY(meta = (BindWidget))`；蓝图钩子用 `BlueprintImplementableEvent`。
- 不做全局 `TObjectPtr` 迁移。跟随文件风格：本项目大多用裸指针，`TObjectPtr` 仅少数位置（如 `AArenaGenerator`）。

### Encapsulation (封装性)
- **配置参数**：`EditAnywhere` + `private` + `meta = (AllowPrivateAccess = "true")` + `FORCEINLINE` getter。编辑器可见可编辑，代码只读访问。
- **运行时状态**：`VisibleInstanceOnly` + `private` + `meta = (AllowPrivateAccess = "true")` + getter。编辑器只读，代码通过 getter 访问。
- **Protected vs Private**：子类需要访问的成员（如 `PendingHitContext`、`ConsumePendingHitKnockback()`）保持 `protected`；纯内部实现细节（如 `bKnockbackActive`、`KnockbackDirection`）用 `private`。
- **受控接口**：容器类成员（`TArray`、`TMap`）用 `private` + 受控方法（如 `ClearIgnoreActors()`），防止外部直接 `Empty()`/`Add()` 绕过逻辑。
- **零开销 Getter**：所有 getter 用 `FORCEINLINE`，编译器内联展开，性能等同于直接访问。
- 实例：`AEnemy` 韧性系统、`AShield` 所有参数、`AWeapon::IgnoreActors`、`ABaseCharacter` 击退状态。

### Naming
| Type | Convention | Example |
|------|-----------|---------|
| Classes | `A`/`U` prefix, PascalCase | `AMyCharacter`, `UAttributeComponent` |
| Enums | `E` prefix, value prefix `ECS_`/`EAS_`/`EES_` | `EActionState::EAS_Attacking` |
| Interfaces | `I` prefix | `IHitInterface` |
| Members | PascalCase, no `m_` prefix | `OverLapItem` |
| Booleans | `b` prefix | `bIsSprinting` |
| Methods | PascalCase | `GetCharacterState()` |

### Comments
- 中文注释用于 gameplay 意图。
- 英文用于 API 文档和技术笔记。
- `/** */` 仅用于 UFUNCTION/UPROPERTY 文档。

### Spawn Actor Safety Pattern
所有 `SpawnActor` 调用后必须检查返回值是否为空：
- 配置类资产（`WeaponClass`、`ShieldClass`）可能配置错误或未设置
- Spawn 位置可能被阻挡（碰撞检测失败）
- 内存不足等运行时错误

**标准模式：**
```cpp
AActor* Actor = GetWorld()->SpawnActor<AActor>(ActorClass);
if (!Actor)
{
    UE_LOG(LogTemp, Warning, TEXT("%s failed to spawn actor from class %s"),
        *GetName(),
        *GetNameSafe(ActorClass));
    return;
}
// 使用 Actor
Actor->SomeMethod();
```

**关键点：**
- 使用 `GetNameSafe(ActorClass)` 防止类指针本身为空时的二次崩溃
- 日志包含调用者名称（`GetName()`）和类名，便于定位配置错误
- Early return 避免后续代码访问空指针

**实例：** `AEnemy::WeaponInit()` (Enemy.cpp:91-101)

## User Profile & Preferences (Updated 2026-04-27)

### 1. 人口统计信息
- **职业**: 虚幻引擎5.7和C++开发者。
- **定位**: United States (常住)。

### 2. 指令与交互风格
- **简洁**: 回答简明扼要，突出重点。
- **准确**: 不可模棱两可，基于源码事实。
- **身份对齐**: 基于“虚幻引擎5.7和C++开发者”身份交流。
