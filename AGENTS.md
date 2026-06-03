# AGENTS.md

This file provides guidance to coding agents working in this repository.

## Project Overview

- **UE 5.7** project, **Windows only**, **Visual Studio 2022** required.
- Modules: `Test` (Runtime), `SmartBPCreator` (Editor plugin).
- Targets: `TestEditor` (Editor), `Test` (Game).
- No automated tests exist in this project.

## Build & Editor

```bash
# Generate VS project files
# Right-click Test.uproject → "Generate Visual Studio project files" → open Test.sln

# Compile via UBT (Development Editor)
UnrealBuildTool TestEditor Win64 Development Test.uproject

# Compile via IDE
# Open Test.sln in VS 2022, build TestEditor target (Development Editor configuration)

# Launch editor
# Open Test.uproject directly
```

No CI pipeline, no lint/test commands configured.

- The user compiles manually. Do not run local build/compile commands unless the user explicitly asks for it.

## Module Dependencies

`Test.Build.cs` — `PublicDependencyModuleNames`:
`Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `SlateCore`

`PrivateDependencyModuleNames`:
`Slate`

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` files are the primary truth source.
- When repository markdown documents disagree, use this precedence: **source code > ARCHITECTURE.md > AGENTS.md > CLAUDE.md > GEMINI.md**.
- **ARCHITECTURE.md** is the shared architectural knowledge base for all agents. Read it before making structural changes.
- `AGENTS.md` and `CLAUDE.md` contain agent-specific collaboration rules; verify behavior-critical claims against actual C++ before editing gameplay logic.

## Agent Coordination

- `plan.md` is the shared agent-to-agent communication and handoff file.
- Before continuing prior collaboration work, read the header rules in `plan.md`.
- Write agent feedback and implementation plans there as working context, not end-user documentation.
- For code review and closeout passes, ignore `Content/*.uasset` by default unless the user explicitly asks to inspect or include asset changes.

## Documentation Update Policy

- Each agent updates only the markdown files within its responsibility.
- Stable architecture facts go in `ARCHITECTURE.md`.
- Agent behavior and workflow rules go in the relevant agent file, such as `AGENTS.md`, `CLAUDE.md`, or `GEMINI.md`.
- User-facing project overview, feature highlights, and setup notes go in `README.md`.
- Temporary plans, cross-agent discussion, implementation proposals, and review feedback go in `plan.md`.
- After a task is complete, the review agent checks whether documentation needs to be supplemented.
- Do not sync every markdown file by default; update only the document layer affected by the task to avoid duplication, conflicts, and doc drift.

## Working Rules

### Communication
- 默认中文回答，除非用户明确要求英文。
- 直接回答问题，不用"好问题"、"很高兴为您服务"等客套开场。

### Code Investigation
- 代码库任务先读真实文件，再下结论。不要凭记忆改关键逻辑。
- 用户已有改动默认保留，除非用户明确要求回滚。

### Change Authorization
- 未明确授权时不做大范围重构、删除、回滚。
- 文件修改前先简短说明要改什么（一句话即可）。
- 停下来比猜错强。不确定就问，有多种解读就都摆出来。

### Code Review Protocol
- **两轮 review 必须**：正常 review + 对抗性 review
- **第一轮（正常 review）**：
  - 优先找 bug、行为回归、风险和缺测试
  - 按严重程度排序（P0/P1/P2）
  - 给文件/行号依据
- **第二轮（对抗性 review）**：
  - 假设代码是自己写的，先为关键设计辩护
  - 辩护中发现站不住脚的地方就诚实标成风险
  - 给改进建议
  - **不是洗地**：能辩护才辩护，不能就直说

### Sub-Agent Strategy
- **轻量子代理（mini 模型）适用场景**：低风险只读任务、机械文档任务、批量搜索
- **不要交给 mini**：架构决策、关键 review、行为敏感逻辑、设计方案制定

## Architecture

**For all architectural knowledge (state machines, class hierarchy, combat pipeline, system boundaries), see [ARCHITECTURE.md](ARCHITECTURE.md).**

This is the shared source of truth for Codex, Claude, Gemini, and all other agents. Read it before making structural changes.

## Code Conventions

### Includes

```cpp
// System/engine first, then project — forward declare when possible
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CharacterTypes.h"   // project includes use full relative path from Public/
#include "MyCharacter.generated.h"        // .generated.h must be last include

class UAttributeComponent;               // forward declare in headers, include in .cpp
class AWeapon;
```

### UPROPERTY / UFUNCTION

- Components: `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` with `meta = (AllowPrivateAccess = "true")` when private.
- Editable config: `EditAnywhere` or `EditDefaultsOnly`.
- Runtime-only props: `VisibleInstanceOnly`.
- If the goal is a readable tooltip in Blueprint / Details panels, use `meta = (ToolTip = "...")`; line comments only help source readers.
- Functions exposed to BP: use `BlueprintCallable` or `BlueprintNativeEvent`.
- Interface functions: use `BlueprintNativeEvent` + `_Implementation` suffix.
- Any function bound through `AddDynamic(...)` must be marked with `UFUNCTION()`, even if it is not exposed to Blueprint. Current examples include `HandleExhausted`, `TargetPerceptionUpdated`, `SphereOverlap`, `SphereEndOverlap`, and HUD health/stamina update callbacks.
- UMG child widgets are bound with `UPROPERTY(meta = (BindWidget))`; Blueprint-owned presentation hooks use `BlueprintImplementableEvent` (for example `UBaseHealthBarWidget::PlayFadeOutAnim()`).
- Do not assume a repo-wide `TObjectPtr` migration. Follow the surrounding file style: this codebase mostly uses reflected raw pointers, with `TObjectPtr` appearing only in a few spots such as `AArenaGenerator`.

### Encapsulation

- Editable config that external code only reads should be `private` `UPROPERTY(EditAnywhere/EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))` plus a `FORCEINLINE` getter.
- Runtime state that should be visible but not externally mutated should be `private` `UPROPERTY(VisibleInstanceOnly, meta = (AllowPrivateAccess = "true"))` plus a getter.
- Keep members `protected` only when subclasses genuinely need direct access; current examples include `PendingHitContext` and `ConsumePendingHitKnockback()`.
- Container members such as `TArray` / `TMap` should stay private behind controlled methods when callers only need a narrow operation. Current example: `AWeapon::IgnoreActors` is cleared through `ClearIgnoreActors()`.
- Recent examples to follow: `AEnemy` poise parameters, `AShield` block/parry/equip parameters, `AWeapon::IgnoreActors`, and `ABaseCharacter` internal knockback state.

### Naming

| Type | Convention | Example |
|------|-----------|---------|
| Classes | `A`/`U` prefix, PascalCase | `AMyCharacter`, `UAttributeComponent` |
| Enums | `E` prefix, `ECS_`/`EAS_`/`EES_` prefixes for values | `EActionState::EAS_Attacking` |
| Interfaces | `I` prefix, PascalCase | `IHitInterface`, `IPickupInterface` |
| Members | PascalCase, no `m_` or `_` prefix | `OverLapItem`, `ActionState` |
| Booleans | `b` prefix | `bIsSprinting` |
| Methods | PascalCase | `GetCharacterState()`, `PlayAttackMontage()` |

### Accessors

Use `FORCEINLINE` inline in headers:

```cpp
FORCEINLINE EWeaponState GetCharacterState() const { return CharacterState; }
FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
```

### Virtual Functions

Override lifecycle functions (`BeginPlay`, `Tick`) and interface implementations in subclasses. Always call `Super::` in overrides unless explicitly avoiding it.

### Error Handling

Use `check()` / `ensure()` for invariants. No `try/catch` unless interacting with third-party code that throws.

### Comments

- Comments in **Chinese** for gameplay intent (current convention in codebase).
- English is acceptable for API docs and technical notes.
- Use `/** */` only when documenting UFUNCTION/UPROPERTY.

## User Profile

- UE 5.7 C++ developer, based in the United States.
- Prefers **concise, accurate** answers grounded in source code facts.
- Chinese is acceptable for general communication; code and technical terms remain in English.
